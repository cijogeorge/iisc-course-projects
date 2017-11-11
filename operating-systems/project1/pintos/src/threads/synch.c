/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* My Code Begin */
/* If a & b are two list elements of waiters list, 
    this function returns true if priority of a < priority of b */
static bool
thread_priority_less_func_waiters (const struct list_elem *t_elem_a,
                                   const struct list_elem *t_elem_b,
                                   void *aux);

/* If a & b are two list elements of donors list, 
    this function returns true if priority of a < priority of b */
static bool
thread_priority_less_func_donors (const struct list_elem *t_elem_a,
                                  const struct list_elem *t_elem_b,
                                  void *aux);

/* If a & b are two list elements of condition waiters' list 
   (element of struct condition), this function returns true if 
   priority of a < priority of b */
static bool
thread_priority_less_func_cond_waiters (const struct list_elem *t_elem_a,
                                        const struct list_elem *t_elem_b,
                                        void *aux);

/* Function that recursively donates priority to other threads as required */
static void priority_donate_chain (struct thread *t);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();

  if (sema->value == 0) 
  {
    list_push_back (&sema->waiters, &thread_current ()->synch_elem);
    thread_block ();
  }
  
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  
  if (!list_empty (&sema->waiters))
  {
    /* Chooses the thread with maximum priority from the waiters list, and removes the 
       thread from the list */
    struct list_elem *e = list_max(&sema->waiters, thread_priority_less_func_waiters, NULL);
    struct thread *t = list_entry (e, struct thread, synch_elem);
    e = list_remove (e);
    thread_unblock (t);
  }
  
  sema->value++;
  
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  enum intr_level old_level;
  struct thread *cur = thread_current ();

  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  
  old_level = intr_disable ();
  
  /* If the lock is held by some other thread & priority of the lock holder
     is less than the priority of current thread, then initiate priority
     donation, after storing the lock in the thread structure of current
     thread and also pushing the current thread into the donors list of
     lock holder */
  if (lock->holder != NULL && lock->holder->priority < cur->priority)
  {
    cur->wait_for_lock = lock;
    list_push_back (&lock->holder->donor_threads, &cur->donor_threads_elem);
    priority_donate_chain (cur);
  }

  sema_down (&lock->semaphore);
  
  cur->wait_for_lock = NULL;
  lock->holder = cur;
  
  intr_set_level (old_level);
}

/* Function to donate priority recursively */
void
priority_donate_chain (struct thread *t)
{
  /* Donate priority recursively until no more donation is required */
  if (t->wait_for_lock != NULL)
  {
    donate_priority (t, t->wait_for_lock->holder);
    priority_donate_chain (t->wait_for_lock->holder);
  }
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  enum intr_level old_level;
  struct semaphore *sema = &lock->semaphore;
  struct thread *cur = thread_current ();

  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
  
  old_level = intr_disable ();

  /* When a thread wants to release a lock, it checks if the 
     list of donor threads is empty and if original_priority is -1. 
     If not, then it checks the waiters list of the semaphore 
     associated with the lock, & see if any thread in that list is 
     present in the donors list of the current thread. If so, such 
     threads are removed from the donors list, since their donations 
     are no more required. */
  
  if (!list_empty (&cur->donor_threads) && !list_empty (&sema->waiters) 
      && cur->original_priority != -1)
  {
    struct thread *waiter, *donor;
    struct list_elem *e, *f;
   
    for (e = list_begin (&sema->waiters); e != list_end (&sema->waiters); 
         e = list_next (e))
    {
      waiter = list_entry (e, struct thread, synch_elem);
 
      for (f = list_begin (&cur->donor_threads); f != list_end (&cur->donor_threads); 
           f = list_next (f))
      {
        donor = list_entry (f, struct thread, donor_threads_elem);

        if (waiter == donor)
          break;
      }

      f = list_remove (f);
    }
  
    /* The priority of maximum priority thread among the threads left in the 
       donors list is set as the priority of the current thread */

    if (!list_empty (&cur->donor_threads))
    {
      donor = list_entry (list_max (&cur->donor_threads, thread_priority_less_func_donors, NULL), 
                          struct thread, donor_threads_elem);
 
      if (cur-> priority != donor->priority)
        donate_priority (donor, cur);
    }
  }
  
  /* If no more donations exist, priority reset to original priority */
  if (list_empty (&cur->donor_threads) && cur->original_priority != -1)
    back_to_original_priority (cur);
  
  lock->holder = NULL;
  sema_up (&lock->semaphore);

  intr_set_level (old_level);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
    int thread_priority;                /* Priority of the holder of the lock that protects the condition. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  waiter.thread_priority = lock->holder->priority;
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  if (!list_empty (&cond->waiters))
  {
    /* Choose the maximum priority thread from &cond->waiters list and removes it from the list */
    struct list_elem *e = list_max (&cond->waiters, thread_priority_less_func_cond_waiters, NULL); 
    struct semaphore_elem *t = list_entry (e, struct semaphore_elem, elem);
    e = list_remove (e);
    sema_up (&t->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}


/* If a & b are two list elements of waiters list, 
   this function returns true if priority of a < priority of b */
bool
thread_priority_less_func_waiters (const struct list_elem *t_elem_a,
                                   const struct list_elem *t_elem_b,
                                   void *aux UNUSED)
{
  struct thread *thread_a = list_entry (t_elem_a, struct thread, synch_elem);
  struct thread *thread_b = list_entry (t_elem_b, struct thread, synch_elem);
  return (thread_a->priority < thread_b->priority);
}

/* If a & b are two list elements of donor_threads list, 
   this function returns true if priority of a < priority of b */
bool
thread_priority_less_func_donors (const struct list_elem *t_elem_a,
                                  const struct list_elem *t_elem_b,
                                  void *aux UNUSED)
{
  struct thread *thread_a = list_entry (t_elem_a, struct thread, donor_threads_elem);
  struct thread *thread_b = list_entry (t_elem_b, struct thread, donor_threads_elem);
  return (thread_a->priority < thread_b->priority);
}

/* If a & b are two list elements of cond waiters list, 
   this function returns true if priority of a < priority of b */
bool
thread_priority_less_func_cond_waiters (const struct list_elem *t_elem_a,
                                        const struct list_elem *t_elem_b,
                                        void *aux UNUSED)
{
  struct semaphore_elem *elem_a = list_entry (t_elem_a, struct semaphore_elem, elem);
  struct semaphore_elem *elem_b = list_entry (t_elem_b, struct semaphore_elem, elem);
  return (elem_a->thread_priority < elem_b->thread_priority);
}

