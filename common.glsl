// Test for (3,5)mod8, and (0)mod(primes 3 -> 23) in one shot. From 446,185,740 potential K-values,
// a list of 72,990,720 are left to TF test on the GPU. List is an array of 32-bit uints, using
// about 278MB.  Each thread takes an offset from the list, adds it to the 96-bit base-K, then
// computes P * K * 2 + 1, which is then TF tested.
// When the entire list has been tested, K-base += M.

#define M (4 * 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23)  // 446,185,740
#define MnLen 22
const uint Mn[MnLen] = {
	(179 * 181 * 191),                  // 0 -   6,188,209
	(193 * 197 * 199),                  // 1 -   7,566,179
	(29 * 31 * 37 * 41 * 43),           // 2 -  58,642,669
	(47 * 53 * 59 * 61),                // 3 -   8,965,109
	(67 * 71 * 73 * 79),                // 4 -  72,370,439
	(83 * 89 * 97 * 101),               // 5 -  27,433,619
	(103 * 107 * 109 * 113),            // 6 - 135,745,657
	(127 * 131 * 137),                  // 7 -   2,279,269
	(149 * 151 * 157),                  // 8 -   3,532,343
	(163 * 167 * 173),                  // 9 -   4,709,233
	(211 * 223 * 227),                  // 10-  10,775,137
	(229 * 233 * 239),                  // 11-  12,752,323
	(241 * 251 * 257),                  // 12-  15,546,187
	(139 * 263 * 269),                  // 13-   9,833,833
	(271 * 277 * 281),                  // 14-  21,093,827
	(283 * 293 * 307),                  // 15-  25,456,133
	(311 * 313 * 317),                  // 16-  30,857,731
	(331 * 337 * 347),                  // 17-  38,706,809
	(349 * 353 * 359),                  // 18-  44,227,723
	(367 * 373 * 379),                  // 19-  51,881,689
	(383 * 389 * 397),                  // 20-  59,147,839
	(401 * 409 * 419),                  // 21-  68,719,771
//	(421 * 431 * 433),                  // 22-  78,568,283
//	(439 * 443 * 449),                  // 23-  87,320,173
//	(457 * 461 * 463),                  // 24-  97,543,451
//	(467 * 479 * 487),                  // 25- 108,938,491
};

#define ListN 72990720
//#define ListN 2043740170
layout (local_size_x = 64) in;

// This is allocated in HOST_VISIBLE_LOCAL memory, and is shared with host.
// it is somewhat slow, compared to DEVICE_LOCAL memory.
layout(binding = 0) buffer buf
{	
	uint64_t    P;            // input from CPU side
	uint64_t    K[2];         // base K input from CPU side
	uint64_t    Found[10][2]; // output to tell the CPU we found a K resulting in a factor
	uint        NFound;
	uint        Init;         // If this is 1, then we setup our tables once.
	uint        Big;          // Need > 96-bit math
	uint        UseDouble;    // use double math in the shader
	uint        Debug[4];     // output only used for debugging
	uint        TestL;        // number of values returned to the cpu in Test[]
	uint        Test[1000];
};

// This is allocated in DEVICE_LOCAL memory, not shared with host.  See CPU code to see how this is allocated.
// This is much faster to access from the shader, especially if the GPU is in a PCIx1 slot.
layout(binding = 1) buffer buf2
{
	uint    xL, xL2, xLl;
	uint    List[ListN];
	uint    List2[ListN];
	uint    Xx[MnLen][1+Mn[6]/32];
};
