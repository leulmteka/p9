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
#include "GarbageCollector/MarkAndSweep.h"
#include "tss.h"

namespace gheith
{
    Atomic<uint32_t> TCB::next_id{0};

    TCB **activeThreads;
    TCB **idleThreads;

    Queue<TCB, InterruptSafeLock> readyQ{};
    Queue<TCB, InterruptSafeLock> zombies{};

    Atomic<bool> worldStopped{false};
    Queue<TCB, InterruptSafeLock> waitQ{};

    extern MarkAndSweep *GC;
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
        }
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
//                         //Debug::printf("s %x\n", candidate);
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
                //Debug::printf("s %x\n", candidate);
                GC->markBlock((void *)candidate);
            }
        }

        // Additionally mark using ESP from TSS if applicable
        uint32_t esp = tcb->interruptEsp();
        if (esp >= (uint32_t)stackStart && esp < (uint32_t)stackEnd)
        {
            uint32_t *espPtr = (uint32_t *)esp;
            for (uint32_t *ptr = espPtr; ptr < stackEnd; ptr++)
            {
                uint32_t candidate = *ptr;
                if ((void *)candidate >= gheith::array && (void *)candidate < gheith::array + gheith::len * sizeof(int))
                {
                    GC->markBlock((void *)candidate);
                }
            }
        } 
        //5221195
    }

    void markPhase()
    {
        // Loop through all processors to check stacks of both active and waiting threads
        for (uint32_t i = 0; i < kConfig.totalProcs; i++)
        {
            TCBWithStack *tcb = (TCBWithStack *)activeThreads[i];
            if (tcb != nullptr && !tcb->isIdle)
            {
                //(tcb);
                markThreadStack((TCBWithStack *)(tcb));
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
    }

  
void sweep()
{
    
    objectMeta *current = gheith::all_objects.getHead();
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

            if(addr != nullptr && addr > (void*) 0x200314U){ free(addr);
               // Debug::printf("deleted address %x\n", addr);
            }
             //to avoid idles (fix) //
//*** Total Memory Free After The Test (heap size - mem tracker): 5200860
//*** Total Memory Still Allocated After The Test (memory tracker): 17532
//*** Total Memory Free After The Test (heap size - mem tracker): 5225348
            // // Free the metadata
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

void threadsInit()
{
    using namespace gheith;
    activeThreads = new TCB *[kConfig.totalProcs]();
    idleThreads = new TCB *[kConfig.totalProcs]();
    Debug::printf("addr of idle: %x\n", idleThreads);
    Debug::printf("addr of ready: %x\n", activeThreads);


    // swiched to using idle threads in order to discuss in class
    for (unsigned i = 0; i < kConfig.totalProcs; i++)
    {
        idleThreads[i] = new IdleTcb();
        Debug::printf("addr of idle: %x\n", idleThreads[i]);
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
    // thread(Process::kernelProcess, []
    //        {
    //     //Debug::printf("| starting GC\n");
    //     while (true) {
            
    //             yield(); //so we don't hog the CPU
    //             stopWorld();
    //             markPhase();
    //             //init_get_potential_children()

    //             sweep();
    //             resumeWorld();
    //             //Debug::printf("eax %x\n", printRegisters(1));
                
    //     } });
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
