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




    int *array; // a "free" list
    int len;
    int safe = 0;
    static int avail = 0; // head of free list
    static BlockingLock *theLock = nullptr;

    uint32_t *fromSpace;
   uint32_t *toSpace;
   uint32_t *toSpaceFree;
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

     void printHeapLayout()
   {
       using namespace gheith;
       Debug::printf("Heap Layout at Initialization:\n");
       Debug::printf("Total heap size: %d bytes\n", totalHeapSize);
       Debug::printf("fromSpace: starts at %p, ends at %p, size: %lu bytes\n",
                     fromSpace, (char *)fromSpace + halfHeapSize - 1, halfHeapSize);
       Debug::printf("toSpace: starts at %p, ends at %p, size: %lu bytes\n",
                     toSpace, (char *)toSpace + halfHeapSize - 1, halfHeapSize);
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


    theLock = new BlockingLock();
    GC = new CopyingCollector(base, bytes); // for now. instantiate mark and sweep GC

    memoryTracker = 0;
    totalHeapSize = len * sizeof(int); // change this

    // maybe change to mallocs to avoid new ObjectAlloc's?
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

void *gcMalloc(size_t bytes)
{
   using namespace gheith;


   if (bytes == 0)
   {
       return nullptr; // Return nullptr for zero-byte requests.
   }


   // Calculate number of int-sized units needed, including metadata.
   int ints = ((bytes + sizeof(int) - 1) / sizeof(int)) + 2;
   if (ints < 4)
   {
       ints = 4; // Enforce a minimum block size to handle metadata.
   }


   LockGuardP g{theLock}; // Acquire the lock to protect heap structures.


   void *res = nullptr;
   int mx = 0x7FFFFFFF;
   int it = 0;


   if (fromSpace < toSpace)
   {
       // Iterate over available blocks to find a suitable one.
       for (int p = avail; p != 0 && (uint32_t *)&array[p] < toSpace; p = next(p))
       {
           if (!isAvail(p))
           {
               Debug::panic("block is not available in malloc %p\n", p);
           }
           int sz = size(p);
           if (sz >= ints && sz < mx)
           {
               mx = sz;
               it = p;
               if (sz == ints)
                   break; // Perfect fit found, no need to search further.
           }
       }


       if (it != 0)
       {
           remove(it);
           int extra = mx - ints;
           if (extra >= 4)
           {
               makeTaken(it, ints);
               makeAvail(it + ints, extra); // Split the block if there's enough space left.
           }
           else
           {
               makeTaken(it, mx); // Use the entire block.
           }
           res = &array[it + 1];
           adjustMemoryTracker(ints * sizeof(int));
           justAllocated += ints * sizeof(int);
           return res;
       }
       else
       {
           Debug::printf("Out of memory HERE\n");
           return nullptr; // No suitable block found, report out of memory.
       }
   }
   else
   {
       for (int p = (int)toSpace; p != 0 && (uint32_t *)&array[p] < fromSpace + halfHeapSize; p = next(p))
       {
           if (!isAvail(p))
           {
               Debug::panic("block is not available in malloc %p\n", p);
           }
           int sz = size(p);
           if (sz >= ints && sz < mx)
           {
               mx = sz;
               it = p;
               if (sz == ints)
                   break; // Perfect fit found, no need to search further.
           }
       }


       if (it != 0)
       {
           remove(it);
           int extra = mx - ints;
           if (extra >= 4)
           {
               makeTaken(it, ints);
               makeAvail(it + ints, extra); // Split the block if there's enough space left.
           }
           else
           {
               makeTaken(it, mx); // Use the entire block.
           }
           res = &array[it + 1];
           adjustMemoryTracker(ints * sizeof(int));
           justAllocated += ints * sizeof(int);
           return res;
       }
       else
       {
           Debug::printf("Out of memory HERE\n");
           return nullptr; // No suitable block found, report out of memory.
       }
   }
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

uint32_t *CopyingCollector::getForwardingAddress(uint32_t *oldAddress)
{
   return (uint32_t *)*oldAddress;
}


bool CopyingCollector::isForwarded(uint32_t *address)
{
   return (*address & 1) != 0;
}


void CopyingCollector::setForwardingAddress(uint32_t *oldAddress, uint32_t *newAddress)
{
   // Assumes the lowest bit is used to indicate forwarding
   *oldAddress = reinterpret_cast<uint32_t>(newAddress) | 1;
}


// Getter methods
uint32_t *CopyingCollector::getFromSpace() { return gheith::fromSpace; }
uint32_t *CopyingCollector::getToSpace() { return gheith::toSpace; }
uint32_t *CopyingCollector::getToSpaceFree() { return gheith::toSpaceFree; }
size_t CopyingCollector::getHeapSize() { return gheith::halfHeapSize; }


void CopyingCollector::flip()
{
   sweep();
   using namespace gheith;
   uint32_t *temp = fromSpace;
   fromSpace = toSpace;
   toSpace = temp;
   toSpaceFree = toSpace;
}


void CopyingCollector::markBlock(void *ptr)
{
   using namespace gheith;
   // Check the bounds to ensure 'ptr' points within the managed array space
   if ( (ptr >= fromSpace && ptr < toSpace)|| (ptr <= fromSpace && ptr > toSpace))
   {
       // Calculate index to see if the pointer is pointing to a valid object start
       uintptr_t index = ((uintptr_t)ptr - (uintptr_t)gheith::array) / sizeof(int) - 1; //-1?


       // Check if the index is within bounds and the slot is marked as taken
       if (gheith::isTaken(index))
       {


           // Find the metadata for the object at the pointer address
           objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
           if (meta)
               init_get_potential_children(meta); // times out


           if (meta && !meta->marked) // Check if metadata exists and object is not already marked
           {
               // Debug::printf("found a match %x\n", ptr);


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

            
            if(addr != nullptr ){ 
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

void CopyingCollector::copy() {
  using namespace gheith;
  for (objectMeta *current = all_objects.getHead(); current != nullptr; current = current->next) {
      if (current->marked && !current->forwarded) {
          size_t sizeInBytes = current->size;
          void *newLocation = gcMalloc(sizeInBytes);
          if (!newLocation) {
              Debug::panic("Out of memory during GC copy phase");
          }
          current->newAddr = (uint32_t *)newLocation;
          current->forwarded = true;
          setForwardingAddress((uint32_t *)current->addr, (uint32_t *)newLocation);
      }
  }
}


void CopyingCollector::updateInternalReferences(objectMeta *meta, uint32_t *newLocation)
{
  // Assuming each object contains a set of pointers (or similar structures that need updating)
  using namespace gheith;
  uintptr_t *ptr = (uintptr_t *)(meta->addr);
  for (size_t i = 0; i < meta->size; ++i)
  {
      uintptr_t possibleAddr = *ptr;
      if (possibleAddr >= (uintptr_t)fromSpace && possibleAddr < (uintptr_t)fromSpace + halfHeapSize)
      {
          // This is a heap pointer and should be updated
          objectMeta *childMeta = gheith::all_objects.find(possibleAddr);
          if (childMeta && childMeta->forwarded)
          {
              *ptr = (uintptr_t)(childMeta->newAddr); // Update to new location
          }
      }
      ++ptr;
  }
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

    void *p = gcMalloc(size); // ptr to data
    if (p == 0)
        Debug::panic("out of memory");
    // objMeta* metadata = new objMeta(p, size, false, theLock);

    if (GC)
    {                                                              // heapInit has been called
        objMeta *metadata = (objMeta *)gcMalloc(sizeof(objectMeta)); // Dynamically allocate a new wrapper
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
    void *p = gcMalloc(size);
    if (p == 0)
        Debug::panic("out of memory");
            if (GC)
    {                                                              // heapInit has been called
    objMeta *metadata = (objMeta *)gcMalloc(sizeof(objectMeta)); // Dynamically allocate a new wrapper

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