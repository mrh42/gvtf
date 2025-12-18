# gvtf
A simple vulkan compute shader TF implementation for mersenne primes.  This isn't meant
for production, mostly to explore what can be done and how well in a compute shader.

## math
tf32.comp is a version of the shader using 32-bit unsigned ints to implement 96/192-bit extended math.
tf64.comp uses 64-bit unsigned ints to implement 128/192/256-bit extended math.

## init
The Init==0 part of the shader is called once for a particular exponent, P.  It tests for (3,5)mod8, and (0)mod(primes 3 -> 23). From
446,185,740 (4 * 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23) potential K-values, a list of 72,990,720 candidates is build.

## tf
During an Init==1 call to the shader, each thread takes an offset from the list, adds it to the 128-bit base-K, then
computes Q = P * K * 2 + 1, which is then TF tested. When the entire list has been tested, the cpu side sets K-base += 446,185,740.

## tf.c
This is all the C-code needed to setup vulkan instance, allocated memory on the GPU and load the SPIR-V binary code.

## gvtf.go
This is the front end to handle command line options, initialize parameters in the shared GPU memory, calls the GPU code repeatedly until all the work is done, then outputs JSON formatted results.

For vulkan dev tools see: https://www.lunarg.com/vulkan-sdk/

## usage
     Usage of ./gvtf:
           -bithi float
	          bit limit to test to (default 68)
           -devn int
    	   	  Vulkan device number to use
           -exponent uint
    	          The exponent to test (default 4112322971)
           -k1 string
    	          Starting K value (default "1")
           -stop
    	          stop when factor found
           -version int
    	          version of GPU code to use, 32, 192(64-bit), or 256(64-bit) (default 32)
