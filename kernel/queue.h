#ifndef _queue_h_
#define _queue_h_

#include "atomic.h"
#include "debug.h"

template <typename T, typename LockType>
class Queue {
    T * volatile first = nullptr;
    T * volatile last = nullptr;
    LockType lock;
public:
    Queue() : first(nullptr), last(nullptr), lock() {}
    Queue(const Queue&) = delete;

    void monitor_add() {
        monitor((uintptr_t)&last);
    }

    void monitor_remove() {
        monitor((uintptr_t)&first);
    }

    void add(T* t) {
        LockGuard g{lock};
        t->next = nullptr;
        if (first == nullptr) {
            first = t;
        } else {
            last->next = t;
        }
        last = t;
    }

    T* remove() {
        LockGuard g{lock};
        if (first == nullptr) {
            return nullptr;
        }
        auto it = first;
        first = it->next;
        if (first == nullptr) {
            last = nullptr;
        }
        return it;
    }

    T* remove(T* target) {
        LockGuard g{lock};
        if (first == nullptr) {
            return 0;
        }
        if (first == target) {
            first = first->next;
            if (first == nullptr) {
                last = nullptr;
            }
            return target;
        }

        T* prev = first;
        T* curr = first->next;
        while (curr != nullptr) {
            if (curr == target) {
                prev->next = curr->next;
                if (last == curr) {
                    last = prev;
                }
                return curr;
            }
            prev = curr;
            curr = curr->next;
        }
        return 0;
    }

    T* remove_all() {
        LockGuard g{lock};
        auto it = first;
        first = nullptr;
        last = nullptr;
        return it;
    }


    T* head(){
        return first;
    }
    void setHead(T* head){
        if(first == nullptr){
        first = head;
        }else Debug::panic("tried to set head while already head\n");
    }


T* find(uintptr_t addr){ //addr!
    T* curr = first;
    while(curr != nullptr){
        if(curr->addr == (void*)addr){
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}
};

#endif
