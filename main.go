package main

import (
	"fmt"
	"math"
	//"runtime"
	//"os"
	//"time"
	//"bufio"
	//"strings"
	//"strconv"
	//"math/big"
	//"encoding/json"
)

//#cgo LDFLAGS: -lm -lvulkan
//#include "tf.h"
import "C"

func initInput(P, K1 uint64) {
	p := C.mrhGetMap();
	
	K1 = C.M * (K1/C.M);
	p.K[0] = C.uint(K1 & 0xffffffff)
	p.K[1] = C.uint(K1>>32)
	p.K[2] = 0
	p.P[0] = C.uint(P & 0xffffffff)
	p.P[1] = C.uint(P>>32)

	for i := 0; i < 10; i++ {
		p.Found[i][0] = 0
		p.Found[i][1] = 0
		p.Found[i][2] = 0
	}
	p.Debug[0] = 0
	p.Debug[1] = 0
	p.Init = 0

	for i := uint64(0); i < C.M2; i++ {
		q := uint64(P % C.M2) * 2 * i + 1
		if ((q % 29) == 0 ||(q % 31) == 0 ||(q % 37) == 0||(q % 41) == 0 || (q % 43) == 0) {
			p.X2[i] = 0
		} else {
			p.X2[i] = 1
		}
	}
	//
	// This takes a second or so on the cpu, but is so much easier with 64bit ints.
	//
	ones := 0
	for i := uint64(0); i < C.M; i++ {
		q := uint64(P % C.M) * 2 * i + 1
		if (((q&7) == 3) || ((q&7) == 5) || (q%3 == 0) || (q%5 == 0) || (q%7 == 0) || (q%11 == 0) ||
		    (q%13 == 0) || (q%17 == 0) || (q%19 == 0) || (q%23 == 0)) {
			//Kx[i] = 0;
		} else {
			p.List[ones] = C.uint(i)
			ones++
			if (ones > C.ListLen) {
				fmt.Printf("Error: list longer than expected: %d.  %d isn't prime??\n", ones, P)
				break
			}
		}
	}
	fmt.Printf("ones: %d\n", ones)
	p.L = 0;
	p.Ll = C.uint(ones)

	C.runCommandBuffer()
	p.L = 0
	p.Init = 1

	
	C.mrhUnMap();
}

func tfRun(P, K1 uint64) {
	K1 = C.M * (K1/C.M);
	p := C.mrhGetMap();

	kfound := make([]uint64, 0, 10)
	mrhDone := false
	k64 := K1
	for {
		p.Debug[0] = 0
		p.Debug[1] = 0
		p.KmodM2 = C.uint(k64 % C.M2)
		C.runCommandBuffer()

		lb2 := math.Log2(float64(k64) * float64(P) * 2.0)
		if lb2 > 68 {
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
	fmt.Printf("Hello\n")
	//runtime.LockOSThread()

	r := C.tfVulkanInit()
	if r == 0 {
		initInput(4112322971, 1);
		//C.mrhInit(4112322971, 1)

		//C.mrhRun(4112322971, 1);
		tfRun(4112322971, 1);
	}
	C.cleanup()
}
