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
	Begink     uint64   `json:"begink"`
	Endk       uint64   `json:"endk"`
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
	fmt.Fprintf(os.Stderr, "init: P.L %d P.Ll %d ListLen %d\n", p.L, p.Ll, C.ListLen)

	p.L = 0
	p.Init = 1

	C.mrhUnMap()
}

func tfRun(P, K1 uint64, bitlimit float64) {
	p := (*C.struct_Stuff)(C.mrhGetMap());

	K1 = C.M * (K1/C.M);
	p.K[0] = C.uint(K1 & 0xffffffff)
	p.K[1] = C.uint(K1>>32)
	p.K[2] = 0

	p.L = 0
	p.Init = 1
	for i := 0; i < 10; i++ {
		p.Found[i][0] = 0
		p.Found[i][1] = 0
		p.Found[i][2] = 0
	}

	kfound := make([]uint64, 0, 10)
	mrhDone := false
	k64 := K1
	for {
		p.Debug[0] = 0
		p.Debug[1] = 0
		p.KmodM2 = C.uint(k64 % C.M2)
		C.runCommandBuffer()

		lb2 := math.Log2(float64(k64) * float64(P) * 2.0)
		if lb2 > bitlimit {
			mrhDone = true
		}
		for i := 0; i < 10; i++ {
			f64 := uint64(p.Found[i][1])
			f64 <<= 32
			f64 |= uint64(p.Found[i][0])
			if f64 != 0 {
				kfound = append(kfound, f64)
				flb2 := math.Log2(float64(f64) * float64(P) * 2.0)
				fmt.Fprintf(os.Stderr, "# %d kfactor %d E: %d D: %d %.1f\n", P, f64, p.Debug[0], p.Debug[1], flb2);

				p.Found[i][0] = 0;
				p.Found[i][1] = 0;
				//mrhDone = 1;
			}
		}
		if (mrhDone) {
			doLog(P, K1, k64, kfound)
			break;
		}
		
		if p.L >= p.Ll {
			k64 += C.M
			p.K[0] = C.uint(k64 & 0xffffffff)
			p.K[1] = C.uint(k64>>32)
			p.K[2] = 0

			p.L = 0
			p.Debug[0] = 0
			p.Debug[1] = 0
		}
	}

	C.mrhUnMap();
}

func doLog(p, K1, K2 uint64, kfactors []uint64) {
	bitlo := math.Log2(float64(K1) * float64(p) * 2.0 + 1)
	bithi := math.Log2(float64(K2) * float64(p) * 2.0 + 1)
	if K1 == 0 {
		K1 = 1
		bitlo = 1.0
	}
	stamp := time.Now().UTC().Format("2006-01-02 15:04:05")
	st := "F"
	if len(kfactors) == 0 {st = "NF"}

	P := big.NewInt(int64(p))
	One := big.NewInt(1)
	factors := make([]*big.Int, 0, len(kfactors))
	for _, k := range kfactors {
		F := big.NewInt(int64(k))
		F.Mul(F, P)
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
	out.Program = map[string]string{"name": "vulkan-tf", "version":"0.2"}
	o, _ := json.Marshal(out)
	fmt.Println(string(o))
}

func main() {
	//runtime.LockOSThread()

	P := parseint(os.Args[1])
	B := parsef(os.Args[2])
	r := C.tfVulkanInit(C.sizeof_struct_Stuff, C.sizeof_struct_Stuff2)
	if r == 0 {
		initInput(P);
		tfRun(P, 1, B);
	}
	C.cleanup()
}
