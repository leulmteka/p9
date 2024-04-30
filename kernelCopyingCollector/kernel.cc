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
    Debug::printf("*** Start of Copying Collector Stress Test\n");
    int i;
    int* ptrs[1000];

    
    Debug::printf("*** Start of Rapid Allocation\n");
    for (i = 0; i < 1000; i++) {
        ptrs[i] = new int(32);
    }
    Debug::printf("*** Successfully Passed Rapid Allocation\n");


    Debug::printf("*** Start of Large Allocation\n");
    int* ptrsTwo = new int[1000];
    Debug::printf("*** Successfully Passed Large Allocation\n");

    Debug::printf("*** Start of Large & Rapid Allocation\n");
    int* ptrsThree[100];
    for (int j = 0; j < 100; j++) {
        ptrsThree[j] = new int(220);
    }
    Debug::printf("*** Successfully Passed Large & Rapid Allocation\n");

    ptrs[0]++;
    ptrsTwo[0]++;
    ptrsThree[0]++;
    // Compared to Shared Pointers
    if(getAvailableMemory() > 4915359){
        Debug::printf("*** Successfull Garbage Collection\n");
    }
    //Debug::printf("*** Total Memory Free After The Test (heap size - mem tracker): %d\n", getAvailableMemory());

    Debug::printf("*** End of Copying Collector Stress Test\n");
}