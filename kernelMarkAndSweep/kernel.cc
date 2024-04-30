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

void kernelMain(void) {
   Debug::printf("*** Start of Mark And Sweep Test\n");
    int i;
    int* ptrs[1000];

    
    // Allocating memory 1000 times
    for (i = 0; i < 1000; i++) {
        ptrs[i] = new int(32);
    }



    Debug::printf("*** First item of ptrs %d\n", ptrs[0]);
    Debug::printf("*** Total Memory Still Allocated After The Test (memory tracker): %d\n", getMemoryTracker());
    Debug::printf("*** Total Memory Free After The Test (heap size - mem tracker): %d\n", getAvailableMemory());


    

    // ptrs[0]++;
    // int memTracker = getMemoryTracker();
    // int availMem = getAvailableMemory();
    // memTracker+=1;
    // memTracker-=1;
    // availMem+=1;
    // availMem-=1;
    // Debug::printf("*** Total Memory Still Allocated After The Test (memory tracker)\n");
    // Debug::printf("*** Total Memory Free After The Test (heap size - mem tracker)\n");


    Debug::printf("*** End of Test\n");
}