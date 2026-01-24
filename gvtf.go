package main

import (
	"fmt"
	"math"
	"flag"
	"io"
	"os"
	"os/user"
	"bufio"
	"time"
	"sort"
	"strings"
	"strconv"
	"math/big"
	"encoding/json"
)

//#cgo LDFLAGS: -L. -lvulkan
//#include "tf.h"
import "C"

//
// remove composite factors from the list.  return a list of factors as strings.
//
func removecomp(factors []*big.Int) []string {
	out := make([]string, 0, len(factors))

	Z := big.NewInt(0)
	for i, f := range factors {

		if ! f.ProbablyPrime(10) {
			fmt.Fprintf(os.Stdout, "# %d is composite\n", f)
		}
		// don't report factors that have factors we're already reporting
		comp := false
		for j := 0; j < i; j++ {
			g := factors[j]
			M := new(big.Int)
			M.Mod(f, g)
			if M.Cmp(Z) == 0 {
				comp = true
			}
		}
		if !comp {
			out = append(out, f.String())
		} else {
			//fmt.Printf("removing: %d\n", f)
		}
	}
	return out
}

type Result struct {
	//timestamp, exponent, worktype, status, bitlo, bithi(, begink, endk), rangecomplete, factors, program, user, computer
	Timestamp  string     `json:"timestamp"`
	Exponent   uint64     `json:"exponent"`
	Worktype   string     `json:"worktype"`
	Status     string     `json:"status"`
	Bitlo      int64      `json:"bitlo"`
	Bithi      int64      `json:"bithi"`
	Begink     *big.Int   `json:"begink"`
	Endk       *big.Int   `json:"endk"`
	Rangec     bool       `json:"rangecomplete"`
	Factors    []string   `json:"factors,omitempty"`
	Program    map[string]string  `json:"program,omitempty"`
	User       string     `json:"user"`
	Computer   string     `json:"computer,omitempty"`

	// not written.
	kfactors   []*big.Int
	krestart   *big.Int
}


func (r *Result) checkpoint(k *big.Int) {
	r.krestart.Set(k)
}

func (r *Result) checkpointLoop(filename string, done chan struct{}) {
	ch := make(chan struct{})
	go func() {
		for {
			time.Sleep(30 * time.Second)
			ch <-struct{}{}
		}
	}()
	One := big.NewInt(1)
	for {
		select {
		case <-done:
			//fmt.Printf("# checkpointLoop(): done\n")
			return
		case <-ch:
			if r.krestart.Cmp(One) > 0 {
				x := map[string]interface{}{"K":r.krestart, "Kfactors":r.kfactors}
				j, _ := json.Marshal(x)

				f, err := os.OpenFile(filename, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0644)
				if err == nil {
					f.Write(j)
					f.Close()
				}
			}
		}
	}
}

func (r *Result) readcheckpoint(filename string) {
	f, err := os.Open(filename)
	if err == nil {
		data, err := io.ReadAll(f)
		if err == nil {
			x := struct{
				K *big.Int
				Kfactors []*big.Int}{}
			err = json.Unmarshal(data, &x)
			if err == nil {
				fmt.Printf("# readcheckpoint(): starting with %s\n", data)
				r.krestart = x.K
				r.kfactors = x.Kfactors
			} else {
				fmt.Printf("rcp: %s\n", err)
			}
		}
	}
}

var useDouble bool

func initInput(P uint64) int {

	p := (*C.struct_Stuff)(C.mrhGetMap())
	
	p.P = C.uint64_t(P)
	p.UseDouble = 0
	if useDouble {
		p.UseDouble = 1
	}

	p.Init = 13  // init atomics
	C.runCommandBuffer()
	p.Init = 3  // init bit arrays
	C.runCommandBuffer()
	//fmt.Printf("---- init3 done = %d\n", p.L)

	p.Init = 11  // init atomics
	C.runCommandBuffer()
	p.Init = 1   // first sieve
	C.runCommandBuffer()
	fmt.Printf("# GPU threads used for first sieve: %d\n", p.Debug[2])

	p.Init = 5;  // copy back atomic counters, to check if it went correctly
	C.runCommandBuffer()

	if p.Debug[0] != C.ListLen {
		fmt.Fprintf(os.Stdout, "# Something went wrong during init on the GPU: P.Ll %d != ListLen %d\n", p.Debug[0], C.ListLen)
		return -1;
	}
	//fmt.Printf("debug: %d %d\n", p.Debug[0], p.Debug[1])

	C.mrhUnMap()
	return 0;
}

func u64n(N *big.Int, pos uint) uint64 {
	one := big.NewInt(1)
	m64 := big.NewInt(1)
	m64.Lsh(m64, 64).Sub(m64, one)

	R := new(big.Int)
	R.Rsh(N, pos * 64).And(R, m64)
	return R.Uint64()
}
func big2(u0, u1 C.uint64_t) *big.Int {
	f := new(big.Int)
	f.SetUint64(uint64(u1))
	f.Lsh(f, 64)
	f1 := new(big.Int)
	f1.SetUint64(uint64(u0))
	f.Or(f, f1)
	return f
}

// Estimate the number of Ghz-Days required for a range of factors
func estGhzDays(K *big.Int) (ghzdays float64) {
	ghzdays, _ = K.Float64()
	ghzdays /= 2465155716822.9
	return
}

func timeremaining(K, K2, LastK *big.Int, e time.Duration) (time.Duration, float64) {
	L := new(big.Int)
	L.Sub(K2, K)

	// not exactly right, but close enough
	ghzdays := estGhzDays(L)

	E := big.NewInt(int64(e.Seconds()))
	R := new(big.Int).Sub(K, LastK)
	R.Div(R, E)

	L.Div(L, R)
	d := L.Int64()
	return time.Duration(d) * time.Second, ghzdays
}


func (result *Result) tfRun() {
	p := (*C.struct_Stuff)(C.mrhGetMap());
	K1 := result.Begink
	K2 := result.Endk

	//K1 = C.M * (K1/C.M);
	M := big.NewInt(C.M)
	K := new(big.Int).Set(K1)

	if K.Cmp(result.krestart) < 0 {
		K.Set(result.krestart)
	}

	// start on a M boundry
	K.Div(K, M)
	K.Mul(K, M)

	// fill in the 128-bit starting K value
	p.K[0] = C.uint64_t(u64n(K, 0))
	p.K[1] = C.uint64_t(u64n(K, 1))

	// do second sieve.
	startt := time.Now()
	p.Init = 12    // init atomic counters
	C.runCommandBuffer()
	p.Init = 2     // second seive
	C.runCommandBuffer()
	p.Init = 5;  // copy L2 back, for sanity checking
	C.runCommandBuffer()
	elapsed := time.Now().Sub(startt)
	first := uint(p.Debug[0])
	second := uint(p.Debug[1])
	rat := float64(second) / float64(C.M)
	
	fmt.Printf("# block: %d, first sieve: %d, second sieve: %d (%0.2f%%) (%s)\n", M, first, second, rat*100, elapsed)

	if result.kfactors == nil {
		result.kfactors = make([]*big.Int, 0, 10)
	}
	done := false
	count := int64(0)
	LastK := new(big.Int)
	LastK.Set(K)
	startt = time.Now()
	for {
		p.Big = 0;
		bits := ktobit(result.Exponent, K)
		if (bits >= 95) {
			// let the GPU code know to switch from 96 to 128-bit code
			p.Big = 1;
		}

		p.Init = 10;
		C.runCommandBuffer() // 10 - init atomics
		p.Init = 0;
		C.runCommandBuffer() // 0 - run TF

		count++
		if elapsed := time.Now().Sub(startt);  elapsed.Seconds() > 30 {
			L := new(big.Int)
			L.Sub(K, LastK)
			remain, g := timeremaining(K, K2, LastK, elapsed)
			percall := float64(elapsed.Milliseconds()) / float64(count)
			fmt.Fprintf(os.Stdout, "# K: %d/%d, ms/call: %.1f, %.1f ghz-d/d, remaining: %s %.1f ghzdays\n",
				K, K2, percall, 24.0*estGhzDays(L)/elapsed.Hours(), remain, g)
			startt = time.Now()
			count = 0
			LastK.Set(K)
		}

		if p.Debug[3] > 0 {
			fmt.Fprintf(os.Stderr, "# Overflow in GPU code detected %d\n", count)
		}
		for i := 0; i < int(p.NFound); i++ {
			f := big2(p.Found[i][0], p.Found[i][1])
			result.kfactors = append(result.kfactors, f)

			f64, _ := f.Float64()
			flb2 := math.Log2(f64 * float64(result.Exponent) * 2.0)
			fmt.Fprintf(os.Stdout, "# %d kfactor %s %.2f %d\n",
					result.Exponent, f, flb2, count);
		}

		if K.Cmp(K2) > 0 {
			done = true
		}
		
		if (done) {
			result.Begink = K1
			result.Endk = K
			break;
		}
		
		result.checkpoint(K)

		// advance to the next chunk to test.
		K.Add(K, M)
		p.K[0] = C.uint64_t(u64n(K, 0))
		p.K[1] = C.uint64_t(u64n(K, 1))

		// re-run second sieve
		p.Init = 12;  // init atomics
		C.runCommandBuffer()
		p.Init = 2;  // run sieve2
		C.runCommandBuffer()
		if (uint(p.TestL) > 0) {
			// sanity check that only composites were tossed.
			result.checkSieve(K, p)
		}
		//p.Init = 5;  // copy L2 back, for sanity checking
		//C.runCommandBuffer()
		//fmt.Printf("# L2: %d/%d Ll: %d, e: %s\n", p.Debug[1], M, p.Debug[0])

	}

	C.mrhUnMap()
	return
}
func (result *Result) checkSieve(K *big.Int, p *C.struct_Stuff) {
	fmt.Printf("# verify: %d\n", p.TestL)
	// Verify the gpu side is working correctly.
	// Check a subset of composite rejected K/Q values, to ensure they are not prime.
	for i := 0; i < 1000; i++ {
		One := big.NewInt(1)
		P := big.NewInt(int64(result.Exponent))
		o := big.NewInt(int64(p.Test[i]))
		kk := new(big.Int).Add(K, o)
		Q := new(big.Int).Mul(P, kk)
		Q.Lsh(Q, 1)
		Q.Add(Q, One)
		if Q.ProbablyPrime(10) {
			fmt.Printf("Error: ---- %d %d looks prime!\n", i, Q)
		}
	}
}


//
// this was verified by James to produce output accepted by mersenne.ca
//
func (out *Result) doLog() {
	p := out.Exponent
	bitlo := ktobit(p, out.Begink)
	bithi := ktobit(p, out.Endk)
	//fmt.Printf("--- %f %f\n", bitlo, bithi)
	if bitlo <= 1 {
		out.Begink.SetInt64(1)
		bitlo = 1.0
	}
	out.Timestamp = time.Now().UTC().Format("2006-01-02 15:04:05")
	st := "F"
	if len(out.kfactors) == 0 {st = "NF"}

	P := big.NewInt(int64(p))
	One := big.NewInt(1)
	sort.Slice(out.kfactors, func(i, j int) bool {
		return out.kfactors[i].Cmp(out.kfactors[j]) < 0
	})
	factors := make([]*big.Int, 0, len(out.kfactors))
	for _, k := range out.kfactors {
		F := new(big.Int)
		F.Mul(k, P)
		F.Lsh(F, 1)
		F.Add(F, One)
		factors = append(factors, F)
	}
	sfactors := removecomp(factors)

	if len(sfactors) > 0 {
		out.Factors = sfactors
	}
	out.Worktype = "TF"
	out.Status = st
	out.Bitlo = int64(math.Round(bitlo))
	out.Bithi = int64(math.Floor(bithi))
	out.Program = map[string]string{"name": "gvtf", "version":"0.6"}
	o, _ := json.Marshal(out)
	fmt.Println(string(o))

	f, err := os.OpenFile("results.txt", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err == nil {
		fmt.Fprintln(f, string(o))
	}
	f.Close()
}

func nextP(p uint64) uint64 {

	if (p & 1) != 1 { p++}
	p += 2
	P := new(big.Int)
	for !P.SetUint64(p).ProbablyPrime(10) {
		p++
	}
	return p
}
type Work struct {
	exponent    uint64
	low,high    uint64
}

func readwork(filename string) ([]*Work, error) {
	f, err := os.Open(filename)
	if err != nil {return nil, err}

	list := make([]*Work, 0, 1000)
	
	reader := bufio.NewScanner(f)
	for reader.Scan() {
		s := reader.Text()
		f := strings.Split(s, "=")
		if len(f) == 2 && f[0] == "Factor" {
			e := strings.Split(f[1], ",")
			if len(e) == 4 {
				e = e[1:]
			}
			if len(e) == 3 {
				w := &Work{}
				w.exponent, err = strconv.ParseUint(e[0], 10, 64)
				w.low, err = strconv.ParseUint(e[1], 10, 32)
				w.high, err = strconv.ParseUint(e[2], 10, 32)
				list = append(list, w)
			}
		}
	}
	f.Close()
	return list, nil
}
func writework(work []*Work, filename string) error {
	f, err := os.OpenFile(filename, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0644)
	if err != nil {
		fmt.Printf("writework(): %s\n", err)
		return err
	}

	for _, w := range work {
		fmt.Fprintf(f, "Factor=%d,%d,%d\n", w.exponent, w.low, w.high)
	}
	f.Close()
	return nil
}

func (result *Result) runOne(docheckpoint bool) {
	startt := time.Now()
	ii := initInput(result.Exponent)
	elapsed := time.Now().Sub(startt)
	fmt.Printf("# setup for exponent %d took %s\n", result.Exponent, elapsed)
	if ii == 0 {
		result.krestart = new(big.Int)
		filename := fmt.Sprintf("%d.ckp", result.Exponent)
		done := make(chan struct{})
		if docheckpoint {
			result.readcheckpoint(filename)
			go result.checkpointLoop(filename, done)
		}

		result.tfRun()
		result.doLog()
		if docheckpoint {
			done <- struct{}{}
			os.Remove(filename)
		}
	}
}

func runWork(filename, username, host string, docheckpoint bool) {
	work, err := readwork(filename)
	if err != nil {
		fmt.Printf("readwork %s\n", err)
		return
	}
	fmt.Printf("read %d work entries\n", len(work))

	for i, w := range work {
		K1 := big.NewInt(1)
		if w.low > 1 {
			K1 = kfrombit(w.exponent, uint(w.low))
		}
		K2 := kfrombit(w.exponent, uint(w.high))
		result := &Result{Exponent:w.exponent, Rangec:true, User:username, Computer:host, Begink:K1, Endk:K2}
		result.runOne(docheckpoint)

		writework(work[i+1:], filename)
	}
}
func kfrombit(P uint64, bit uint) *big.Int {
	K2 := big.NewInt(1)
	K2.Lsh(K2, bit - 1)
	K2.Div(K2, big.NewInt(int64(P)))
	return K2
}
func ktobit(P uint64, K *big.Int) float64 {
	One:= big.NewInt(1)
	if K.Cmp(One) == 0 {return 1.0}
	Q := new(big.Int)
	Q.SetUint64(P)
	Q.Lsh(Q, 1)
	Q.Mul(Q, K)
	Q.Add(Q, One)
	x, _ := Q.Float64()
	return math.Log2(x)
}
func main() {

	var P uint64
	var username string
	var workfile string
	var docheckpoint bool
	
	u, _ := user.Current()
	host, _ := os.Hostname()

	// 31202533, 726064763, 4112322971, 4113809639, 6000003419, 6000003437, 6000003167, 6000001031, 6000003743
	flag.Uint64Var(&P, "exponent", 4112322971, "The exponent to test")
	flag.StringVar(&workfile, "worktodo", "", "worktodo filename")
	flag.BoolVar(&docheckpoint, "checkpoint", false, "do checkpoints while running")
	flag.BoolVar(&useDouble, "usedouble", true, "Use floating point double values in the shader")
	devn := flag.Int("devn", 0, "Vulkan device number to use")
	k1 := flag.String("k1", "1", "Starting K value")
	B2 := flag.Uint("bithi", 68, "bit limit to test to")
	B1 := flag.Uint("bitlo", 0, "bit limit to test from")

	// the 32-bit version can handle factors under 96-bits.
	// It is generally faster than the 64-bit version, which can handle factors under 128-bits.
	version := flag.Int("version", 32, "version of GPU code to use, 32 or 64")

	flag.StringVar(&username, "username", u.Username, "username")
	flag.StringVar(&host, "host", host, "hostname")
	flag.Parse()

	fmt.Printf("# size of GPU memory allocations: %d (shared) %d (local)\n", C.sizeof_struct_Stuff, C.sizeof_struct_Stuff2)
	if 0 != C.tfVulkanInit(C.int(*devn), C.sizeof_struct_Stuff, C.sizeof_struct_Stuff2, C.int(*version)) {
		fmt.Fprintln(os.Stderr, "could not intialize vulkan")
		return
	}

	if workfile != "" {
		runWork(workfile, username, host, docheckpoint)
	} else {
		K1 := new(big.Int)
		if *B1 == 0 {
			K1.SetString(*k1, 10)
		} else {
			K1 = kfrombit(P, *B1)
		}
		K2 := kfrombit(P, *B2)
			

		if !big.NewInt(int64(P)).ProbablyPrime(10) {
			fmt.Fprintf(os.Stderr, "%d doesn't look prime. How about %d instead?\n", P, nextP(P))
			os.Exit(1)
		}
		result := &Result{Exponent:P, Rangec:true, User:username, Computer:host, Begink:K1, Endk:K2}
		result.runOne(docheckpoint)
	}
	C.cleanup()
}
