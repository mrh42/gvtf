#include <stdint.h>

// Test for (3,5)mod8, and (0)mod(primes 3 -> 23) in one shot. From 446,185,740 potential K-values,
// a list of 72,990,720 are left to TF test on the GPU. List is an array of 32-bit uints, using
// about 278MB.  Each thread takes an offset from the list, adds it to the 96-bit base-K, then
// computes P * K * 2 + 1, which is then TF tested.
// When the entire list has been tested, K-base += M.
//
#define M (4 * 3L * 5 * 7 * 11 * 13 * 17 * 19 * 23)  // 446,185,740
#define M2 (29 * 31 * 37 * 41 * 43)                  //  58,642,669
#define ListLen 72990720

// This is allocated in HOST_VISIBLE_LOCAL memory, and is shared with host.
// it is somewhat slow, compared to DEVICE_LOCAL memory.
struct Stuff {
	uint32_t    P[2];  // 64 bit
	uint32_t    K[3];  // 96, but only 64 used currently
	uint32_t    Found[10][3];
	uint32_t    Debug[2];
	uint32_t    Init;
	uint32_t    L;            // start with 0, each thread will increment with AtomicAdd(L, 1)  
	uint32_t    Ll;           // ListLen, when L >= Ll, threads will return.
        //uint32_t    List[ListLen];
	uint32_t    KmodM2;
	//uint32_t    X2[M2];

};
// This is allocated in DEVICE_LOCAL memory, and is not shared with host.
// This is much to access faster from the shader, especially if the GPU is in a PCIx1 slot.
struct Stuff2 {
	uint32_t    Listx[ListLen];  // copy of List.  Just a tiny speed up.
	uint32_t    X2x[M2];
};


int tfVulkanInit();
void runCommandBuffer();
void mrhInit(uint64_t P, uint64_t K1);
void mrhRun(uint64_t P, uint64_t K1);
void cleanup();

struct Stuff * mrhGetMap();
void mrhUnMap();
