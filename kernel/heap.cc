#include "heap.h"
#include "debug.h"
#include "stdint.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "GarbageCollector/MarkAndSweep.h"
#include "LinkedList.h"
/* A first-fit heap */

namespace gheith
{


    MarkAndSweep *GC = nullptr;

//     //LL
//template <typename T>
// struct Node {
//         T* data;
//         Node<T>* next;

//         Node(T* data, Node<T>* next = nullptr) : data(data), next(next) {}
//     };


// template <typename T, typename LockType>
// class LinkedList {

//     Node<T>*  head = nullptr;
//     Node<T>*  tail = nullptr;
//     LockType lock;

// public:
//     LinkedList() : head(nullptr), tail(nullptr), lock() {}

//     void monitor_add() {
//         monitor((uintptr_t)&tail);
//     }

//     void monitor_remove() {
//         monitor((uintptr_t)&head);
//     }

//     void append(T* t) {
//         LockGuard g{lock};
//         if(GC){
//             Debug::printf("%x\n", GC);
//             Node<T>* newNode = new Node(t);
//         if (head == nullptr) {
//             head = newNode;
//         } else {
//             tail->next = newNode;
//         }
//         tail = newNode;
//         }
//     }
    

//     T* remove() {
//         LockGuard g{lock};
//         if (head == nullptr) {
//             return nullptr;
//         }
//         Node<T>* nodeToRemove = head;
//         head = head->next;
//         if (head == nullptr) {
//             tail = nullptr;
//         }
//         T* data = nodeToRemove->data;
//         delete nodeToRemove;
//         return data;
//     }

//     T* remove(T* target) {
//         LockGuard g{lock};
//         if (head == nullptr) {
//             return nullptr;
//         }
//         if (head->data == target) {
//             return remove(); // Reuse remove() to handle head case
//         }

//         Node<T>* prev = head;
//         Node<T>* curr = head->next;
//         while (curr != nullptr) {
//             if (curr->data == target) {
//                 prev->next = curr->next;
//                 if (tail == curr) {
//                     tail = prev;
//                 }
//                 T* data = curr->data;
//                 delete curr;
//                 return data;
//             }
//             prev = curr;
//             curr = curr->next;
//         }
//         return nullptr;
//     }

//     T* removeAll() {
//         LockGuard g{lock};
//         Node<T>* current = head;
//         while (current != nullptr) {
//             Node<T>* next = current->next;
//             //delete current;
//             current = next;
//         }
//         head = nullptr;
//         tail = nullptr;
//         return nullptr; // No data returned as all are deleted
//     }

//     T* find(uintptr_t addr) {
//         Node<T>* current = head;
//         while (current != nullptr) {
//             if ((uintptr_t)(current->data) == addr) {
//                 return current->data;
//             }
//             current = current->next;
//         }
//         return nullptr;
//     }

//     bool isEmpty() {
//         return head == nullptr;
//     }

//     Node<T>* getHead() {
//         return head ? head : nullptr;
//     }

//     void setHead(T* newHead) {
//         LockGuard g{lock};
//         if (head == nullptr) {
//             head = new Node(newHead);
//             tail = head;  // Ensure tail points correctly if list was empty
//         } else {
//             Debug::panic("tried to set head while already head\n");
//         }
//     }
// };

    namespace object_metadata
    {
        typedef struct objectMeta
        {
            void *addr; // from malloc()'s call
            size_t size;
            bool marked;                         // false initially (in new)
            BlockingLock *theLock;               // using the heap's lock
            //Queue<objectMeta, BlockingLock> q{}; // child references

            // queue stuff
            objectMeta *next; // next in all_objects

            // objectMeta* first_children; //all children
            objectMeta* child_next;
            //objectMeta *child;
            //LinkedList<objectMeta, NoLock> children;

        } objMeta;
    };
    using namespace object_metadata;
    int *array; // a "free" list
    int len;
    int safe = 0;
    static int avail = 0; // head of free list
    static BlockingLock *theLock = nullptr;

    //static Queue<objMeta, NoLock> all_objects{}; // no lock for now
    static LinkedList<objMeta, NoLock> all_objects{};

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

    // get references of parent here, and add it to children linked list
    //     void init_get_potential_children(objectMeta* parent) {
    //         uintptr_t* potentialPointer = (uintptr_t*) parent->addr;
    //         uintptr_t* end = (uintptr_t*)((char*)parent->addr + parent->size);
    //         objectMeta* last_child = nullptr;

    //         while (potentialPointer < end) {
    //             uintptr_t possibleAddr = (uintptr_t) potentialPointer; //*?
    //             objectMeta* childMeta = all_objects.find(possibleAddr);
    //             if (childMeta) {
    //                 if (parent->first_children == nullptr) {
    //                     parent->first_children = childMeta; // First child
    //                     last_child = childMeta;
    //                 } else {
    //                     last_child->child_next = childMeta; // Append new child
    //                     last_child = childMeta;
    //                 }
    //             }
    //         potentialPointer++;
    //     }
    // }
    void init_get_potential_children(objectMeta *parent)
    {
        uintptr_t *potentialPointer = (uintptr_t *)parent->addr;
        uintptr_t *end = (uintptr_t *)((char *)parent->addr + parent->size);

        while (potentialPointer < end)
        {
            uintptr_t possibleAddr = *potentialPointer; // Dereference potentialPointer to check its content as an address
            objectMeta *childMeta = all_objects.find(possibleAddr);
            if (childMeta)
            {
                // parent->children.append(childMeta);
                if(parent->child_next == nullptr){
                    parent->child_next = childMeta;
                }else{
                    childMeta->child_next = parent->child_next;
                    parent->child_next = childMeta;
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
    theLock = new BlockingLock();
    GC = new MarkAndSweep(base, bytes); // for now. instantiate mark and sweep GC
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
        adjustMemoryTracker(bytes);
        justAllocated+=bytes;
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

    int blockSize = size(idx);
    adjustMemoryTracker(-blockSize * sizeof(int));

    //    Debug::printf("THIS IS THE AMOUNT OF MEMORY ALLOCATED %d\n", memoryTracker);
    //     Debug::printf("THIS IS THE AMOUNT OF MEMORY FREE %d\n", getAvailableMemory());

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
// void markChildren(objectMeta* parent){
//     objectMeta* child = parent->first_children;
//     while(child != nullptr){
//         if(!child->marked){
//             child->marked = true;
//             markChildren(child); //recursively mark children; DFS
//         }
//     }
// }
void markChildren(gheith::objectMeta *parent)
{
    using namespace gheith;
    // for (gheith::objectMeta *child = parent->child; child != nullptr; child = child->next)
    // {
    //     if (!child->marked)
    //     {
    //         child->marked = true;
    //         Debug::printf("fouhdn children as well, parent: %x\n", parent);
    //         markChildren(child); // Recursively mark children
    //     }
    // }
    
    // LinkedList<objectMeta, NoLock> children = parent->children;
    // Node<objectMeta>* head = children.getHead();
    // while(head != nullptr){
    //     if(!head->data->marked){
    //         head->data->marked = true;
    //         Debug::printf("fouhdn children as well. parent: %x, child: %x\n", parent->addr, head->data->addr);
    //         //markChildren(head->data);
    //     }
    // }
    objectMeta* child = parent->child_next;
    Debug::printf("chi\n");
    while(child != nullptr){
        if(!child->marked){
            child->marked = true;
            Debug::printf("fouhdn children as well. parent: %x, child: %x\n", parent->addr, child->addr);
            markChildren(child);
            child = child->child_next;
        }
    }

}
void MarkAndSweep::markBlock(void *ptr)
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
        uintptr_t index = ((uintptr_t)ptr - (uintptr_t)gheith::array) / sizeof(int) -1 ; //-1?

        // Check if the index is within bounds and the slot is marked as taken
        if (gheith::isTaken(index))
        {   
                Debug::printf("found a match %x\n", ptr);

            // Find the metadata for the object at the pointer address
            gheith::objectMeta *meta = gheith::all_objects.find((uintptr_t)ptr);
            if (meta && !meta->marked) // Check if metadata exists and object is not already marked
            {

                meta->marked = true; // Mark the object as reachable
                markChildren(meta);  // Recursively mark all reachable children
            }
        }
    }
}

void MarkAndSweep::sweep()
{
    gheith::objectMeta *current = gheith::all_objects.getHead();
    gheith::objectMeta *prev = nullptr;
    while (current != nullptr)
    {
        if (!current->marked)
        {
            // Object not marked: it's unreachable, so free it
            gheith::objectMeta *toDelete = current;
            void *addr = current->addr; // Save address to free

            // Advance the list before removing the current node
            current = current->next;

            // Remove from the queue
            if (prev != nullptr)
            {
                Debug::printf("to remove: %x\n", addr);
                prev->next = current; // Bypass the deleted node
                if(prev->next)
                Debug::printf("removed %x. prev next is %x\n", addr, prev->next->addr);
                else
                Debug::printf("removed %x. prev next is null\n", addr);
            }
            else
            {
                Debug::printf("to remove: %x\n", addr);
                Debug::printf("removed %x\n",gheith::all_objects.remove(toDelete)); // Update head if the first element is being removed
                
            }
            //Debug::printf("deleting.. %x\n",addr );
            // Free the actual object memory
            free(addr);
            // Free the metadata
            free(toDelete);
        }
        else
        {
            // Object was marked: unmark for next GC cycle
            current->marked = false;
            prev = current;          // Update prev only if not deleting the current node
            current = current->next; // Move to the next node
        }
    }
}

using namespace gheith;

// every dynamically allocated object has to go through here
void *operator new(size_t size)
{

    void *p = malloc(size); // ptr to data
    if (p == 0)
        Debug::panic("out of memory");
    // objMeta* metadata = new objMeta(p, size, false, theLock);

    if (GC)
    {                                                              // heapInit has been called
        objMeta *metadata = (objMeta *)malloc(sizeof(objectMeta)); // Dynamically allocate a new wrapper

        metadata->addr = p;
        metadata->marked = false;
        metadata->size = size;
        metadata->theLock = theLock;

        init_get_potential_children(metadata);
        all_objects.append(metadata);


        // Debug::printf("\n___starting meta data traversal__\n");
        // while (head != nullptr)
        // {
        //     Debug::printf("addr %x\n", head->addr);
        //     head = head->next;
        // }
        // Debug::printf("___ending meta data traversal__\n\n");
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