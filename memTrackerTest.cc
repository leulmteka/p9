#include "ide.h"
#include "ext2.h"
#include "shared.h"
#include "libk.h"
#include "vmm.h"
#include "config.h"
#include "future.h"
#include "heap.h" 

/* Called by one CPU */
void kernelMain(void) {
    Debug::printf("*** Hello\n");
    // int i;
    // void* ptrs[1000];

    
    // // Allocating memory 1000 times
    // for (i = 0; i < 1000; i++) {
    //     ptrs[i] = operator new(32);  // Assume each allocation is for 32 bytes
    // }


    // // You can add more debug prints or checks here to verify the state of the heap
    // Debug::printf("*** Total allocated memory after test: %d\n", getMemoryTracker());
    // Debug::printf("*** Total free memory after test: %d\n", getAvailableMemory());

}

