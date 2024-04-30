#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "GarbageCollector/CopyingCollector.h"
#include "threads.h"
#include "globals.h"
// #include "LinkedList.h"
/* A first-fit heap */
namespace gheith
{

    CopyingCollector *GC = nullptr;

    int *array; // a "free" list
    int len;
    int safe = 0;
    static int avail = 0; // head of free list
    static int availFrom = 0;
    static int availTo = 0;
    static BlockingLock *theLock = nullptr;

    uint32_t *fromSpace;
    uint32_t *toSpace;
    uint32_t *toSpaceFree;
    size_t halfHeapSize;
    uint32_t allObjectsSize;

    LinkedList<objMeta, InterruptSafeLock> all_objects{};

    static uint64_t memoryTracker;
    static int totalHeapSize;
    static uint64_t justAllocated;

    void adjustMemoryTracker(int64_t value)
    {
        using namespace gheith;
        auto new_value = memoryTracker += value;
        ASSERT(new_value >= 0);
    }

    void makeTaken(int i, int ints);
    void makeAvail(int i, int ints);

    int abs(int x)
    {
        if (x < 0)
            return -x;
        else
            return x;
    }

    int size(int i)
    {
        return abs(array[i]);
    }

    int headerFromFooter(int i)
    {
        return i - size(i) + 1;
    }

    int footerFromHeader(int i)
    {
        return i + size(i) - 1;
    }

    int sanity(int i)
    {
        if (safe)
        {
            if (i == 0)
                return 0;
            if ((i < 0) || (i >= len))
            {
                Debug::panic("bad header index %d\n", i);
                return i;
            }
            int footer = footerFromHeader(i);
            if ((footer < 0) || (footer >= len))
            {
                Debug::panic("bad footer index %d\n", footer);
                return i;
            }
            int hv = array[i];
            int fv = array[footer];

            if (hv != fv)
            {
                Debug::panic("bad block at %d, hv:%d fv:%d\n", i, hv, fv);
                return i;
            }
        }

        return i;
    }

    int left(int i)
    {
        return sanity(headerFromFooter(i - 1));
    }

    int right(int i)
    {
        return sanity(i + size(i));
    }

    int next(int i)
    {
        return sanity(array[i + 1]);
    }

    int prev(int i)
    {
        return sanity(array[i + 2]);
    }

    void setNext(int i, int x)
    {
        array[i + 1] = x;
    }

    void setPrev(int i, int x)
    {
        array[i + 2] = x;
    }

    void remove(int i)
    {
        int prevIndex = prev(i);
        int nextIndex = next(i);

        if (prevIndex == 0)
        {
            /* at head */
            avail = nextIndex;
        }
        else
        {
            /* in the middle */
            setNext(prevIndex, nextIndex);
        }
        if (nextIndex != 0)
        {
            setPrev(nextIndex, prevIndex);
        }
    }

    void makeAvail(int i, int ints)
    {
        array[i] = ints;
        array[footerFromHeader(i)] = ints;
        setNext(i, avail);
        setPrev(i, 0);
        if (avail != 0)
        {
            setPrev(avail, i);
        }
        avail = i;
    }

    void makeTaken(int i, int ints)
    {
        array[i] = -ints;
        array[footerFromHeader(i)] = -ints;
    }

    int isAvail(int i)
    {
        return array[i] > 0;
    }

    int isTaken(int i)
    {
        return array[i] < 0;
    }
    void printHeap()
    {
        Debug::printf("Heap Layout:\n");
        int p = 0;
        while (p < len)
        {
            int blockSize = abs(size(p));
            if (isAvail(p))
            {
                Debug::printf("| Free Block at %d: Size = %d\n", p, blockSize);
            }
            else
            {
                Debug::printf("| Taken Block at %d: Size = %d\n", p, blockSize);
            }
            p += blockSize;
        }
    }
    void printMarks()
    {
        Debug::printf("Marking Status:\n");
        for (int i = 0; i < len; i++)
        {
            if (isTaken(i))
            {
                if (GC->marks[i])
                    Debug::printf("| Taken Block at %d: Size = %d, Marked = %s\n", i, size(i), GC->marks[i] ? "True" : "False");
            }
        }
    }

};

int getMemoryTracker()
{
    using namespace gheith;
    return memoryTracker;
}

int getAvailableMemory()
{
    using namespace gheith;

    return totalHeapSize - memoryTracker;
}

int getJustAllocated()
{
    using namespace gheith;
    return justAllocated;
}

void heapInit(void *base, size_t bytes)
{
    using namespace gheith;

    Debug::printf("| heap range 0x%x 0x%x\n", (uint32_t)base, (uint32_t)base + bytes);

    /* can't say new becasue we're initializing the heap */
    array = (int *)base;
    len = bytes / 4;
    makeTaken(0, 2);
    makeAvail(2, len - 4);
    makeTaken(len - 2, 2);

    halfHeapSize = (len - 4) * sizeof(int) / 2;
    fromSpace = (uint32_t *)(array + 2);
    toSpace = fromSpace + halfHeapSize / sizeof(uint32_t);
    toSpaceFree = toSpace;
    allObjectsSize = 0;

    theLock = new BlockingLock();
    GC = new CopyingCollector(base, bytes);

    availFrom = avail;  // start of first available block in from-space
    availTo = halfHeapSize;  // start of first available block in to-space
    avail = availFrom;

    memoryTracker = 0;
    totalHeapSize = len * sizeof(int);
}

void *gcMalloc(size_t bytes) {
    using namespace gheith;
    if (bytes == 0) {
        return (void *)array;    
        }

    int ints = ((bytes + 3) / 4) + 2; // Calculate the number of integer slots needed, including header.
    if (ints < 4) ints = 4; // Enforce minimum block size.

    void *res = 0;
    {
        //LockGuardP g{theLock}; // Use RAII-based lock guard to manage lock acquisition and release
        //bool alreadyLocked = ;
        if (!theLock->isMine()) {
            theLock->lock();
        }

        uint32_t *spaceEnd = fromSpace + (halfHeapSize / sizeof(uint32_t)); // Calculate end of active space

       // Debug::printf("Going into the for loop\n");
        for (int p = avail; p != 0 && (uint32_t *)&array[p] < spaceEnd; p = next(p)) {
            // if (!isAvail(p)) {
            //     Debug::panic("Block at %p is not available in gcMalloc\n", &array[p]);
            //     continue; // Safeguard against corrupt free list
            // }
         //   Debug::printf("Passed if statement\n");
            int sz = size(p);
            if (sz >= ints) {
                remove(p);
                int extra = sz - ints;
                if (extra >= 4) {
                    makeTaken(p, ints);
                    makeAvail(p + ints, extra); // Split the block if there's enough space left.
                } else {
                    makeTaken(p, sz); // Use the entire block if not enough space to split.
                }
                res = &array[p + 1];
                adjustMemoryTracker(ints * sizeof(int));
                justAllocated += ints * sizeof(int);
                break; // Exit after allocation.
            }else{
                remove(p);
                int extra = ints - sz;
                if (extra >= 4) {
                    makeTaken(p, sz);
                    makeAvail(p + sz, extra); // Split the block if there's enough space left.
                } else {
                    makeTaken(p, sz); // Use the entire block if not enough space to split.
                }


               //  makeTaken(p, sz);
                res = &array[p + 1];
                adjustMemoryTracker(sz * sizeof(int));
                justAllocated += sz * sizeof(int);
               // continue;
            }
        }
    } // Lock is automatically released here due to RAII

    if (!theLock->isMine()) {
        theLock->unlock();
    }

    // if (res == nullptr) {
    //     Debug::printf("Out of memory in gcMalloc. No block large enough for %d units.\n", ints);
    // }
    return res;
}


void free(void *p)
{
    using namespace gheith;
    if (p == 0)
        return;
    if (p == (void *)array)
        return;

     LockGuardP g{theLock};
    // LockGuardP guard(theLock);

    int idx = ((((uintptr_t)p) - ((uintptr_t)array)) / 4) - 1;
    sanity(idx);
    if (!isTaken(idx))
    {
        Debug::panic("freeing free block, p:%x idx:%d\n", (uint32_t)p, (int32_t)idx);
        return;
    }

    int blockSize = size(idx);
    adjustMemoryTracker(-(blockSize * sizeof(int)));

    int sz = size(idx);

    int leftIndex = left(idx);
    int rightIndex = right(idx);

    if (isAvail(leftIndex))
    {
        remove(leftIndex);
        idx = leftIndex;
        sz += size(leftIndex);
        blockSize += sz;
    }

    if (isAvail(rightIndex))
    {
        remove(rightIndex);
        sz += size(rightIndex);
        blockSize += sz;
    }

    makeAvail(idx, sz);
    
}

/*****************/
/* C++ operators */
/*****************/

// recursively mark children; DFS
void markChildren(objectMeta *parent)
{
    using namespace gheith;
    objectMeta *child = parent->child_next;
    while (child != nullptr)
    {
        if (!child->marked)
        {
            child->marked = true;
        }
        markChildren(child);
        child = child->child_next;
    }
}

void CopyingCollector::setForwardingAddress(objectMeta *meta, void *newAddress)
{
    if (!meta->forwarded)
    {
        meta->newAddr = newAddress;
        meta->forwarded = true;
    }
}

void *CopyingCollector::getForwardedAddress(objectMeta *meta)
{
    return meta->forwarded ? meta->newAddr : nullptr;
}

bool CopyingCollector::isForwarded(objectMeta *meta)
{
    return meta->forwarded;
}

// Getter methods
uint32_t *CopyingCollector::getFromSpace() { return gheith::fromSpace; }
uint32_t *CopyingCollector::getToSpace() { return gheith::toSpace; }
uint32_t *CopyingCollector::getToSpaceFree() { return gheith::toSpaceFree; }
size_t CopyingCollector::getHeapSize() { return gheith::halfHeapSize; }

void CopyingCollector::flipCC() {
    using namespace gheith;

    if (!all_objects.isEmpty()) {
        sweepCC();
    }
    uint32_t *temp = fromSpace;
    fromSpace = toSpace;
    toSpace = temp;
    toSpaceFree = toSpace;
    
    
    // Update avail to point to the start of the new fromSpace
    //avail = (int)((uintptr_t)fromSpace - (uintptr_t)array) / sizeof(int);

    
}

void CopyingCollector::markBlockCC(void *ptr)
{
    using namespace gheith;
    if ((ptr >= fromSpace && ptr < toSpace) || (ptr <= fromSpace && ptr > toSpace))
    {
        uintptr_t index = ((uintptr_t)ptr - (uintptr_t)gheith::array) / sizeof(int) ; //-1? //////

        if (gheith::isTaken(index))
        {

            objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
            if (meta)
                init_get_potential_children(meta); // times out

            if (meta && !meta->marked) // Check if metadata exists and object is not already marked
            {
                meta->marked = true; // Mark the object as reachable
                markChildren(meta);  // Recursively mark all reachable children
            }
        }
    }

    // if (ptr >= &data_start && ptr < &data_end)
    // {
    //     objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
    //     if (meta)
    //         init_get_potential_children(meta); // times out

    //     if (meta && !meta->marked) // Check if metadata exists and object is not already marked
    //     {
    //         meta->marked = true; // Mark the object as reachable
    //         markChildren(meta);  // Recursively mark all reachable children
    //     }
    // }
    // if (ptr >= &bss_start && ptr < &bss_end)
    // {
    //     objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
    //     if (meta)
    //         init_get_potential_children(meta); // times out

    //     if (meta && !meta->marked) // Check if metadata exists and object is not already marked
    //     {
    //         meta->marked = true; // Mark the object as reachable
    //         markChildren(meta);  // Recursively mark all reachable children
    //     }
    // }
}

void CopyingCollector::clearSpace(uint32_t *start, uint32_t *end)
{
    size_t size = (end - start) * sizeof(uint32_t);

    bzero((void *)start, size);
}

// void freeBlock(int index, size_t size) {
//     // Check for adjacent free blocks before and after and merge them if possible
//     using namespace gheith;
//     int leftIndex = left(index);
//     int rightIndex = right(index);

//     if (isAvail(leftIndex)) {
//         // Merge with left block
//         index = leftIndex;
//         size += gheith::size(leftIndex);
//         remove(leftIndex);
//     }

//     if (isAvail(rightIndex)) {
//         // Merge with right block
//         size += gheith::size(rightIndex);
//         remove(rightIndex);
//     }

//     gheith::makeAvail(index, size); // Mark the whole new merged block as available
// }


void CopyingCollector::sweepCC()
{
   // Debug::printf("stuck in sweep\n");
    using namespace gheith;
    objectMeta *current = all_objects.getHead();
    objectMeta *prev = nullptr;
    while (current != nullptr)
    {
        if (!current->marked)
        {
            // Object not marked: it's unreachable, so free it
            objectMeta *toDelete = current;
            void *addr = current->addr; // Save address to free
            current = current->next;
            if (prev != nullptr)
            {
                prev->next = current; // Bypass the deleted node
            }
            else
            {
                allObjectsSize--;
                gheith::all_objects.remove(toDelete);
                //  int index = ((uintptr_t)addr - (uintptr_t)array) / sizeof(int);
                //  freeBlock(index, toDelete->size);
            }

            if (addr != nullptr && addr > (void *)0x200314U)
            {
                uintptr_t index = ((uintptr_t)addr - (uintptr_t)gheith::array) / sizeof(int);
                if (isTaken(index))
                    free(addr);
            }
            free(toDelete);
           // delete toDelete;
        }
        else if (!current->forwarded)
        {
            void *objAddress = current->addr;
            if (objAddress >= fromSpace && objAddress < fromSpace + halfHeapSize)
            {
                free(objAddress); // Free the actual object if there's external resource allocation
            }

            objectMeta *toDelete = current;
            current = current->next;
            if (prev)
            {
                prev->next = current;
            }
            else
            {
                gheith::all_objects.setHead(current);
            }
            free(toDelete);

            
        }
        else
        {
            current->forwarded = false;
            current->marked = false;
            prev = current;          // Update prev only if not deleting the current node
            current = current->next; // Move to the next node
        }
    }

    
}

void CopyingCollector::switchFT(){
    using namespace gheith;

    int temp = availTo;
    availTo = avail;
    avail = temp; //switch to allocating in toSpace
    //availFrom = avail;
}

void CopyingCollector::copyCC()
{
    using namespace gheith;
   //switchFT();
    if (allObjectsSize == 0)
    {
        return;
    }
    if (all_objects.isEmpty())
    {
        return;      
    }
     switchFT();
    for (objectMeta *current = all_objects.getHead(); current != nullptr; current = current->next)
    {
        if (current->marked && !current->forwarded)
        {
            size_t sizeInBytes = current->size;
        //    Debug::printf("WE ARE CALLING GCMALLOC WITH %d\n", sizeInBytes);
           // Debug::printf("NUMBER OF OBJECTS IN ALL OBJECTS %d\n", allObjectsSize);
            //printHeapLayout();
            void *newLocation = gcMalloc(sizeInBytes);
            if (!newLocation)
            {
                Debug::panic("Out of memory during GC copy phase");
            }
            setForwardingAddress(current, newLocation);
            current->addr = (uint32_t *)newLocation;
        }
    }
    return;
}

void CopyingCollector::updateInternalReferencesCC(objectMeta *meta, void *newLocation)
{
    using namespace gheith;
    uintptr_t *ptr = (uintptr_t *)(meta->addr);
    for (size_t i = 0; i < meta->size; ++i)
    {
        uintptr_t possibleAddr = *ptr;
        objectMeta *childMeta = all_objects.find(possibleAddr);
        if (childMeta && childMeta->forwarded)
        {
            *ptr = (uintptr_t)(childMeta->newAddr);
        }
        ++ptr;
    }
}

using namespace gheith;
void init_get_potential_children(objectMeta *parent)
{
    uintptr_t *potentialPointer = (uintptr_t *)parent->addr;
    uintptr_t *end = (uintptr_t *)((char *)parent->addr + parent->size);

    objectMeta *last_child = nullptr;

    while (potentialPointer < end)
    {
        uintptr_t possibleAddr = *potentialPointer; // Dereference potentialPointer to check its content as an address

        // Check if the address falls within any of the managed object areas (heap, data, BSS)
        if ((possibleAddr >= (uintptr_t)gheith::array && possibleAddr < (uintptr_t)gheith::array + gheith::len * sizeof(int)) ||
            (possibleAddr >= (uintptr_t)&data_start && possibleAddr < (uintptr_t)&data_end) ||
            (possibleAddr >= (uintptr_t)&bss_start && possibleAddr < (uintptr_t)&bss_end))
        {

            objectMeta *childMeta = all_objects.find(possibleAddr);
            if (childMeta)
            {
                // Debug::printf("finding children..\n");
                if (parent->child_next == nullptr)
                {
                    parent->child_next = childMeta; // First child
                    last_child = childMeta;
                }
                else
                {
                    if (last_child != nullptr)
                    {
                        last_child->child_next = childMeta; // Linking children in a list
                        last_child = childMeta;
                    }
                }
                childMeta->child_next = nullptr; // Ensure the newly added child points to null
            }
        }
        potentialPointer++;
    }
}
// every dynamically allocated object has to go through here
void *operator new(size_t size)
{

    void *p = gcMalloc(size); // ptr to data
    if (p == 0)
        Debug::panic("out of memory");
    // objMeta* metadata = new objMeta(p, size, false, theLock);

    if (GC)
    {                                                                // heapInit has been called
        objMeta *metadata = (objMeta *)gcMalloc(sizeof(objectMeta)); // Dynamically allocate a new wrapper
        metadata->addr = p;
        metadata->marked = false;
        metadata->size = size;
      //  metadata->theLock = theLock;
        metadata->child_next = nullptr;

        init_get_potential_children(metadata);
        allObjectsSize++;
        all_objects.append(metadata);
    }

    return p;
}

void operator delete(void *p) noexcept
{
    return free(p);
}

void operator delete(void *p, size_t sz)
{
    return free(p);
}

void *operator new[](size_t size)
{
    void *p = gcMalloc(size);
    if (p == 0)
        Debug::panic("out of memory");
    if (GC)
    {                                                                // heapInit has been called
        objMeta *metadata = (objMeta *)gcMalloc(sizeof(objectMeta)); // Dynamically allocate a new wrapper

        metadata->addr = p;
        metadata->marked = false;
        metadata->size = size;
        //metadata->theLock = theLock;
        metadata->child_next = nullptr;

        init_get_potential_children(metadata);
        allObjectsSize++;
        all_objects.append(metadata);
    }
    return p;
}

void operator delete[](void *p) noexcept
{
    return free(p);
}

void operator delete[](void *p, size_t sz)
{
    return free(p);
}