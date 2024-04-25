#include "debug.h"
#include "smp.h"
#include "debug.h"
#include "config.h"
#include "machine.h"
#include "ext2.h"
#include "shared.h"
#include "threads.h"
#include "vmm.h"
#include "process.h"
#include "GarbageCollector/CopyingCollector.h"
#include "tss.h"
#include "globals.h"

namespace gheith
{
    Atomic<uint32_t> TCB::next_id{0};

    TCB **activeThreads;
    TCB **idleThreads;

    Queue<TCB, InterruptSafeLock> readyQ{};
    Queue<TCB, InterruptSafeLock> zombies{};

    Atomic<bool> worldStopped{false};
    Queue<TCB, InterruptSafeLock> waitQ{};

    extern CopyingCollector *GC;
    extern int *array;
    extern int len;
    extern void printHeap();
    extern void printMarks();

    TCB *current()
    {
        auto was = Interrupts::disable();
        TCB *out = activeThreads[SMP::me()];
        Interrupts::restore(was);
        return out;
    }

    void entry()
    {
        auto me = current();
        vmm_on((uint32_t)me->process->pd);
        sti();
        me->doYourThing();
        stop();
    }

    void delete_zombies()
    {
        while (true)
        {
            auto it = zombies.remove();
            if (it == nullptr)
                return;
            delete it;
        }
    }

    void schedule(TCB *tcb)
    {
        if (!tcb->isIdle)
        {
            if (worldStopped)
                waitQ.add(tcb);
            else
                readyQ.add(tcb);
        }//else waitQ.add(tcb);
    }

    struct IdleTcb : public TCB
    {
        IdleTcb() : TCB(Process::kernelProcess, true) {}
        void doYourThing() override
        {
            Debug::panic("should not call this");
        }
        uint32_t interruptEsp() override
        {
            // idle threads never enter user mode, this should be ok
            return 0;
        }
    };

    TCB::TCB(Process* process, bool isIdle) : isIdle(isIdle), id(next_id.fetch_add(1)), process{process}
    {
        saveArea.tcb = this;
        saveArea.cr3 = (uint32_t)process->pd;
    }

    TCB::~TCB()
    {
    }

    // TCBWithStack
    TCBWithStack::TCBWithStack(Process* process) : TCB(process, false)
    {
        stack[STACK_WORDS - 2] = 0x200; // EFLAGS: IF
        stack[STACK_WORDS - 1] = (uint32_t)entry;
        saveArea.no_preempt = 0;
        saveArea.esp = (uint32_t)&stack[STACK_WORDS - 2];
    }

    TCBWithStack::~TCBWithStack()
    {
        if (stack)
        {
            delete[] stack;
            stack = nullptr;
        }
    }

};
namespace GC{
    using namespace gheith;
    void stopWorld()
    {
       bool was = Interrupts::disable();

        worldStopped.set(true);

        TCB *tcb = readyQ.remove();
        while (tcb != nullptr)
        {
            waitQ.add(tcb);
            tcb = readyQ.remove();
        }
        Interrupts::restore(was);
    }
    void resumeWorld()
    {
        bool was = Interrupts::disable();

        worldStopped.set(false);

        TCB *tcb = waitQ.remove();
        while (tcb != nullptr)
        {
            schedule(tcb);
            tcb = waitQ.remove();
        }
        Interrupts::restore(was);
    }

//     void markPhase() {

//         for (uint32_t i = 0; i < kConfig.totalProcs; i++) {
        
//         //our world is stopped
//             TCBWithStack *tcb = (TCBWithStack *)activeThreads[i];


//             if (tcb != nullptr && !tcb->isIdle) {
//                 uint32_t **stackStart = (uint32_t**)tcb->stack;
//                 uint32_t *stackEnd = (uint32_t*)&stackStart[STACK_WORDS]; // Calculating the end of the stack

//             // Scanning from the start to the end of the stack
//                 for (uint32_t **ptr = stackStart; ptr < (uint32_t**) stackEnd; ptr++) {
//                     uint32_t candidate = (uint32_t)*ptr; // Dereferencing to get the potential pointer

//                 // Checking if the candidate pointer points inside the heap
//                     if ((void *)candidate >= gheith::array && (void *)candidate < gheith::array + gheith::len * sizeof(int)) {
//                         Debug::printf("s %x\n", candidate);
//                         gheith::GC->markBlock((void*)candidate);

//                     }
//                 }
//             }else if(tcb->isIdle){
//                 all_objects.find((uintptr_t)tcb)->marked =true;
//             }

//     }

// }

  void markThreadStack(TCBWithStack *tcb)
    {
        uint32_t **stackStart = (uint32_t **)tcb->stack;
        uint32_t *stackEnd = (uint32_t *)&stackStart[STACK_WORDS];

        // Scanning from the start to the end of the stack
        for (uint32_t **ptr = stackStart; ptr < (uint32_t **)stackEnd; ptr++)
        {
            uint32_t candidate = (uint32_t)*ptr; // Dereferencing to get the potential pointer

            // Checking if the candidate pointer points inside the heap
            if ((void *)candidate >= gheith::array && (void *)candidate < gheith::array + gheith::len * sizeof(int))
            {
                gheith::GC->markBlock((void *)candidate);
            }
        }

        // //Additionally mark using ESP from TSS if applicable
        // uint32_t esp = tss->esp0;
        // if (esp >= (uint32_t)stackStart && esp < (uint32_t)stackEnd)
        // {
        //     uint32_t *espPtr = (uint32_t *)esp;
        //     for (uint32_t *ptr = espPtr; ptr < stackEnd; ptr++)
        //     {
        //         uint32_t candidate = *ptr;
        //         if ((void *)candidate >= gheith::array && (void *)candidate < gheith::array + gheith::len * sizeof(int))
        //         {
        //             gheith::GC->markBlock((void *)candidate);
        //         }
        //     }
        // }
    }

    void markPhase()
    {
        // Loop through all processors to check stacks of both active and waiting threads
        for (uint32_t i = 0; i < kConfig.totalProcs; i++)
        {
            TCBWithStack *tcb = (TCBWithStack *)activeThreads[i];
            if (tcb != nullptr && !tcb->isIdle)
            {
                //(;
                markThreadStack(tcb);
            }

            // Additionally, check waiting queue if world is stopped
            // if (worldStopped)
            // {
            //     auto waitTCB = waitQ.head();
            //     while (waitTCB != nullptr)
            //     {
            //         if (waitTCB != tcb)
            //         { // Avoid marking the same thread twice if it's also active
            //             markThreadStack((TCBWithStack *)(waitTCB));
            //         }
            //         waitTCB = waitTCB->next;
            //     }
            // }
        }

            uint32_t* dataStart = (uint32_t*)&data_start;
            uint32_t* dataEnd = (uint32_t*)&data_end;
            uint32_t* bssStart = (uint32_t*)&bss_start;
            uint32_t* bssEnd = (uint32_t*)&bss_end;
            
            while(dataStart < dataEnd){
                if(dataStart != 0){
                    gheith::GC->markBlock((void *)dataStart);
                }
                dataStart++;
            }
            while(bssStart < bssEnd){
                if(bssStart != 0){
                    gheith::GC->markBlock((void *)bssStart);
                } 
                bssStart++;

            }
    }

  
};
void threadsInit()
{
    using namespace gheith;
    activeThreads = new TCB *[kConfig.totalProcs]();
    idleThreads = new TCB *[kConfig.totalProcs]();

    // swiched to using idle threads in order to discuss in class
    for (unsigned i = 0; i < kConfig.totalProcs; i++)
    {
        idleThreads[i] = new IdleTcb();
        activeThreads[i] = idleThreads[i];
    }

    // The reaper
    thread(Process::kernelProcess, []
           {
        //Debug::printf("| starting reaper\n");
        while (true) {
            ASSERT(!Interrupts::isDisabled());
            delete_zombies();
            yield();
            
        } });

    // GC
    thread(Process::kernelProcess, []
          {
       using namespace GC;
       //Debug::printf("| starting GC\n");
       while (true) {
               yield();
               stopWorld();


               Debug::printf("WE HAVE STOPPED THE WORLD\n");



               markPhase();
               Debug::printf("LEFT MARK PHASE\n");

               gheith::GC->copy();
               Debug::printf("LEFT copy PHASE\n");

               gheith::GC->flip();
               Debug::printf("LEFT flip PHASE\n");
               uint32_t* small = gheith::GC->getFromSpace();
               uint32_t* big = gheith::GC->getFromSpace() + gheith::GC->getHeapSize() / sizeof(uint32_t) - 1;
               Debug::printf("THE FROM SPACE STARTS AT %d\n", small);
               Debug::printf("THE FROM SPACE ENDS AT %d\n", big);
               Debug::printf("THE FROM SPACE SIZE IS %d\n",  (uint32_t)big - (uint32_t)small  );




               small = gheith::GC->getToSpace();
               big = gheith::GC->getToSpace() + gheith::GC->getHeapSize() / sizeof(uint32_t) - 1;
               Debug::printf("THE TO SPACE STARTS AT %d\n",small );
               Debug::printf("THE TO SPACE ENDS AT %d\n", big);
               Debug::printf("THE TO SPACE SIZE IS %d\n",  (uint32_t)big - (uint32_t)small );


               Debug::printf("THE HEAP SIZE IS %d\n", gheith::GC->getHeapSize() * 2);
               resumeWorld();
       } });

}

void yield()
{
    using namespace gheith;
    block(BlockOption::CanReturn, [](TCB *me)
          { schedule(me); });
}

void stop()
{
    using namespace gheith;

    while (true)
    {
        block(BlockOption::MustBlock, [](TCB *me)
              {
            if (!me->isIdle) {
                zombies.add(me);
            } });
        ASSERT(current()->isIdle);
    }
}