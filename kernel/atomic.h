#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#include "machine.h"
#include "init.h"

// Called when the code is spinning in a loop
//
// use_mwait (if true) tells us that the caller recommends that
// we use mwait. They are responsible for calling monitor
// as needed before checking the condition and calling us.
//
inline void iAmStuckInALoop(bool use_mwait) {
    if (onHypervisor) {
        // I'm going to assume it is QEMU
        // QEMU's support for pause and mwait is lousy and inconsistent
        // This gives me a place to experiment.
        if (use_mwait) {
            mwait();
        } else {
            asm volatile("pause");
        }
    } else {
        if (use_mwait) {
            mwait();
        } else {
            asm volatile("pause");
        }
    }
}


template <typename T>
class AtomicPtr {
    volatile T *ptr;
public:
    AtomicPtr() : ptr(nullptr) {}
    AtomicPtr(T *x) : ptr(x) {}
    AtomicPtr<T>& operator= (T v) {
        __atomic_store_n(ptr,v,__ATOMIC_SEQ_CST);
        return *this;
    }
    operator T () const {
        return __atomic_load_n(ptr,__ATOMIC_SEQ_CST);
    }
    T fetch_add(T inc) {
        return __atomic_fetch_add(ptr,inc,__ATOMIC_SEQ_CST);
    }
    T add_fetch(T inc) {
        return __atomic_add_fetch(ptr,inc,__ATOMIC_SEQ_CST);
    }
    void set(T inc) {
        return __atomic_store_n(ptr,inc,__ATOMIC_SEQ_CST);
    }
    T get(void) {
        return __atomic_load_n(ptr,__ATOMIC_SEQ_CST);
    }
    T exchange(T v) {
        T ret;
        __atomic_exchange(ptr,&v,&ret,__ATOMIC_SEQ_CST);
        return ret;
    }
};

template <typename T>
class Atomic {
    volatile T value;
public:
    Atomic(T x) : value(x) {}
    Atomic<T>& operator= (T v) {
        __atomic_store_n(&value,v,__ATOMIC_SEQ_CST);
        return *this;
    }
    operator T () const {
        return __atomic_load_n(&value,__ATOMIC_SEQ_CST);
    }
    T fetch_add(T inc) {
        return __atomic_fetch_add(&value,inc,__ATOMIC_SEQ_CST);
    }
    T add_fetch(T inc) {
        return __atomic_add_fetch(&value,inc,__ATOMIC_SEQ_CST);
    }
    void set(T inc) {
        return __atomic_store_n(&value,inc,__ATOMIC_SEQ_CST);
    }
    T get(void) {
        return __atomic_load_n(&value,__ATOMIC_SEQ_CST);
    }
    T exchange(T v) {
        T ret;
        __atomic_exchange(&value,&v,&ret,__ATOMIC_SEQ_CST);
        return ret;
    }
    void monitor_value() {
        monitor((uintptr_t)&value);
    }
};

template <>
class Atomic<uint64_t> {
    volatile uint64_t value;
public:
    Atomic(uint64_t x = 0) : value(x) {} // Provide default constructor with initializer

    Atomic<uint64_t>& operator= (uint64_t v) {
        __atomic_store_n(&value, v, __ATOMIC_SEQ_CST);
        return *this;
    }

    operator uint64_t () const {
        return __atomic_load_n(&value, __ATOMIC_SEQ_CST);
    }

    uint64_t fetch_add(uint64_t inc) {
        return __atomic_fetch_add(&value, inc, __ATOMIC_SEQ_CST);
    }

    uint64_t add_fetch(uint64_t inc) {
        return __atomic_add_fetch(&value, inc, __ATOMIC_SEQ_CST);
    }

    void set(uint64_t v) {
        __atomic_store_n(&value, v, __ATOMIC_SEQ_CST);
    }

    uint64_t get(void) const {
        return __atomic_load_n(&value, __ATOMIC_SEQ_CST);
    }

    uint64_t exchange(uint64_t v) {
        uint64_t ret;
        __atomic_exchange(&value, &v, &ret, __ATOMIC_SEQ_CST);
        return ret;
    }
};


template <>
class Atomic<int64_t> {
    volatile int64_t value;
public:
    Atomic(int64_t x = 0) : value(x) {} // Provide default constructor with initializer

    Atomic<int64_t>& operator= (int64_t v) {
        __atomic_store_n(&value, v, __ATOMIC_SEQ_CST);
        return *this;
    }

    operator int64_t () const {
        return __atomic_load_n(&value, __ATOMIC_SEQ_CST);
    }

    int64_t fetch_add(int64_t inc) {
        return __atomic_fetch_add(&value, inc, __ATOMIC_SEQ_CST);
    }

    int64_t add_fetch(int64_t inc) {
        return __atomic_add_fetch(&value, inc, __ATOMIC_SEQ_CST);
    }

    void set(int64_t v) {
        __atomic_store_n(&value, v, __ATOMIC_SEQ_CST);
    }

    int64_t get(void) const {
        return __atomic_load_n(&value, __ATOMIC_SEQ_CST);
    }

    int64_t exchange(int64_t v) {
        int64_t ret;
        __atomic_exchange(&value, &v, &ret, __ATOMIC_SEQ_CST);
        return ret;
    }
};


class Interrupts {
public:
    static bool isDisabled() {
        uint32_t oldFlags = getFlags();
        return (oldFlags & 0x200) == 0;
    }

    static bool disable() {
        bool wasDisabled = isDisabled();
        if (!wasDisabled)
            cli();
        return wasDisabled;
    }

    static void restore(bool wasDisabled) {
        if (!wasDisabled) {
            sti();
        }
    }

    template <typename Work>
    static inline void protect(Work work) {
        auto was = disable();
        work();
        restore(was);
    }

};

template <typename T>
class LockGuard {
    T& it;
public:
    inline LockGuard(T& it): it(it) {
        it.lock();
    }
    inline ~LockGuard() {
        it.unlock();
    }
};

template <typename T>
class LockGuardP {
    T* it;
public:
    inline LockGuardP(T* it): it(it) {
        if (it) it->lock();
    }
    inline ~LockGuardP() {
        if (it) it->unlock();
    }
};

class NoLock {
public:
    inline void lock() {}
    inline void unlock() {}
};

extern void pause();

class SpinLock {
    Atomic<bool> taken;
public:
    SpinLock() : taken(false) {}

    SpinLock(const SpinLock&) = delete;

    // for debugging, etc. Allows false positives
    bool isMine() {
        return taken.get();
    }

    void lock(void) {
        taken.monitor_value();
        while (taken.exchange(true)) {
            iAmStuckInALoop(true);
            taken.monitor_value();
        }
    }
    
    void unlock(void) {
        taken.set(false);
    }
};

// Is this correct? 
class InterruptSafeLock  {
    Atomic<bool> taken;
    volatile bool was;
public:    
    Atomic<uint32_t> ref_count;
    InterruptSafeLock() : taken(false), was(false), ref_count(0) {}

    InterruptSafeLock(const InterruptSafeLock&) = delete;

    // for debugging, etc. Allows false positives
    bool isMine() {
        return taken.get();
    }

    void lock() {
        while (true) {
            taken.monitor_value();
            bool wasDisabled = Interrupts::disable();           
            if (!taken.exchange(true)) {
                was = wasDisabled;
                return;
            }
            Interrupts::restore(wasDisabled);
            iAmStuckInALoop(true);
        }
    }

    void unlock() {
        auto wasDisabled = was;
        taken.set(false);
        Interrupts::restore(wasDisabled);
    }
};

// A more flexible InterruptSafeLock
class ISL  {
    Atomic<bool> taken;
public:    
    Atomic<uint32_t> ref_count;
    ISL() : taken(false), ref_count(0) {}

    ISL(const ISL&) = delete;
    ISL& operator=(const ISL&) const = delete;


    // for debugging, etc. Allows false positives
    bool isMine() {
        return taken.get();
    }

    bool lock() {
        while (true) {
            taken.monitor_value();
            bool wasDisabled = Interrupts::disable();           
            if (!taken.exchange(true)) {
                return wasDisabled;
            }
            Interrupts::restore(wasDisabled);
            iAmStuckInALoop(true);
        }
    }

    void unlock(bool disable) {
        taken.set(false);
        if (disable) {
            cli();
        } else {
            sti();
        }
    }
};




#endif
