# gvtf
A simple vulkan compute shader TF implementation for mersenne primes.  This isn't meant
for production, mostly to explore what can be done and how well in a computer shader.

For vulkan tools see: https://www.lunarg.com/vulkan-sdk/

This version using a golang frontend, with some C-code helping functions.

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
