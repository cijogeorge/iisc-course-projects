#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <string.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/fixed-point.h"
#include <list.h>
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

/* List of threads sleeping on timer_sleep (), implemeted as an ordered queue, 
   ordered in non-decreasing order of their wake up ticks */
static struct list sleeping_list;

/* Function to wake up sleeping threads whose time has come to wake up */
static void thread_wakeup (void);

/* If a & b are two list elements of sleeping_list, 
   this function returns true if wakeup_time of a < wakeup_time of b */
static bool thread_wakeup_time_less_func (const struct list_elem *t_elem_a,
                                          const struct list_elem *t_elem_b,
                                          void *aux);

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  /* Initialize list of sleeping threads */
  list_init (&sleeping_list);
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }
  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  enum intr_level old_level;
  struct thread *cur;

  ASSERT (intr_get_level () == INTR_ON);
  
  cur = thread_current ();
  
  /* Calculate wake up time of the thread and store it in the thread structure */
  cur->wakeup_time = timer_ticks () + ticks;

  old_level = intr_disable ();
 
  /* Insert the thread into ordered list sleeping_list */
  list_insert_ordered (&sleeping_list, &cur->sleep_elem,
		       &thread_wakeup_time_less_func, NULL);
  thread_block ();

  intr_set_level (old_level);
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;
 
  /* If sleeping_list is not empty, then it should be checked whether any thread's
     wake up time has reached. If so, such threads have to be woken up */
  if(!list_empty (&sleeping_list))
    thread_wakeup ();

  thread_tick ();
  
  /* Code for Advanced scheduler */
  if (thread_mlfqs)
  {
    /* recent_cpu is incremented by 1 everytime a timer interrupt happens,
       unless it is an idle thread */
    if (strcmp(thread_current ()->name, "idle"))
      thread_current ()->recent_cpu = x_plus_n (thread_current ()->recent_cpu, 1);
    
    /* At the end of every second, load_avg should be recalculated for the system
       and recent_cpu should be recalcualted for all the threads. */
    if (timer_ticks () % TIMER_FREQ == 0)
    {
      /* Recalculate load_avg */
      thread_calculate_load_avg ();
      
      /* Recalculate recent_cpu for each thread */
      thread_calculate_recent_cpu_all ();
    }
    
    /* Every 4th tick, priority should be recalculated for all the threads */
    if (timer_ticks () % 4 == 0)
      thread_calculate_priority_all ();
  }        
}

/* Function to wake up sleeping threads whose time has come to wake up */
static void
thread_wakeup (void)
{
  struct list_elem *t_elem;
  struct thread *sleeping_thread;
  
  /* All threads whose wak up time has reached are unblocked */
  while(!list_empty (&sleeping_list))
  {
    t_elem = list_front (&sleeping_list);
    sleeping_thread = list_entry (t_elem, struct thread, sleep_elem);
    
    if (sleeping_thread->wakeup_time <= timer_ticks())
    {
      t_elem = list_pop_front (&sleeping_list);
      thread_unblock (sleeping_thread);
    }
    else break;
  }
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}

/* If a & b are two list elements of sleeping_list, 
   this function returns true if wakeup_time of a < wakeup_time of b */
static bool
thread_wakeup_time_less_func (const struct list_elem *t_elem_a,
                              const struct list_elem *t_elem_b,
                              void *aux UNUSED)
{
  struct thread *sleeping_thread_a = list_entry (t_elem_a, struct thread, sleep_elem);
  struct thread *sleeping_thread_b = list_entry (t_elem_b, struct thread, sleep_elem);
  return (sleeping_thread_a->wakeup_time < sleeping_thread_b->wakeup_time);
}

