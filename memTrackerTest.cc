#include "ide.h"
#include "ext2.h"
#include "shared.h"
#include "libk.h"
#include "vmm.h"
#include "config.h"
#include "future.h"
#include "heap.h" 
#include "threads.h" 

/* Called by one CPU */
void kernelMain(void) {
    Debug::printf("*** Start of Test\n");
    int i;
    int* ptrs[1000];

    
    // Allocating memory 1000 times
    for (i = 0; i < 1000; i++) {
        ptrs[i] = (int*)operator new(32);
    }


    Debug::printf("*** First item of ptrs %d\n", ptrs[0]);
    Debug::printf("*** Total Memory Still Allocated After The Test (memory tracker): %d\n", getMemoryTracker());
    Debug::printf("*** Total Memory Free After The Test (heap size - mem tracker): %d\n", getAvailableMemory());

}

