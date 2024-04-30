#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "GarbageCollector/CopyingCollector.h"
#include "threads.h"
#include "globals.h"
//#include "LinkedList.h"
/* A first-fit heap */
namespace gheith
{

    CopyingCollector *GC = nullptr;




    int *array; 

    int len;
    int safe = 0;
    static int avail = 0; // head of free list
    //static int availFrom = 0;
    static int availTo = 0;
    static BlockingLock *theLock = nullptr;

    uint32_t *fromSpace;
   uint32_t *toSpace;
   size_t halfHeapSize;


    //static Queue<objMeta, NoLock> all_objects{}; // no lock for now
    LinkedList<objMeta, InterruptSafeLock> all_objects{};

    static uint64_t memoryTracker;
    static int totalHeapSize;
    static uint64_t justAllocated;

    void adjustMemoryTracker(int64_t value)
    {
        using namespace gheith;
        // LockGuardP lockGuard(theLock);
        // auto old_value = memoryTracker;
        auto new_value = memoryTracker += value;
        // Debug::printf("Adjusting memoryTracker: old value = %lu, adjustment = %lu, new value = %lu\n", old_value, value, new_value);
        ASSERT(new_value >= 0); // Ensure that the memory tracker does not underflow
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
            //availFrom = avail;
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

    void makeAvail(int i, int ints) //2, len - 4
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
void printHeap()
    {
        using namespace gheith;
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

    void heapInit(void *base, size_t bytes) {
        using namespace gheith;

        Debug::printf("| heap range 0x%x 0x%x\n", (uint32_t)base, (uint32_t)base + bytes); //~500000 bytes

        // Initialize the array and calculate its length in terms of integers
        array = (int *)base;
        len = bytes / sizeof(int);  // total number of integers in the heap

        // Divide the heap into two halves
        halfHeapSize = len / 2;  // half the number of integers
        fromSpace = (uint32_t *)(array + 2);
 toSpace = (uint32_t *)((array + halfHeapSize)) ;
            Debug::printf("heap start %d, heap end %d\n", base, (uint32_t) base + bytes);
        Debug::printf("from: %d, to: %d, half: %d, len: %d, arr: %d\n", fromSpace, toSpace, halfHeapSize, len, (uint32_t)base + bytes);

        // makeTaken(halfHeapSize, 2);  // boundary block at the start of the to-space
        // makeAvail(halfHeapSize + 2, halfHeapSize - 4);  // the initial available block in the to-space
        // makeTaken(len - 2, 2);  // boundary block at the end of the heap

        

        // Initialize both spaces with a taken block at the beginning and end
                makeTaken(0, 2);  // boundary block at the start of the heap
        makeAvail(2, halfHeapSize - 4);  // the initial available block in the from-space
        makeTaken(halfHeapSize - 2, 2);  // boundary block at the end of the from-space


        // makeTaken(halfHeapSize - 1, 2);  // boundary block at the end of the from-space
        // makeAvail(halfHeapSize, halfHeapSize + 1);  // the initial available block in the from-space
        // makeTaken(halfHeapSize + 1, 2);

        makeTaken((int)halfHeapSize, 2);  // boundary block at the start of the to-space
        makeAvail((int)halfHeapSize + 2, len - 4);  // the initial available block in the to-space
        makeTaken(len + 50, 2);  // boundary block at the end of the heap

        //avail = 2;
        // Set initial avail pointers


        // makeTaken(0, 2);
        // makeAvail(2, len - 4);
        // makeTaken(len - 2, 2);



        //availFrom = 0;  // start of first available block in from-space
        availTo = (int)halfHeapSize + 4;  // start of first available block in to-space
        //availTo = (int)toSpace + 2;
        //avail = availFrom;  // initially allocate from from-space
        //Debug::printf("AVAIL IS %d\n", avail);
        theLock = new BlockingLock();
        GC = new CopyingCollector(base, bytes);  // initialize the garbage collector

        memoryTracker = 0;
        totalHeapSize = len * sizeof(int);

    }

void *malloc(size_t bytes)
{
    using namespace gheith;
    // Debug::printf("malloc(%d)\n",bytes);
    if (bytes == 0)
        return (void *)array;

    int ints = ((bytes + 3) / 4) + 2;
    if (ints < 4)
        ints = 4;

    LockGuardP g{theLock};

    void *res = 0;

    int mx = 0x7FFFFFFF;
    int it = 0;

    {
        int countDown = 20;
        int p = avail;
        while (p != 0)
        {
            if (!isAvail(p))
            {
                Debug::panic("block is not available in malloc %p\n", p);
            }
            int sz = size(p);

            if (sz >= ints)
            {
                if (sz < mx)
                {
                    mx = sz;
                    it = p;
                }
                countDown--;
                if (countDown == 0)
                    break;
            }
            p = next(p);
        }
    }

    if (it != 0)
    {
        remove(it);
        int extra = mx - ints;
        if (extra >= 4)
        {
            makeTaken(it, ints);
            makeAvail(it + ints, extra);
        }
        else
        {
            makeTaken(it, mx);
        }
        res = &array[it + 1];
        //adjustMemoryTracker(bytes);
         adjustMemoryTracker(ints * sizeof(int)); //5221319

        justAllocated+=bytes;
    }

    return res;
}

void *ccmalloc(size_t bytes, uint32_t* fromSpace)
{
    using namespace gheith;
    // Debug::printf("malloc(%d)\n",bytes);
    if (bytes == 0)
        return (void *)array;

    int ints = ((bytes + 3) / 4) + 2;
    if (ints < 4)
        ints = 4;
    Debug::printf("avail is %d\n", avail);
    LockGuardP g{theLock};

    void *res = 0;

    // int mx = 0x7FFFFFFF;
    int mx = (int) (fromSpace + halfHeapSize);
    int it = 0;
    
    {
        int countDown = 20;
        int p = avail;
        // availFrom = p;
        Debug::printf("avail: %d avail from: %d, avail to: %d\n", avail, avail, availTo);
        while (p != 0)
        {
            if (!isAvail(p))
            {
                Debug::printf("p invalid %d\n", p);
                printHeap();
                Debug::panic("block is not available in malloc %p\n", p);
            }
            int sz = size(p);

            if (sz >= ints)
            {
                if (sz < mx)
                {
                    mx = sz;
                    it = p;
                }
                countDown--;
                if (countDown == 0)
                    break;
            }
            p = next(p);
            // availFrom = p;

        }
    }

    if (it != 0)
    {
        remove(it);
        int extra = mx - ints;
        if (extra >= 4)
        {
            makeTaken(it, ints);
            makeAvail(it + ints, extra);
        }
        else
        {
            makeTaken(it, mx);
        }
        res = &array[it + 1];
        //adjustMemoryTracker(bytes);
         adjustMemoryTracker(ints * sizeof(int)); //5221319

        justAllocated+=bytes;
    }

    return res;
}


void *gcMalloc(size_t bytes) {
    using namespace gheith;

    if (bytes == 0) {
        return nullptr; // Return nullptr for zero-byte requests.
    }

    // Calculate number of int-sized units needed, including metadata.
    int ints = ((bytes + sizeof(int) - 1) / sizeof(int)) + 2;
    if (ints < 4) {
        ints = 4; // Enforce a minimum block size to handle metadata.
    }

    LockGuardP g{theLock}; // Acquire the lock to protect heap structures.

    void *res = nullptr;

    // Determine the correct space to search in based on GC phase.
   // uint32_t *space = fromSpace < toSpace ? fromSpace : toSpace;
    // uint32_t *spaceEnd = fromSpace < toSpace ? toSpace : fromSpace + halfHeapSize / sizeof(uint32_t);
    uint32_t *spaceEnd = fromSpace + halfHeapSize ;

    // Iterate over available blocks to find the first suitable one.
    for (int p = avail; p != 0 && (uint32_t *)&array[p] < spaceEnd; p = next(p)) {
        if (!isAvail(p)) {
            Debug::panic("block is not available in malloc %p\n", p);
        }
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
            return res;
        }
    }

    Debug::printf("Out of memory HERE\n");
    return nullptr; // No suitable block found, report out of memory.
}


void free(void *p)
{
    using namespace gheith;
    if (p == 0)
        return;
    if (p == (void *)array)
        return;

    LockGuardP g{theLock};

    int idx = ((((uintptr_t)p) - ((uintptr_t)array)) / 4) - 1;
    sanity(idx);
    if (!isTaken(idx))
    {
        Debug::panic("freeing free block, p:%x idx:%d\n", (uint32_t)p, (int32_t)idx);
        return;
    }
    // GC->unmarkBlock(idx);

    int blockSize = size(idx);
    //adjustMemoryTracker(-blockSize * sizeof(int));
    adjustMemoryTracker(-(blockSize * sizeof(int)));

       //Debug::printf("THIS IS THE AMOUNT OF MEMORY ALLOCATED! %d\n", memoryTracker);
        //Debug::printf("THIS IS THE AMOUNT OF MEMORY FREE! %d\n", getAvailableMemory());

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

//recursively mark children; DFS
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



// Getter methods


void CopyingCollector::flip() {
    using namespace gheith;

    // Swap the spaces
    uint32_t* tempSpace = fromSpace;
    fromSpace = toSpace;
    toSpace = tempSpace;

    // Swap the avail pointers
    // int tempAvail = avail;
    // avail = availTo;
    // availTo = tempAvail;

    Debug::printf("Spaces flipped. New fromSpace starts at %d, New toSpace starts at %d\n", fromSpace, toSpace);
    Debug::printf("New avail from: %d, New avail to: %d\n", avail, availTo);
}


// void CopyingCollector::markBlock(void *ptr)
// {
//    using namespace gheith;
//    // Check the bounds to ensure 'ptr' points within the managed array space
//    if ( (ptr >= fromSpace && ptr < toSpace)|| (ptr <= fromSpace && ptr > toSpace))
//    {
//        // Calculate index to see if the pointer is pointing to a valid object start
//        uintptr_t index = ((uintptr_t)ptr - (uintptr_t)gheith::array) / sizeof(int) - 1; //-1?
//        // Check if the index is within bounds and the slot is marked as taken
//        if (gheith::isTaken(index))
//        {
//            // Find the metadata for the object at the pointer address
//            objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
//            if (meta)
//                init_get_potential_children(meta); // times out
//            if (meta && !meta->marked) // Check if metadata exists and object is not already marked
//            {
//                 Debug::printf("found a match %x\n", ptr);
//                meta->marked = true; // Mark the object as reachable
//                markChildren(meta);  // Recursively mark all reachable children
//            }
//        }
//    }
//     if(ptr >= &data_start && ptr < &data_end){
//          objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
//             if(meta)           init_get_potential_children(meta); //times out
//
//             if (meta && !meta->marked) // Check if metadata exists and object is not already marked
//             {
//                 //Debug::printf("found a match %x\n", ptr);
//
//                 meta->marked = true; // Mark the object as reachable
//                 markChildren(meta);  // Recursively mark all reachable children
//             }
//     }
//     if(ptr >= &bss_start && ptr < &bss_end){
//          objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
//             if(meta)           init_get_potential_children(meta); //times out
//
//             if (meta && !meta->marked) // Check if metadata exists and object is not already marked
//             {
//                 //Debug::printf("found a match %x\n", ptr);
//
//                 meta->marked = true; // Mark the object as reachable
//                 markChildren(meta);  // Recursively mark all reachable children
//             }
//     }
// }
void CopyingCollector::markBlock(void *ptr)
{
    // uintptr_t index = (((uintptr_t)ptr - (uintptr_t)gheith::array) / sizeof(int)) - 1; //header

    // if (ptr >= gheith::array && ptr < gheith::array + gheith::len * sizeof(int)) //valid?
    // {
    //     if (gheith::isTaken(index))
    //     {
    //         //marks[index] = true; //reachable
    //         all_objects.find((uintptr_t) ptr)->marked = true; //will this get duplicates?
    //     }
    // }
    // Check the bounds to ensure 'ptr' points within the managed array space
    if (ptr >= gheith::array && ptr < gheith::array + gheith::len * sizeof(int))
    {
        // Calculate index to see if the pointer is pointing to a valid object start
        uintptr_t index = ((uintptr_t)ptr - (uintptr_t)gheith::array) / sizeof(int)  -1; //-1?

        // Check if the index is within bounds and the slot is marked as taken
        if (gheith::isTaken(index))
        {   

            // Find the metadata for the object at the pointer address
            objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
            if(meta)           init_get_potential_children(meta); //times out

            if (meta && !meta->marked) // Check if metadata exists and object is not already marked
            {
                Debug::printf("found a match %x\n", ptr);

                meta->marked = true; // Mark the object as reachable
                markChildren(meta);  // Recursively mark all reachable children
            }
        }
    }
    if(ptr >= &data_start && ptr < &data_end){
         objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
            if(meta)           init_get_potential_children(meta); //times out

            if (meta && !meta->marked) // Check if metadata exists and object is not already marked
            {
                //Debug::printf("found a match %x\n", ptr);

                meta->marked = true; // Mark the object as reachable
                markChildren(meta);  // Recursively mark all reachable children
            }
    }
    if(ptr >= &bss_start && ptr < &bss_end){
         objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
            if(meta)           init_get_potential_children(meta); //times out

            if (meta && !meta->marked) // Check if metadata exists and object is not already marked
            {
                //Debug::printf("found a match %x\n", ptr);

                meta->marked = true; // Mark the object as reachable
                markChildren(meta);  // Recursively mark all reachable children
            }
    }
}
void CopyingCollector::sweep()
{
    
    objectMeta *current = gheith::all_objects.getHead();
    using namespace gheith;
    objectMeta *prev = nullptr;
    while (current != nullptr)
    {
        if (!current->marked)
        {
            // Object not marked: it's unreachable, so free it
            objectMeta *toDelete = current;
             void *addr = current->addr; // Save address to free

            // Advance the list before removing the current node
            current = current->next;

            // Remove from the queue
            if (prev != nullptr)
            {
                //Debug::printf("to remove: %x\n", addr);
                prev->next = current; // Bypass the deleted node
                //if(prev->next)
                //Debug::printf("removed %x. prev next is %x\n", addr, prev->next->addr);
                //else
                //Debug::printf("removed %x. prev next is null\n", addr);
            }
            else
            {
                //Debug::printf("to remove: %x\n", addr);
                //Debug::printf("removed %x\n",gheith::all_objects.remove(toDelete)); // Update head if the first element is being removed
                gheith::all_objects.remove(toDelete);
                }
            //Debug::printf("deleting.. %x\n",addr );
            // Free the actual object memory

            
            if(addr != nullptr && addr > (void*) 0x200314U){ 
                uintptr_t index = ((uintptr_t)addr - (uintptr_t)gheith::array) / sizeof(int) ;
                if(isTaken(index)  )
                    free(addr);
               // Debug::printf("deleted address %x\n", addr);
            }
             //to avoid idles (fix) //
//*** Total Memory Free After The Test (heap size - mem tracker): 5200860
//*** Total Memory Still Allocated After The Test (memory tracker): 17532
//*** Total Memory Free After The Test (heap size - mem tracker): 5225348
            // // Free the metadata
             free(toDelete);
        } else if(!current->forwarded)
       {
          // The object was not forwarded, so it's considered garbage
          void *objAddress = current->addr;
          if (objAddress >= fromSpace && objAddress < fromSpace + halfHeapSize) {
              // This was a valid address in the fromSpace that wasn't copied
              free(objAddress); // Free the actual object if there's external resource allocation
          }




          objectMeta *toDelete = current;
          current = current->next;
          if (prev) {
              prev->next = current;
          } else {
              gheith::all_objects.setHead(current);
          }
          free(toDelete);
       }else
        {
            if((current->addr >= &data_start && current->addr < &data_end) ||  (current->addr >= &bss_start && current->addr < &bss_end)) continue;

            // Object was marked: unmark for next GC cycle
            current->marked = false;
            prev = current;          // Update prev only if not deleting the current node
            current = current->next; // Move to the next node

            current->forwarded = false;

        }
    }
}

//switch from -> to, to -> from
void CopyingCollector::switchFT(){
    using namespace gheith;

    int temp = availTo;
    availTo = avail;
    avail = temp; //switch to allocating in toSpace
    //avail += 0x20;
    //availFrom = avail;
}
// from - to
// to -- from
// 
void CopyingCollector::copy() {
  using namespace gheith;
  
 switchFT();
  for (objectMeta *current = all_objects.getHead(); current != nullptr; current = current->next) {
      if(current->marked && (!current->forwarded)) {
          size_t sizeInBytes = current->size;

          //allocate in the toSpace now
          void *newLocation = ccmalloc(sizeInBytes, toSpace);
        //   Debug::printf("is taken? %d\n", isAvail());
          if (newLocation == nullptr || newLocation == 0) {
              Debug::panic("Out of memory during GC copy phase");
          }
          current->newAddr = newLocation;
          current->forwarded = true;
          memcpy(current->newAddr, current->addr, sizeInBytes);

          Debug::printf("old status %d, new status %d\n", (current->addr),   (current->newAddr));
      }
  }

//     uint32_t *temp = fromSpace;
//    fromSpace = toSpace;
//    toSpace = temp;
// //              switchFT();
//     int tempAvail = availTo;
//     availTo = avail;
//     avail = tempAvail;
   
// uint32_t *temp = fromSpace;
//     fromSpace = toSpace;
//     toSpace = temp;

    // Swap the available space pointers

    
}
void CopyingCollector::updateThreadStack(gheith::TCBWithStack *tcb) {
    using namespace gheith;
    uint32_t **stackStart = (uint32_t **)tcb->stack;
    uint32_t *stackEnd = (uint32_t *)&stackStart[STACK_WORDS];

    // Scanning from the start to the end of the stack
    for (uint32_t **ptr = stackStart; ptr < (uint32_t **)stackEnd; ptr++) {
        uint32_t candidate = (uint32_t)*ptr;  // Dereferencing to get the potential pointer
        if ((void *)candidate >= array && (void *)candidate < array + len * sizeof(int)) {
            objectMeta *meta = all_objects.find((uintptr_t)candidate);
            if (meta && meta->forwarded) {  // Ensure the object was moved
                *ptr = (uint32_t*)meta->newAddr;  // Update the stack pointer to new address
            }
        }
    }
}
void CopyingCollector::updateReferences() {
    using namespace gheith;

    for (uint32_t i = 0; i < kConfig.totalProcs; i++) {
        TCBWithStack *tcb = (TCBWithStack *)activeThreads[i];
        if (tcb != nullptr && !tcb->isIdle) {
            updateThreadStack(tcb);
        }
    }

    for (objectMeta* current = all_objects.getHead(); current != nullptr; current = current->next) {
        if (current->marked && current->forwarded) {
            updateObjectReferences(current);
        }
    }
}
void CopyingCollector::updateObjectReferences(objectMeta* objMeta) {
    using namespace gheith;
    // Assuming each object has a known layout of fields which are pointers
    objMeta->addr = objMeta->newAddr;
    objMeta->newAddr = nullptr;
}
void CopyingCollector::cleanUpFromSpace() {
    using namespace gheith;



    for (objectMeta* current = all_objects.getHead(); current != nullptr; current = current->next) {
        if (current->marked ) {
            // uint32_t* toDel = (uint32_t*) current->addr;
             Debug::printf("cleaning \n");
            // //delete toDel;
            //free(current->addr);
        }
    }
 

    // Reset avail pointer to the start of from-space
}







using namespace gheith;
void init_get_potential_children(objectMeta *parent) {
    uintptr_t *potentialPointer = (uintptr_t *)parent->addr;
    uintptr_t *end = (uintptr_t *)((char *)parent->addr + parent->size);

    objectMeta *last_child = nullptr;

    while (potentialPointer < end) {
        uintptr_t possibleAddr = *potentialPointer; // Dereference potentialPointer to check its content as an address

        // Check if the address falls within any of the managed object areas (heap, data, BSS)
        if ((possibleAddr >= (uintptr_t)gheith::array && possibleAddr < (uintptr_t)gheith::array + gheith::len * sizeof(int)) ||
            (possibleAddr >= (uintptr_t)&data_start && possibleAddr < (uintptr_t)&data_end) ||
            (possibleAddr >= (uintptr_t)&bss_start && possibleAddr < (uintptr_t)&bss_end)) {
            
            objectMeta *childMeta = all_objects.find(possibleAddr);
            if (childMeta) {
                // Debug::printf("finding children..\n");
                if (parent->child_next == nullptr) {
                    parent->child_next = childMeta; // First child
                    last_child = childMeta;
                } else {
                    if (last_child != nullptr) {
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

    void *p = ccmalloc(size, fromSpace); // ptr to data
    if (p == 0)
        Debug::panic("out of memory");
    // objMeta* metadata = new objMeta(p, size, false, theLock);

    if (GC)
    {                                                              // heapInit has been called
        objMeta *metadata = (objMeta *)ccmalloc(sizeof(objectMeta), fromSpace); // Dynamically allocate a new wrapper
        metadata->addr = p;
        metadata->marked = false;
        metadata->size = size;
        metadata->theLock = theLock;
        metadata->child_next = nullptr;
        

        init_get_potential_children(metadata);
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
    void *p = ccmalloc(size, fromSpace); // ptr to data
    if (p == 0)
        Debug::panic("out of memory");
            if (GC)
    {                                                              // heapInit has been called
        objMeta *metadata = (objMeta *)ccmalloc(sizeof(objectMeta), fromSpace); // Dynamically allocate a new wrapper

    metadata->addr = p;
    metadata->marked = false;
    metadata->size = size;
    metadata->theLock = theLock;
    metadata->child_next = nullptr;

    init_get_potential_children(metadata);
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
