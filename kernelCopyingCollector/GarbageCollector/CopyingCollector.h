#ifndef _COPYING_COLLECTOR_H_
#define _COPYING_COLLECTOR_H_

#include "GarbageCollector.h"
#include "stdint.h"
#include "debug.h"
#include "blocking_lock.h"
#include "atomic.h"
#include "threads.h"
#include "machine.h"

class CopyingCollector : public GarbageCollector
{
private:


public:
    bool *marks;
    CopyingCollector(void *start, size_t size){}

    ~CopyingCollector() {}

    void copyCC();

    void updateInternalReferencesCC(objectMeta *meta, void *newLocation);

    void flipCC();

    void sweepCC();

    void markBlockCC(void *ptr);

    void switchFT();

    uint32_t* getFromSpace();
    uint32_t* getToSpace();
    uint32_t* getToSpaceFree();
    size_t getHeapSize();

    void setFromSpace(uint32_t* newFromSpace);
    void setToSpace(uint32_t* newToSpace);
    void setToSpaceFree(uint32_t* newToSpaceFree);
    void setHeapSize(size_t newHeapSize);

    void clearSpace(uint32_t *start, uint32_t *end);

    void *getForwardedAddress(objectMeta *meta);

    bool isForwarded(objectMeta *meta);

    void setForwardingAddress(objectMeta *meta, void *newAddress);
};

#endif