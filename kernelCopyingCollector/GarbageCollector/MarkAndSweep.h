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
    }
    ~MarkAndSweep(){};

    // void *allocate(size_t size) override { return nullptr; };
    // void free(void *ptr) override{Debug::printf("im doing a wrong free\n");}
    // void beginCollection() override{};
    // void garbageCollect() override{};
    // void endCollection() override{};



     void markBlockMarkAndSweep(void *ptr);
    // //void markChildren(gheith::object_metadata::objectMeta* parent);
     void sweepMarkAndSweep();
    //     if (ptr >= gheith::array && ptr < gheith::array + gheith::len * sizeof(int))
    //     {
    //         int idx = ((((uintptr_t)ptr) - ((uintptr_t)gheith::array)) / 4) - 1;
    //         marks[idx] = true;
    //     }
    // }
};

#endif