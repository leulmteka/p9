#ifndef _MARK_AND_SWEEP_H_
#define _MARK_AND_SWEEP_H_

#include "GarbageCollector.h"
#include "stdint.h"
#include "atomic.h"
#include "blocking_lock.h"

using namespace gheith;

class MarkAndSweep : public GarbageCollector
{


public:
    bool *marks;
    MarkAndSweep(void *heapStart, size_t bytes)
    {
        //marks = new bool[bytes / 4];
        // marks[bytes/sizeof(uint32_t)];
        //  for(uint32_t i = 0; i < bytes/sizeof(uint32_t); i++){
        Debug::printf("sz%d\n", bytes / 4);
        // }
    }
    ~MarkAndSweep(){};

     void markBlock(void *ptr);
    // //void markChildren(gheith::object_metadata::objectMeta* parent);
     void sweep();
    //     if (ptr >= gheith::array && ptr < gheith::array + gheith::len * sizeof(int))
    //     {
    //         int idx = ((((uintptr_t)ptr) - ((uintptr_t)gheith::array)) / 4) - 1;
    //         marks[idx] = true;
    //     }
    // }
    void unmarkBlock(int blockIndex)
    {
        marks[blockIndex] = false;
    }
};

#endif