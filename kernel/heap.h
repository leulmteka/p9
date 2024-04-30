#ifndef _HEAP_H_
#define _HEAP_H_

#include "stdint.h"
#include "LinkedList.h"
class BlockingLock;
class InterruptSafeLock;
typedef struct objectMeta
{
    void *addr;
    size_t size;
    bool marked;
    BlockingLock *theLock;
    objectMeta *next;
    objectMeta *child_next;

    void *newAddr;
    bool forwarded = false;
} objMeta;

template <typename T, typename LockType>
class LinkedList; // find children in threads.

extern void heapInit(void *start, size_t bytes);

extern "C" void *malloc(size_t size);
extern "C" void free(void *p);
extern int getMemoryTracker();
extern int getAvailableMemory();
extern int getJustAllocated();
extern int getAmountFreed();
extern void init_get_potential_children(objectMeta *parent);
namespace gheith
{
    extern LinkedList<objMeta, InterruptSafeLock> all_objects;
}

#endif