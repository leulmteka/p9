#include "libc.h" 

int main() {
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
    shutdown();
    return 0;
}
