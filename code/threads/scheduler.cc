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

int BurstTimeCompare(Thread *t1, Thread *t2) {
  return t1->getBurstTime() - t2->getBurstTime();
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
  l1Queue = new SortedList<Thread *>(BurstTimeCompare);
  l2Queue = new SortedList<Thread *>(PriorityCompare);
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
  thread->setAgeTick(kernel->stats->totalTicks + 1500);

  if (thread->getPriority() < 50) {
    l3Queue->Append(thread);
    DEBUG(dbgScheduler, "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L3");
  } else if (thread->getPriority() < 100) {
    l2Queue->Insert(thread);
    DEBUG(dbgScheduler, "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L2");
  } else { // thread->getPriority() < 150
    l1Queue->Insert(thread);
    DEBUG(dbgScheduler, "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L1");
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

  if (!l1Queue->IsEmpty()) {
    return l1Queue->RemoveFront();
  }

  if (!l2Queue->IsEmpty()) {
    return l2Queue->RemoveFront();
  }

  if (!l3Queue->IsEmpty()) {
    return l3Queue->RemoveFront();
  }

  return NULL;
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

  // This is a machine-dependent assembly language routine defined
  // in switch.s.  You may have to think
  // a bit to figure out what happens after this, both from the point
  // of view of the thread and from the perspective of the "outside world".

  // nextThread start
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

void Scheduler::AgingProcess() {
  ASSERT(kernel->interrupt->getLevel() == IntOff);

  SortedList<Thread*>* newL2Queue = new SortedList<Thread*>(PriorityCompare);
  List<Thread*>* newL3Queue = new List<Thread*>;

  ListIterator<Thread*> *iterator;

  // Level 1 is ok to add priority directly
  iterator = new ListIterator<Thread*>(l1Queue);
  for (; !iterator->IsDone(); iterator->Next()) {
    Thread* t = iterator->Item();
    if (t->getAgeTick() <= kernel->stats->totalTicks) {
      if (t->getPriority() + 10 >= 150) {
        t->setPriority(149);
      } else {
        t->setPriority(t->getPriority() + 10);
      }
      t->setAgeTick(kernel->stats->totalTicks + 1500);
    }
  }
  delete iterator;

  // Level 2
  iterator = new ListIterator<Thread*>(l2Queue);
  for (; !iterator->IsDone(); iterator->Next()) {
    Thread* t = iterator->Item();
    if (t->getAgeTick() <= kernel->stats->totalTicks) {
      t->setPriority(t->getPriority() + 10);
      t->setAgeTick(kernel->stats->totalTicks + 1500);
      if (t->getPriority() >= 100) {
        l1Queue->Insert(t);
      } else {
        newL2Queue->Insert(t);
      }
    } else {
      newL2Queue->Insert(t);
    }
  }

  // Level 3
  iterator = new ListIterator<Thread*>(l3Queue);
  for (; !iterator->IsDone(); iterator->Next()) {
    Thread* t = iterator->Item();
    if (t->getAgeTick() <= kernel->stats->totalTicks) {
      t->setPriority(t->getPriority() + 10);
      t->setAgeTick(kernel->stats->totalTicks + 1500);
      if (t->getPriority() >= 50) {
        newL2Queue->Insert(t);
      } else {
        newL3Queue->Append(t);
      }
    } else {
      newL3Queue->Append(t);
    }
  }
  delete iterator;

  delete l2Queue;
  l2Queue = newL2Queue;
  delete l3Queue;
  l3Queue = newL3Queue;
}
