#include <stdint.h>

// Test for (3,5)mod8, and (0)mod(primes 3 -> 23) in one shot. From 446,185,740 potential K-values,
// a list of 72,990,720 are left to TF test on the GPU. List is an array of 32-bit uints, using
// about 278MB.  Each thread takes an offset from the list, adds it to the 96-bit base-K, then
// computes P * K * 2 + 1, which is then TF tested.
// When the entire list has been tested, K-base += M.
//

#define M (4 * 3 * 5 * 7 * 11 * 13 * 17 * 19 * 23) // 446,185,740
#define M2 (29 * 31 * 37 * 41 * 43)                 //  58,642,669
#define M3 (47 * 53 * 59 * 61)                      //   8,965,109
#define M4 (67 * 71 * 73 * 79)                      //  27,433,619
#define M5 (83 * 89 * 97 * 101)                     //  72,370,439
#define M6 (103 * 107 * 109 * 113)                  // 135,745,657
#define M7 (127 * 131 * 137 * 139)                  // 316,818,391
#define M8 (149 * 151 * 157)                        //   3,532,343


#define ListLen 72990720
//#define ListLen 2043740170
// This is allocated in HOST_VISIBLE_LOCAL memory, and is shared with host.
// it is somewhat slow, compared to DEVICE_LOCAL memory.
struct Stuff {
	uint64_t    P;            // 64 bit
	uint32_t    Init;         // controls the code path in main(), 0 causes initialization of the Stuff2 tables.
	uint32_t    L;            // start with 0, each thread will increment with AtomicAdd(L, 1)  
	uint32_t    Ll;           // ListLen, when L >= Ll, threads will return.
	uint64_t    K[2];         // 128-bit K
	uint64_t    Found[10][2]; // up to 10 96-bit K values
	uint32_t    NFound;
	uint32_t    Debug[2];     // debugging passed back from the gpu
	uint32_t    L2;
	uint32_t    L3;
	uint32_t    Test[1000];
};
//
// This is allocated in DEVICE_LOCAL memory, and is not shared with host.
// This is much to access faster from the shader, especially if the GPU is in a PCIx1 slot.
// These fields don't need to match the shader directly, the struct just needs to be big enough
// to allocate enough memory on the GPU for what the shader needs.
//
struct Stuff2 {
	uint32_t    List[ListLen];
	uint32_t    List2[ListLen];
	uint32_t    PreTop;
	uint64_t    PreSq[2];
	uint32_t    X2[1+M2/32];
	uint32_t    X3[1+M3/32];
	uint32_t    X4[1+M4/32];
	uint32_t    X5[1+M5/32];
	uint32_t    X6[1+M6/32];
	uint32_t    X7[1+M7/32];
	uint32_t    X8[1+M8/32];
};


int tfVulkanInit(int devn, uint64_t bs1, uint64_t bs2, int version);
void runCommandBuffer();
void cleanup();

//struct Stuff * mrhGetMap();
void * mrhGetMap();
void mrhUnMap();
