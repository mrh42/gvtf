package main

import (
	"fmt"
	"math"
	//"runtime"
	"os"
	"time"
	//"bufio"
	//"strings"
	"strconv"
	"math/big"
	"encoding/json"
)

//#cgo LDFLAGS: -lm -lvulkan
//#include "tf.h"
import "C"

func removecomp(factors []*big.Int) []string {
	out := make([]string, 0, len(factors))

	Z := big.NewInt(0)
	for i, f := range factors {
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
	Timestamp  string   `json:"timestamp"`
	Exponent   uint64   `json:"exponent"`
	Worktype   string   `json:"worktype"`
	Status     string   `json:"status"`
	Bitlo      int64    `json:"bitlo"`
	Bithi      int64    `json:"bithi"`
	Begink     *big.Int   `json:"begink"`
	Endk       *big.Int   `json:"endk"`
	Rangec     bool     `json:"rangecomplete"`
	Factors    []string `json:"factors,omitempty"`
	Program    map[string]string  `json:"program,omitempty"`
	User       string   `json:"user"`
	Computer   string   `json:"computer,omitempty"`
}

func parseint(s string) uint64 {
	i, _ := strconv.ParseInt(s, 10, 64)
	return uint64(i)
}
func parsef(s string) float64 {
	i, _ := strconv.ParseFloat(s, 64)
	return i
}

func initInput(P uint64) {
	//p := C.mrhGetMap();
	p := (*C.struct_Stuff)(C.mrhGetMap());
	
	p.P = C.ulong(P)

	p.Init = 0
	p.L = 0
	p.Ll = 0
	C.runCommandBuffer()
	if p.Ll != C.ListLen {
		fmt.Fprintf(os.Stderr, "-------- init: P.L %d P.Ll %d ListLen %d\n", p.L, p.Ll, C.ListLen)
	}

	p.L = 0
	p.Init = 1

	C.mrhUnMap()
}

func u32n(N *big.Int, pos uint) uint32 {
	m32 := big.NewInt(0xffffffff)

	R := new(big.Int)
	R.Rsh(N, pos * 32)
	R.And(R, m32)
	return uint32(R.Uint64())
}
func big3(u0, u1, u2 C.uint) *big.Int {
	f := big.NewInt(int64(u2))
	f.Lsh(f, 64)
	f1 := big.NewInt(int64(u1))
	f1.Lsh(f1, 32)
	f.Or(f, f1)
	f0 := big.NewInt(int64(u0))
	f.Or(f, f0)
	return f
}

func tfRun(P uint64, K1 *big.Int, bitlimit float64) {
	p := (*C.struct_Stuff)(C.mrhGetMap());

	//K1 = C.M * (K1/C.M);
	M := big.NewInt(C.M)
	K:= new(big.Int)
	K.Div(K1, M)
	K.Mul(K, M)

	p.K[0] = C.uint(u32n(K, 0))
	p.K[1] = C.uint(u32n(K, 1))
	p.K[2] = C.uint(u32n(K, 2))


	p.L = 0
	p.Init = 1
	for i := 0; i < 10; i++ {
		p.Found[i][0] = 0
		p.Found[i][1] = 0
		p.Found[i][2] = 0
	}

	M2 := big.NewInt(C.M2)
	kfound := make([]*big.Int, 0, 10)
	mrhDone := false
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
		//fmt.Printf("lb2: %d %f %f\n", K, lb2, bitlimit)
		if lb2 > bitlimit {
			mrhDone = true
		}
		if p.Debug[1] > 0 {
			for i := 0; i < 10; i++ {
				f := big3(p.Found[i][0], p.Found[i][1], p.Found[i][2])
				f64, _ := f.Float64()
				if f64 > 0 {
					kfound = append(kfound, f)
					flb2 := math.Log2(f64 * float64(P) * 2.0)
					fmt.Fprintf(os.Stderr, "# %d kfactor %d E: %d D: %d %.4f\n", P, f, p.Debug[0], p.Debug[1], flb2);

					p.Found[i][0] = 0;
					p.Found[i][1] = 0;
					p.Found[i][2] = 0;
					//mrhDone = 1;
				}
			}
		}
		
		if (mrhDone) {
			doLog(P, K1, K, kfound)
			break;
		}
		
		if p.L >= p.Ll {
			K.Add(K, M)
			p.K[0] = C.uint(u32n(K, 0))
			p.K[1] = C.uint(u32n(K, 1))
			p.K[2] = C.uint(u32n(K, 2))

			p.L = 0
			p.Debug[0] = 0
			p.Debug[1] = 0
		}
	}

	C.mrhUnMap();
}

func doLog(p uint64, K1, K2 *big.Int, kfactors []*big.Int) {
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
	out.User = "mrh"
	out.Computer = "h0"
	out.Rangec = true
	out.Program = map[string]string{"name": "vulkan-tf", "version":"0.3"}
	o, _ := json.Marshal(out)
	fmt.Println(string(o))
}

func main() {
	//runtime.LockOSThread()
	K1 := new(big.Int)

	P := parseint(os.Args[1])
	K1.SetString(os.Args[2], 10)
	B2 := parsef(os.Args[3])

	r := C.tfVulkanInit(C.sizeof_struct_Stuff, C.sizeof_struct_Stuff2)
	if r == 0 {
		initInput(P);
		tfRun(P, K1, B2);
	}
	C.cleanup()
}
