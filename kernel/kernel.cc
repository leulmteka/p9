#include "stdint.h"
#include "debug.h"
#include "ide.h"
#include "ext2.h"
#include "sys.h"
#include "threads.h"
#include "heap.h"
#include "machine.h"
#include "globals.h"

const char* initName = "/sbin/init";


namespace gheith {
    //Shared<Ext2> root_fs = Shared<Ext2>::make(Shared<Ide>::make(1));
        Ext2* root_fs = new Ext2(new Ide(1));

}
struct  n{
    //int x;
    void* next;
    //n(void* next) :next(next){}
    n(void* x ) : next(x) {}
}typedef n;

// void kernelMain(void) {
//     auto argv = new const char* [2];
//     argv[0] = "init";
//     argv[1] = nullptr;
//     // int** arr = new int*[5];
//     // arr[0] = new ;

    
//     int rc = SYS::exec(initName,1,argv);
//     Debug::panic("*** rc = %d",rc);
// }

// void kernelMain(void) {
//    Debug::printf("WE ARE IN KERNEL MAIN %x\n");
//    auto argv = new const char* [2];
//    argv[0] = "init";
//    argv[1] = nullptr;
//    Debug::printf("argv ptr: %x\n", argv);
//    Debug::printf("*** Start of Test\n");
//    int i;
//    int* ptrs[1000];



  
//    // Allocating memory 1000 times
//    for (i = 0; i < 1000; i++) {
//        ptrs[i] = new int(32);  // Assume each allocation is for 32 bytes
//    }


//    Debug::printf("*** First item of ptrs %x\n", ptrs);
//      n* en = new n(ptrs[5]);
//      //en += 1;
//     Debug::printf("en %x\n", en);
//     // Debug::printf("en addr %x\n", &en);
//     // Debug::printf("whats inside n %d\n", *en);






//    // You can add more debug prints or checks here to verify the state of the heap

  
// //   for(uint32_t i = 0; i < kConfig.totalProcs; i++){
// //     delete gheith::idleThreads[i];
// //   }
//      Debug::printf("*** Total Memory Still Allocated After The Test (memory tracker): %d\n", getMemoryTracker());
//    Debug::printf("*** Total Memory Free After The Test (heap size - mem tracker): %d\n", getAvailableMemory());
//    // int rc = SYS::exec(initName,1,argv);
//    // Debug::panic("*** rc = %d",rc);
// }
void kernelMain(void) {
//     uint32_t* ds = (uint32_t*) getKernelDS();
//    Debug::printf("kernelDS %x\n", *ds);
        Debug::printf("hello word\n");

    // char* start = (char*)data_start;
    // char* end = (char*)data_end;
    // Debug::printf("sstart %x and stop %x\n", start, *end);
    // while(start < end){
    //     Debug::printf("hello %x\n", *start);
    //     start++;
    // }
}