#ifndef _COPYING_COLLECTOR_H_
#define _COPYING_COLLECTOR_H_

#include "GarbageCollector.h"
#include "stdint.h"
#include "debug.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "threads.h"
#include "machine.h"

class CopyingCollector : public GarbageCollector
{
private:


public:
    // uint32_t *fromSpace;
    // uint32_t *toSpace;
    // uint32_t *toSpaceFree;
    // size_t heapSize;
    bool *marks;
    CopyingCollector(void *start, size_t size)
    {
       //Debug::printf("entered GC\n");
        // heapSize = size / 2;
        // fromSpace = (uint32_t *)start;
        // toSpace = fromSpace + heapSize / sizeof(uint32_t);
        // toSpaceFree = toSpace;
    }

    ~CopyingCollector() {}

    void copy();
    void updateObjectReferences(objectMeta* objMeta);
    void cleanUpFromSpace();
    void updateThreadStack(gheith::TCBWithStack *tcb); 

    //void *copy(void *obj);
    // {
    //     uint32_t *oldAddr = static_cast<uint32_t *>(obj);
    //     if (isForwarded(oldAddr))
    //     {
    //         return getForwardingAddress(oldAddr);
    //     }

    //     size_t objSize = *oldAddr; // Assuming first word is size
    //     if (toSpaceFree + objSize > toSpace + heapSize / sizeof(uint32_t))
    //     {
    //         Debug::panic("Out of memory in toSpace");
    //     }

    //     uint32_t *newAddr = toSpaceFree;
    //     toSpaceFree += objSize;

    //     for (size_t i = 0; i < objSize; ++i)
    //     {
    //         newAddr[i] = oldAddr[i];
    //     }

    //     *oldAddr = reinterpret_cast<uint32_t>(newAddr) | 1; // Set as forwarded
    //     return newAddr;
    // }

    void updateReferences();

    void updateInternalReferences(objectMeta *meta, uint32_t *newLocation);

    void flip();
    // {
    //     uint32_t *temp = fromSpace;
    //     fromSpace = toSpace;
    //     toSpace = temp;
    //     toSpaceFree = toSpace; // Reset free pointer in the new toSpace
    // }

    void sweep();
    // {
    //     // Since all live objects are already moved to toSpace, just reset fromSpace
    //     // This might be needed to get called inside of flip
    //     bzero(fromSpace, heapSize * sizeof(uint32_t));
    // }

    void markBlock(void *ptr);

    void unmarkBlock(int blockIndex)
    {
        marks[blockIndex] = false;
    }

    // Getter methods
    uint32_t* getFromSpace();
    uint32_t* getToSpace();
    uint32_t* getToSpaceFree();
    size_t getHeapSize();

    // // Setter methods
    void setFromSpace(uint32_t* newFromSpace);
    void setToSpace(uint32_t* newToSpace);
    void setToSpaceFree(uint32_t* newToSpaceFree);
    void setHeapSize(size_t newHeapSize);


    uint32_t *getForwardingAddress(uint32_t *oldAddress);
    // {
    //     return (uint32_t *)*oldAddress;
    // }

    bool isForwarded(uint32_t *address);
    // {
    //     return (*address & 1) != 0;
    // }

    void setForwardingAddress(uint32_t *oldAddress, uint32_t *newAddress);
    // {
    //     // Assumes the lowest bit is used to indicate forwarding
    //     *oldAddress = reinterpret_cast<uint32_t>(newAddress) | 1;
    // }

    void switchFT();
};

#endif