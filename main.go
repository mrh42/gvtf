package main

import (
	"fmt"
	"math"
	//"runtime"
	"os"
	//"time"
	//"bufio"
	//"strings"
	"strconv"
	//"math/big"
	//"encoding/json"
)

//#cgo LDFLAGS: -lm -lvulkan
//#include "tf.h"
import "C"

func parseint(s string) uint64 {
	i, _ := strconv.ParseInt(s, 10, 64)
	return uint64(i)
}

func mklist(p, m, n1, n2 uint64) ([]uint32) {

	list := make([]uint32, C.ListLen)
	x := 0
	for i := n1; i < n2; i++ {
		q := uint64(p % m) * 2 * i + 1
		if (((q&7) == 3) || ((q&7) == 5) || (q%3 == 0) || (q%5 == 0) || (q%7 == 0) || (q%11 == 0) ||
		    (q%13 == 0) || (q%17 == 0) || (q%19 == 0) || (q%23 == 0)) {
			//
		} else {
			list[x] = uint32(i)
			x = x + 1
		}
	}
	return list
}

func initInput(P, K1 uint64) {
	//p := C.mrhGetMap();
	p := (*C.struct_Stuff)(C.mrhGetMap());
	
	K1 = C.M * (K1/C.M);
	p.K[0] = C.uint(K1 & 0xffffffff)
	p.K[1] = C.uint(K1>>32)
	p.K[2] = 0
	p.P = C.ulong(P)

	for i := 0; i < 10; i++ {
		p.Found[i][0] = 0
		p.Found[i][1] = 0
		p.Found[i][2] = 0
	}
	p.Debug[0] = 0
	p.Debug[1] = 0

	p.Init = 0
	p.L = 0
	p.Ll = 0
	C.runCommandBuffer()
	fmt.Printf("P.L %d P.Ll %d ListLen %d\n", p.L, p.Ll, C.ListLen)

	p.L = 0
	p.Init = 1
	
	C.mrhUnMap()
}

func tfRun(P, K1 uint64, bitlimit float64) {
	K1 = C.M * (K1/C.M);
	p := (*C.struct_Stuff)(C.mrhGetMap());

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
				fmt.Printf("# %d kfactor %d E: %d D: %d %.1f\n", P, f64, p.Debug[0], p.Debug[1], flb2);

				p.Found[i][0] = 0;
				p.Found[i][1] = 0;
				//mrhDone = 1;
			}
		}
		if (mrhDone) {
			fmt.Printf("%v\n", kfound)
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

func main() {
	//runtime.LockOSThread()

	P := parseint(os.Args[1])
	B := parseint(os.Args[2])
	r := C.tfVulkanInit(C.sizeof_struct_Stuff, C.sizeof_struct_Stuff2)
	if r == 0 {
		initInput(P, 1);
		fmt.Printf("init done\n")
		tfRun(P, 1, float64(B));
	}
	C.cleanup()
}
