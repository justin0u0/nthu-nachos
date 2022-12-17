// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "scheduler.h"

#include "copyright.h"
#include "debug.h"
#include "main.h"

int PriorityCompare(Thread *t1, Thread *t2) {
  return t2->getPriority() - t1->getPriority();
}

int PredictedRemainingBurstTimeCompare(Thread *t1, Thread *t2) {
  return t1->getPredictedRemainingBurstTime() - t2->getPredictedRemainingBurstTime();
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
  l1Queue = new SortedList<Thread*>(PredictedRemainingBurstTimeCompare);
  l2Queue = new SortedList<Thread*>(PriorityCompare);
  l3Queue = new List<Thread *>;
  toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler() {
  delete l1Queue;
  delete l2Queue;
  delete l3Queue;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread) {
  ASSERT(kernel->interrupt->getLevel() == IntOff);
  DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
  //cout << "Putting thread on ready list: " << thread->getName() << endl ;

  thread->setStatus(READY);

  thread->setLastAgeTick(kernel->stats->totalTicks);

  if (thread->getPriority() < 50) {
    l3Queue->Append(thread);
    DEBUG(dbgScheduler, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L3");
  } else if (thread->getPriority() < 100) {
    l2Queue->Insert(thread);
    DEBUG(dbgScheduler, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L2");
  } else { // thread->getPriority() < 150
    l1Queue->Insert(thread);
    DEBUG(dbgScheduler, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L1");
  }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread* Scheduler::FindNextToRun() {
  ASSERT(kernel->interrupt->getLevel() == IntOff);

  Thread* t = NULL;

  if (!t && !l1Queue->IsEmpty()) {
		DEBUG(dbgScheduler, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << l1Queue->Front()->getID() << "] is removed from queue L1");
    t = l1Queue->RemoveFront();
  }

  if (!t && !l2Queue->IsEmpty()) {
		DEBUG(dbgScheduler, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << l2Queue->Front()->getID() << "] is removed from queue L2");
    t = l2Queue->RemoveFront();
  }

  if (!t && !l3Queue->IsEmpty()) {
		DEBUG(dbgScheduler, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << l3Queue->Front()->getID() << "] is removed from queue L3");
    t = l3Queue->RemoveFront();
  }

  if (t != NULL) {
    t->setTotalWaitingTicks(t->getTotalWaitingTicks() + (kernel->stats->totalTicks - t->getLastAgeTick()));
  }
  return t;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing) {
  Thread *oldThread = kernel->currentThread;

  ASSERT(kernel->interrupt->getLevel() == IntOff);

  if (finishing) {  // mark that we need to delete current thread
    ASSERT(toBeDestroyed == NULL);
    toBeDestroyed = oldThread;
  }

  if (oldThread->space != NULL) {  // if this thread is a user program,
    oldThread->SaveUserState();    // save the user's CPU registers
    oldThread->space->SaveState();
  }

  oldThread->CheckOverflow();  // check if the old thread
                               // had an undetected stack overflow

  kernel->currentThread = nextThread;  // switch to the next thread
  nextThread->setStatus(RUNNING);      // nextThread is now running

  DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
	DEBUG(dbgScheduler, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID()
    << "] is now selected for execution, thread [" << oldThread->getID() << "] is replaced, and it has executed ["
    << oldThread->getBurstTime() + (kernel->stats->totalTicks - oldThread->getStartTick()) << "] ticks");

  // This is a machine-dependent assembly language routine defined
  // in switch.s.  You may have to think
  // a bit to figure out what happens after this, both from the point
  // of view of the thread and from the perspective of the "outside world".

  nextThread->setStartTick(kernel->stats->totalTicks);

  SWITCH(oldThread, nextThread);

  // we're back, running oldThread
  oldThread->setStartTick(kernel->stats->totalTicks);

  // interrupts are off when we return from switch!
  ASSERT(kernel->interrupt->getLevel() == IntOff);

  DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

  CheckToBeDestroyed();  // check if thread we were running
                         // before this one has finished
                         // and needs to be cleaned up

  if (oldThread->space != NULL) {   // if there is an address space
    oldThread->RestoreUserState();  // to restore, do it.
    oldThread->space->RestoreState();
  }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed() {
  if (toBeDestroyed != NULL) {
    delete toBeDestroyed;
    toBeDestroyed = NULL;
  }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print() {
  cout << "Ready list contents:\n";
  l1Queue->Apply(ThreadPrint);
  l2Queue->Apply(ThreadPrint);
  l3Queue->Apply(ThreadPrint);
}

//----------------------------------------------------------------------
// Scheduler::AgingProcess
// 	Increase priority if thread in ready list over 1500 ticks
//----------------------------------------------------------------------
void Scheduler::AgingProcess() {
  ASSERT(kernel->interrupt->getLevel() == IntOff);

  SortedList<Thread*>* newL2Queue = new SortedList<Thread*>(PriorityCompare);
  List<Thread*>* newL3Queue = new List<Thread*>;

  ListIterator<Thread*>* it;

  for (it = new ListIterator<Thread*>(l1Queue); !it->IsDone(); it->Next()) {
    Thread* t = it->Item();
    t->setTotalWaitingTicks(t->getTotalWaitingTicks() + (kernel->stats->totalTicks - t->getLastAgeTick()));
    t->setLastAgeTick(kernel->stats->totalTicks);

    if (t->getTotalWaitingTicks() >= 1500) {
      int newPriority = t->getPriority() + 10;
      if (newPriority >= 150) newPriority = 149;

      DEBUG(dbgScheduler, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" 
          << t->getID() << "] chagnes its priority from [" << t->getPriority() << "] to [" << newPriority << "]");
      t->setPriority(newPriority);
      t->setTotalWaitingTicks(t->getTotalWaitingTicks() - 1500);
    }
  }
  delete it;

  for (it = new ListIterator<Thread*>(l2Queue); !it->IsDone(); it->Next()) {
    Thread* t = it->Item();
    t->setTotalWaitingTicks(t->getTotalWaitingTicks() + (kernel->stats->totalTicks - t->getLastAgeTick()));
    t->setLastAgeTick(kernel->stats->totalTicks);

    if (t->getTotalWaitingTicks() >= 1500) {
      int newPriority = t->getPriority() + 10;

      DEBUG(dbgScheduler, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" 
          << t->getID() << "] chagnes its priority from [" << t->getPriority() << "] to [" << newPriority << "]");

      t->setPriority(newPriority);

      if (t->getPriority() >= 100) {
        l1Queue->Insert(t);
        DEBUG(dbgScheduler, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is removed from queue L2");
        DEBUG(dbgScheduler, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is inserted into queue L1");
      } else {
        newL2Queue->Insert(t);
      }

      t->setTotalWaitingTicks(t->getTotalWaitingTicks() - 1500);
    } else {
      newL2Queue->Insert(t);
    }
  }
  delete it;

  for (it = new ListIterator<Thread*>(l3Queue); !it->IsDone(); it->Next()) {
    Thread* t = it->Item();
    t->setTotalWaitingTicks(t->getTotalWaitingTicks() + (kernel->stats->totalTicks - t->getLastAgeTick()));
    t->setLastAgeTick(kernel->stats->totalTicks);

    if (t->getTotalWaitingTicks() >= 1500) {
      int newPriority = t->getPriority() + 10;

      DEBUG(dbgScheduler, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" 
          << t->getID() << "] chagnes its priority from [" << t->getPriority() << "] to [" << newPriority << "]");

      t->setPriority(newPriority);

      if (t->getPriority() >= 50) {
        newL2Queue->Insert(t);
        DEBUG(dbgScheduler, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is removed from queue L3");
        DEBUG(dbgScheduler, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is inserted into queue L2");
      } else {
        newL3Queue->Append(t);
      }

      t->setTotalWaitingTicks(t->getTotalWaitingTicks() - 1500);
    } else {
      newL3Queue->Append(t);
    }
  }
  delete it;

  delete l2Queue;
  l2Queue = newL2Queue;
  delete l3Queue;
  l3Queue = newL3Queue;
}

//----------------------------------------------------------------------
// Scheduler::CheckIfYield
// 	Return true if currentThread should be preempt
//----------------------------------------------------------------------
bool Scheduler::CheckIfYield() {
  ASSERT(kernel->interrupt->getLevel() == IntOff);

  Thread *t = kernel->currentThread;

  // Switch next thread if current thread is l2, l3, or current thread is L1 but longer burst time
  if (!l1Queue->IsEmpty()) {
    // calculate the current thread's predicted remaining burst time
    int predictedRemainingBurstTime = t->getPredictedBurstTime() - (kernel->stats->totalTicks - t->getStartTick());

    return (t->getPriority() < 100
      || (t->getPriority() >= 100 && predictedRemainingBurstTime > l1Queue->Front()->getPredictedRemainingBurstTime()));
  }

  // Switch next thread if current thread is L3
  if (!l2Queue->IsEmpty()) {
    return (t->getPriority() < 50);
  }

  // Scheduler::CheckIfYield is called in Alarm::Callback and L3 is RR algorithm,
  // if currentThread is L3 and L3 is not empty, switch
  return (t->getPriority() < 50 && !l3Queue->IsEmpty());
}
