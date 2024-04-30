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



// }

  void markThreadStack(TCBWithStack *tcb)
    {
        uint32_t **stackStart = (uint32_t **)tcb->stack;
        uint32_t *stackEnd = (uint32_t *)&stackStart[STACK_WORDS];

        for (uint32_t **ptr = stackStart; ptr < (uint32_t **)stackEnd; ptr++)
        {
            uint32_t candidate = (uint32_t)*ptr; 

            if ((void *)candidate >= gheith::array && (void *)candidate < gheith::array + gheith::len * sizeof(int))
            {
                gheith::GC->markBlockCC((void *)candidate);
            }
        }

        
    }

    void markPhase()
    {
        for (uint32_t i = 0; i < kConfig.totalProcs; i++)
        {
            TCBWithStack *tcb = (TCBWithStack *)activeThreads[i];
            if (tcb != nullptr && !tcb->isIdle)
            {
                //(;
                markThreadStack(tcb);
            }

            
        }

            uint32_t* dataStart = (uint32_t*)&data_start;
            uint32_t* dataEnd = (uint32_t*)&data_end;
            uint32_t* bssStart = (uint32_t*)&bss_start;
            uint32_t* bssEnd = (uint32_t*)&bss_end;
            
            while(dataStart < dataEnd){
                if(dataStart != 0){
                    gheith::GC->markBlockCC((void *)dataStart);
                }
                dataStart++;
            }
            while(bssStart < bssEnd){
                if(bssStart != 0){
                    gheith::GC->markBlockCC((void *)bssStart);
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
              // Debug::printf("WE ARE ABOUT TO STOP THE WORLD\n");
               stopWorld();
               // Debug::printf("WE HAVE STOPPED THE WORLD\n");
          //      gheith::GC->printHeapLayout();
             //   Debug::printf("WE ARE ABOUT TO MARK\n");
               markPhase();
               //Debug::printf("WE HAVE MARKED\n");
          //     gheith::GC->printHeapLayout();
         // yield();
              // Debug::printf("WE ARE ABOUT TO COPY\n");       
               gheith::GC->copyCC();
            //   Debug::printf("WE HAVE COPIED\n");   
          //     gheith::GC->printHeapLayout();
        //      Debug::printf("WE ARE ABOUT TO FLIP\n");   
               gheith::GC->flipCC();
          //    Debug::printf("WE HAVE FLIPPED\n");
         //      gheith::GC->printHeapLayout();
            //   Debug::printf("WE ARE ABOUT TO RESUME THE WORLD\n"); 
               resumeWorld();
           //  Debug::printf("WE HAVE RESUMED THE WORLD\n");


               
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