#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include "blocking_lock.h"

template <typename T>
struct NodeTwo
{
    T data;
    NodeTwo *next;

    NodeTwo(const T &data, NodeTwo *next = nullptr) : data(data), next(next) {}
};

template <typename T>
class LinkedList
{
    NodeTwo<T> *head;
    NodeTwo<T> *end;
    BlockingLock lock;

public:
    LinkedList() : head(nullptr), end(nullptr) {}

    ~LinkedList()
    {
        lock.lock();
        NodeTwo<T> *curr = head;
        while (curr)
        {
            NodeTwo<T> *next = curr->next;
            delete curr;
            curr = next;
        }
        head = nullptr;
        end = nullptr;
        lock.unlock();
    }

    void append(const T &val)
    {
        lock.lock();
        NodeTwo<T> *newNode = new NodeTwo<T>(val);
        if (end)
        {
            end->next = newNode;
        }
        else
        {
            head = newNode;
        }
        end = newNode;
        lock.unlock();
    }

    bool remove(const T &val)
    {
        if (isEmpty())
        {
            return false;
        }

        lock.lock();
        NodeTwo<T> *curr = head;
        NodeTwo<T> *prev = nullptr;

        while (curr)
        {
            if (curr->data == val)
            {
                if (prev)
                {
                    prev->next = curr->next;
                }
                else
                {
                    head = curr->next;
                }

                if (curr == end)
                {
                    end = prev;
                }

                delete curr;
                lock.unlock();
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        lock.unlock();
        return false;
    }

    bool contains(const T &val)
    {
        return find(val) != nullptr;
    }

    NodeTwo<T> *find(const T &val)
    {
        NodeTwo<T> *curr = head;
        while (curr)
        {
            if (curr->data == val)
            {
                return curr;
            }
            curr = curr->next;
        }
        return nullptr;
    }

    bool isEmpty()
    {
        return head == nullptr;
    }

    NodeTwo<T> *getHead()
    {
        return head;
    }
    NodeTwo<T> *getEnd()
    {
        return end;
    }
};

#endif
