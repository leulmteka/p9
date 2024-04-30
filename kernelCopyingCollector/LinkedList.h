#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include "atomic.h"
#include "debug.h"

template <typename T, typename LockType>
class LinkedList {
    T * volatile head = nullptr;
    T * volatile tail = nullptr;
    int size = 0; // Track the number of elements
    LockType lock;

public:
    LinkedList() : head(nullptr), tail(nullptr), size(0), lock() {}

    void monitor_add() {
        monitor((uintptr_t)&tail);
    }

    void monitor_remove() {
        monitor((uintptr_t)&head);
    }

    void append(T* t) {
        LockGuard g{lock};
        t->next = nullptr;
        if (head == nullptr) {
            head = t;
        } else {
            tail->next = t;
        }
        tail = t;
        size++;
    }

    T* remove() {
        LockGuard g{lock};
        if (head == nullptr) {
            return nullptr;
        }
        auto it = head;
        head = it->next;
        if (head == nullptr) {
            tail = nullptr;
        }
        size--;
        return it;
    }

    T* remove(T* target) {
        LockGuard g{lock};
        if (head == nullptr) {
            return nullptr;
        }
        if (head == target) {
            head = head->next;
            if (head == nullptr) {
                tail = nullptr;
            }
            return target;
        }

        T* prev = head;
        T* curr = head->next;
        while (curr != nullptr) {
            if (curr == target) {
                prev->next = curr->next;
                if (tail == curr) {
                    tail = prev;
                }
                return curr;
            }
            prev = curr;
            curr = curr->next;
        }
        return nullptr;
    }

    T* removeAll() {
        LockGuard g{lock};
        auto it = head;
        head = nullptr;
        tail = nullptr;
        return it;
    }

    T* find(uintptr_t addr) {
        T* curr = head;
        while (curr != nullptr) {
            if ((uintptr_t)(curr->addr) == addr) {
                return curr;
            }
            curr = curr->next;
        }
        return nullptr;
    }

    T* getHead() {
        LockGuard g{lock};

        return head;
    }

    void setHead(T* newHead) {
        LockGuard g{lock};
        if (head == nullptr) {
            head = newHead;
        } else {
            Debug::panic("tried to set head while already head\n");
        }
    }

    bool isEmpty() {
        LockGuard g{lock};
        return size == 0;
    }

};

#endif