#ifndef STACK_H
#define STACK_H

#include "LinkedList.h"

template <typename T>
class Stack {
    LinkedList<T> list;
    static T defaultReturnVal;

public:
    Stack() {}

    ~Stack() {}

    void push(const T& value) {
        list.append(value);
    }

    T pop() {
        if (list.isEmpty()) {
            Debug::printf("Stack underflow\n");
            return defaultReturnVal;
        }
        
        T topVal = list.getEnd()->data;
        list.remove(topVal);
        return topVal;
    }

    T& top() {
        if (list.isEmpty()) {
            Debug::printf("Stack underflow\n");
            return defaultReturnVal;
        }

        return list.getEnd()->data;
    }

    bool isEmpty() {
        return list.isEmpty();
    }

    bool contains(const T& value) {
        return list.contains(value);
    }

    NodeTwo<T>* find(const T& value) {
        return list.find(value);
    }
};

template <typename T>
T Stack<T>::defaultReturnVal = (T)0; 

#endif
