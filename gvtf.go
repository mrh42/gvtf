package main

import (
	"fmt"
	"math"
	"flag"
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

	kfactors   []*big.Int  // not written.
}

func initInput(P uint64) {
	//p := C.mrhGetMap();
	p := (*C.struct_Stuff)(C.mrhGetMap());
	
	p.P = C.uint64_t(P)

	p.Init = 0
	p.L = 0
	p.Ll = 0
	p.Z = 0
	// Run the shader with init==0
	C.runCommandBuffer()
	if p.Ll != C.ListLen {
		fmt.Fprintf(os.Stdout, "# -------- Something went wrong during init: P.L %d P.Ll %d ListLen %d\n", p.L, p.Ll, C.ListLen)
	}

	p.L = 0
	// from now on, init==1
	p.Init = 1

	C.mrhUnMap()
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

func tfRun(P uint64, K1, K2 *big.Int, result *Result) {
	p := (*C.struct_Stuff)(C.mrhGetMap());

	//K1 = C.M * (K1/C.M);
	M := big.NewInt(C.M)
	K := new(big.Int)
	K.Div(K1, M)
	K.Mul(K, M)

	p.K[0] = C.uint64_t(u64n(K, 0))
	p.K[1] = C.uint64_t(u64n(K, 1))


	p.L = 0
	p.Init = 1
	for i := 0; i < 10; i++ {
		p.Found[i][0] = 0
		p.Found[i][1] = 0
	}

	M2 := big.NewInt(C.M2)
	kfound := make([]*big.Int, 0, 10)
	mrhDone := false
	count := 0
	for {
		p.Debug[0] = 0
		p.Debug[1] = 0

		KmM2 := new(big.Int)
		KmM2.Mod(K, M2)
		p.KmodM2 = C.uint(KmM2.Uint64())

		C.runCommandBuffer()

		//fk64, _ := K.Float64()
		//lb2 := math.Log2(fk64 * float64(P) * 2.0)
		count++
		if (count % 2000) == 0 {
			fmt.Fprintf(os.Stdout, "# %d %d \n", K, K2)
		}
		//fmt.Fprintf(os.Stderr, "%d %d\n", count, p.Debug[0])
		p.Debug[0] = 0;
		if p.Debug[1] > 0 {
			for i := 0; i < 10; i++ {
				f := big2(p.Found[i][0], p.Found[i][1])
				f64, _ := f.Float64()
				if f64 > 0 {
					kfound = append(kfound, f)
					flb2 := math.Log2(f64 * float64(P) * 2.0)
					fmt.Fprintf(os.Stdout, "# %d kfactor %d E: %d D: %d %.4f C: %d\n", P, f, p.Debug[0], p.Debug[1], flb2, count);

					p.Found[i][0] = 0;
					p.Found[i][1] = 0;
				}
			}			
		}
		if K.Cmp(K2) > 0 {
			mrhDone = true
		}
		
		if (mrhDone) {
			result.kfactors = kfound
			result.Begink = K1
			result.Endk = K
			break;
		}
		
		if p.L >= p.Ll {
			K.Add(K, M)
			p.K[0] = C.uint64_t(u64n(K, 0))
			p.K[1] = C.uint64_t(u64n(K, 1))

			p.L = 0
			p.Debug[0] = 0
			p.Debug[1] = 0
		}
	}

	C.mrhUnMap()
	return
}

//func doLog(p uint64, K1, K2 *big.Int, kfactors []*big.Int, complete bool) {
func doLog(out *Result) {
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
	out.Program = map[string]string{"name": "vulkan-tf", "version":"0.4"}
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

func runOne(P uint64, K1, K2 *big.Int, result *Result) {
	initInput(P)

	tfRun(P, K1, K2, result)
	doLog(result)
}

func runWork(filename, username, host string) {
	work, err := readwork(filename)
	if err != nil {
		fmt.Printf("readwork %s\n", err)
		return
	}
	fmt.Printf("read %d work entries\n", len(work))

	for i, w := range work {
		result := &Result{Exponent:w.exponent, Rangec:true, User:username, Computer:host}
		K1 := big.NewInt(1)
		if w.low > 1 {
			K1 = kfrombit(w.exponent, uint(w.low))
		}
		K2 := kfrombit(w.exponent, uint(w.high))
		runOne(w.exponent, K1, K2, result)

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
	
	u, _ := user.Current()
	host, _ := os.Hostname()

	// 4112322971, 4113809639, 6000003419, 6000003437, 6000003167
	flag.Uint64Var(&P, "exponent", 4112322971, "The exponent to test")
	flag.StringVar(&workfile, "worktodo", "", "worktodo filename")
	devn := flag.Int("devn", 0, "Vulkan device number to use")
	k1 := flag.String("k1", "1", "Starting K value")
	B2 := flag.Uint("bithi", 68, "bit limit to test to")
	B1 := flag.Uint("bitlo", 0, "bit limit to test from")
	version := flag.Int("version", 32, "version of GPU code to use, 32, 192(64-bit), or 256(64-bit)")

	flag.StringVar(&username, "username", u.Username, "username")
	flag.StringVar(&host, "host", host, "hostname")
	flag.Parse()


	if 0 != C.tfVulkanInit(C.int(*devn), C.sizeof_struct_Stuff, C.sizeof_struct_Stuff2, C.int(*version)) {
		fmt.Fprintln(os.Stderr, "could not intialize vulkan")
		return
	}

	if workfile != "" {
		runWork(workfile, username, host)
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
		result := &Result{Exponent:P, Rangec:true, User:username, Computer:host}
		runOne(P, K1, K2, result)
	}
	C.cleanup()
}
