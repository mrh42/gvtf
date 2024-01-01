package main

import (
	"fmt"
	"math"
	"flag"
	"os"
	"os/user"
	"time"
	"sort"
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
			fmt.Fprintf(os.Stderr, "# %d is composite\n", f)
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

type Jtf struct {
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
}

func initInput(P uint64) {
	//p := C.mrhGetMap();
	p := (*C.struct_Stuff)(C.mrhGetMap());
	
	p.P = C.uint64_t(P)

	p.Init = 0
	p.L = 0
	p.Ll = 0
	p.Z = 0
	C.runCommandBuffer()
	if p.Ll != C.ListLen {
		fmt.Fprintf(os.Stderr, "# -------- Something went wrong during init: P.L %d P.Ll %d ListLen %d\n", p.L, p.Ll, C.ListLen)
	}

	p.L = 0
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

func tfRun(P uint64, K1 *big.Int, bitlimit float64, stop bool) {
	p := (*C.struct_Stuff)(C.mrhGetMap());

	//K1 = C.M * (K1/C.M);
	M := big.NewInt(C.M)
	K:= new(big.Int)
	K.Div(K1, M)
	K.Mul(K, M)

	p.K[0] = C.uint64_t(u64n(K, 0))
	p.K[1] = C.uint64_t(u64n(K, 1))
	//p.K[2] = C.uint(u32n(K, 2))


	p.L = 0
	p.Init = 1
	for i := 0; i < 10; i++ {
		p.Found[i][0] = 0
		p.Found[i][1] = 0
		//p.Found[i][2] = 0
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
		//p.KmodM2 = C.uint(k64 % C.M2)
		p.KmodM2 = C.uint(KmM2.Uint64())

		C.runCommandBuffer()

		fk64, _ := K.Float64()
		lb2 := math.Log2(fk64 * float64(P) * 2.0)
		count++
		if (count % 2000) == 0 {
			fmt.Fprintf(os.Stderr, "# %f %f\n", lb2, bitlimit)
		}
		if p.Debug[1] > 0 {
			for i := 0; i < 10; i++ {
				f := big2(p.Found[i][0], p.Found[i][1])
				f64, _ := f.Float64()
				if f64 > 0 {
					kfound = append(kfound, f)
					flb2 := math.Log2(f64 * float64(P) * 2.0)
					fmt.Fprintf(os.Stderr, "# %d kfactor %d E: %d D: %d %.4f C: %d\n", P, f, p.Debug[0], p.Debug[1], flb2, count);

					p.Found[i][0] = 0;
					p.Found[i][1] = 0;
					//mrhDone = 1;
				}
			}
			if stop {mrhDone = true}
		}
		if lb2 > bitlimit {
			mrhDone = true
		}
		
		if (mrhDone) {
			doLog(P, K1, K, kfound, !stop)
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

	C.mrhUnMap();
}

func doLog(p uint64, K1, K2 *big.Int, kfactors []*big.Int, complete bool) {
	k1f, _ := K1.Float64()
	k2f, _ := K2.Float64()
	bitlo := math.Log2(k1f * float64(p) * 2.0 + 1)
	bithi := math.Log2(k2f * float64(p) * 2.0 + 1)
	if k1f <= 1 {
		K1.SetInt64(1)
		bitlo = 1.0
	}
	stamp := time.Now().UTC().Format("2006-01-02 15:04:05")
	st := "F"
	if len(kfactors) == 0 {st = "NF"}

	P := big.NewInt(int64(p))
	One := big.NewInt(1)
	sort.Slice(kfactors, func(i, j int) bool {
		return kfactors[i].Cmp(kfactors[j]) < 0
	})
	factors := make([]*big.Int, 0, len(kfactors))
	for _, k := range kfactors {
		F := new(big.Int)
		F.Mul(k, P)
		F.Lsh(F, 1)
		F.Add(F, One)
		factors = append(factors, F)
	}
	sfactors := removecomp(factors)

	out := &Jtf{}
	if len(sfactors) > 0 {
		out.Factors = sfactors
	}
	out.Timestamp = stamp
	out.Exponent = p
	out.Worktype = "TF"
	out.Status = st
	out.Begink = K1
	out.Endk = K2
	out.Bitlo = int64(bitlo)
	out.Bithi = int64(bithi)
	u, _ := user.Current()
	out.User = u.Username
	out.Computer, _ = os.Hostname()
	out.Rangec = complete
	out.Program = map[string]string{"name": "vulkan-tf", "version":"0.3"}
	o, _ := json.Marshal(out)
	fmt.Println(string(o))
}

func nextP(p uint64) uint64 {

	p += 2
	P := new(big.Int)
	for !P.SetUint64(p).ProbablyPrime(10) {
		p++
	}
	return p
}

func main() {

	// 4112322971, 4113809639, 6000003419, 6000003437, 6000003167
	Pp := flag.Uint64("exponent", 4112322971, "The exponent to test")
	devn := flag.Int("devn", 0, "Vulkan device number to use")
	k1 := flag.String("k1", "1", "Starting K value")
	B2 := flag.Float64("bithi", 68.0, "bit limit to test to")
	version := flag.Int("version", 32, "version of GPU code to use, 32, 192(64-bit), or 256(64-bit)")
	stop := flag.Bool("stop", false, "stop when factor found")

	flag.Parse()
	//runtime.LockOSThread()

	//count := *countp
	P := *Pp
	K1 := new(big.Int)

	r := C.tfVulkanInit(C.int(*devn), C.sizeof_struct_Stuff, C.sizeof_struct_Stuff2, C.int(*version))
	K1.SetString(*k1, 10)
	if r == 0 {
		initInput(P);
		tfRun(P, K1, *B2, *stop);
		C.cleanup()
	}
}
