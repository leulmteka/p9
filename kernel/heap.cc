#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "GarbageCollector/MarkAndSweep.h"
/* A first-fit heap */

namespace gheith
{
namespace object_metadata{
    typedef struct objectMeta{
    void* addr; //from malloc()'s call
    size_t size;
    bool marked; //false initially (in new)
    BlockingLock* theLock; //using the heap's lock
    Queue<objectMeta, BlockingLock> q{}; //child references

    // queue stuff
    objectMeta* next; //next in all_objects
    objectMeta* first_children; //all children
    objectMeta* child_next; 



}objMeta;
}
    using namespace object_metadata;
    int *array; // a "free" list
    int len;
    int safe = 0;
    static int avail = 0; // head of free list
    static BlockingLock *theLock = nullptr;
    static Queue<objMeta, NoLock> all_objects{}; //no lock for now
    MarkAndSweep *GC = nullptr;



    //get references of parent here, and add it to children linked list
    void init_get_potential_children(objectMeta* parent) {
        uintptr_t* potentialPointer = (uintptr_t*) parent->addr;
        uintptr_t* end = (uintptr_t*)((char*)parent->addr + parent->size);
        objectMeta* last_child = nullptr;

        while (potentialPointer < end) {
            uintptr_t possibleAddr = (uintptr_t) potentialPointer; //*?
            objectMeta* childMeta = all_objects.find(possibleAddr);
            if (childMeta) {
                if (parent->first_children == nullptr) {
                    parent->first_children = childMeta; // First child
                    last_child = childMeta;
                } else {
                    last_child->child_next = childMeta; // Append new child
                    last_child = childMeta;
                }
            }
        potentialPointer++;
    }
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
void printHeap() {
    Debug::printf("Heap Layout:\n");
    int p = 0;
    while (p < len) {
        int blockSize = abs(size(p)); 
        if (isAvail(p)) {
            Debug::printf("| Free Block at %d: Size = %d\n", p, blockSize);
        } else {
            Debug::printf("| Taken Block at %d: Size = %d\n", p, blockSize);
        }
        p += blockSize; 
    }
}
void printMarks() {
    Debug::printf("Marking Status:\n");
    for (int i = 0; i < len; i++) {
        if (isTaken(i)) {  
            if(GC->marks[i])
            Debug::printf("| Taken Block at %d: Size = %d, Marked = %s\n", i, size(i), GC->marks[i] ? "True" : "False");
        } 
    }
}

};

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
    theLock = new BlockingLock();
    GC = new MarkAndSweep(base, bytes); // for now. instantiate mark and sweep GC
    //maybe change to mallocs to avoid new ObjectAlloc's?
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
    }

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

    int idx = ((((uintptr_t)p) - ((uintptr_t)array)) / 4) - 1;
    sanity(idx);
    if (!isTaken(idx))
    {
        Debug::panic("freeing free block, p:%x idx:%d\n", (uint32_t)p, (int32_t)idx);
        return;
    }
    // GC->unmarkBlock(idx);

    int sz = size(idx);

    int leftIndex = left(idx);
    int rightIndex = right(idx);

    if (isAvail(leftIndex))
    {
        remove(leftIndex);
        idx = leftIndex;
        sz += size(leftIndex);
    }

    if (isAvail(rightIndex))
    {
        remove(rightIndex);
        sz += size(rightIndex);
    }

    makeAvail(idx, sz);

}

/*****************/
/* C++ operators */
/*****************/

void MarkAndSweep::markBlock(void *ptr)
{

    uintptr_t index = (((uintptr_t)ptr - (uintptr_t)gheith::array) / sizeof(int)) - 1; //header
    
    if (ptr >= gheith::array && ptr < gheith::array + gheith::len * sizeof(int)) //valid?
    {
        if (gheith::isTaken(index))
        {
            marks[index] = true; //reachable
        }
    }
    // Debug::printf("HEAP AFTER MARK:\n");
    // gheith::printHeap();
}
void MarkAndSweep::sweep()
{
    int i = 0;
    while (i < gheith::len)
    {
        if (gheith::isTaken(i) && !marks[i])
        {
            free(&gheith::array[i + 1]); //header + 1 is the start of data
        }
        i += gheith::size(i); // next
    }

    for (int j = 0; j < gheith::len; j++)
    {
        marks[j] = false;
    }
    // Debug::printf("HEAP AFTER SWEEP:\n");
    // gheith::printHeap();
}



using namespace gheith;


//every dynamically allocated object has to go through here
void *operator new(size_t size)
{

    void *p = malloc(size); // ptr to data
    if (p == 0)
        Debug::panic("out of memory");
    // objMeta* metadata = new objMeta(p, size, false, theLock);
   
    if(GC){  //heapInit has been called
        objMeta* metadata = (objMeta*) malloc(sizeof(objectMeta)); // Dynamically allocate a new wrapper

        metadata->addr = p;
        metadata->marked = false;
        metadata->size = size;
        metadata->theLock = theLock;

        all_objects.add(metadata);

        objMeta* head = all_objects.head();
        Debug::printf("\n___starting meta data traversal__\n");
        while(head != nullptr){
            Debug::printf("addr %x\n", head->addr);
            head = head->next;
        }
        Debug::printf("___ending meta data traversal__\n\n");
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
    void *p = malloc(size);
    if (p == 0)
        Debug::panic("out of memory");
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
