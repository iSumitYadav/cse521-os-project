#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include<list.h>
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);


static int64_t ticks;
struct semaphore t_sema;
static struct list thread_sleep_insert_ordered;

// Though it's already present in src/tests/internal/list.c
/* Returns true if value A is less than value B, false
   otherwise. */
static bool value_less(const struct list_elem *a_, const struct list_elem *b_, void *aux){
  const struct thread *a = list_entry (a_, struct thread, elem_ptr);
  const struct thread *b = list_entry (b_, struct thread, elem_ptr);

  // return a->wakeup_ticks < b->wakeup_ticks;
  // return a->priority > b->priority;
if(a->wakeup_ticks != b->wakeup_ticks){
  return a->wakeup_ticks < b->wakeup_ticks;
}else{
  return a->priority > b->priority;
}

// if(a->priority != b->priority){
//   return a->priority > b->priority;
// }else{
//   return a->wakeup_ticks < b->wakeup_ticks;
// }

  // if(a->priority < b->priority){
  //   return false;
  // }else if(a->priority > b->priority){
  //   return true;
  // }else{
  //   return a->wakeup_ticks < b->wakeup_ticks;
  // }

  // if(aux == "wakeup_ticks"){
  //   return a->wakeup_ticks < b->wakeup_ticks;
  // }else if(aux == "priority"){
  //   return a->priority > b->priority;
  // }
}


/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
  list_init(&thread_sleep_insert_ordered);
  sema_init(&t_sema, 1);
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
    if (!too_many_loops (loops_per_tick | test_bit))
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
  // return if ticks to sleep are zero or negative
  if (ticks <= 0)
    return;

  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);
  // while (timer_elapsed (start) < ticks)
  //  thread_yield ();

  // pointer to currebt running thread
  struct thread * current_thread = thread_current();
  //add the sleeping ticks to current system ticks, so that it can be used at the time of wakeup and minimizes calculations in timer interrupt
  current_thread->wakeup_ticks = start + ticks;
  // printf("\n\n==================\n\n");
  // printf("Ticks:%d\n", current_thread->wakeup_ticks);
  // printf("PRIO:%d\n", current_thread->priority);
  // printf("\n\n==================\n\n");

  // call sema down
  sema_down(&t_sema);
  // insert in thread sleeping list, ordered by ascending wakeup ticks
  list_insert_ordered(&thread_sleep_insert_ordered, &current_thread->elem_ptr, value_less, "wakeup_ticks");
  // call sema up
  sema_up(&t_sema);

  //enum intr_level old_level;
  //old_level = intr_disable ();
  thread_block();
  //intr_set_level (old_level);
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
  thread_tick ();

  // pointer to a thread (used for pointing to front thread of sleeping list)
  struct thread *front_thread_ptr;

  // execute this block only if the thread sleeping list is not empty
  if(!list_empty(&thread_sleep_insert_ordered)){
    // get the pointer to the first thread in thread sleeping list
    front_thread_ptr = list_entry(
      list_front(&thread_sleep_insert_ordered),
      struct thread,
      elem_ptr
    );


    while(front_thread_ptr->wakeup_ticks <= ticks){
      // unblock the thread pointed by the front_thread_ptr (the first thread is sleeping thread list)
      thread_unblock(front_thread_ptr);
      // remove the unblocked thread from sleeping list
      list_pop_front(&thread_sleep_insert_ordered);

      // if after popping the thread out of the list, the list gets empty, check for it and break the loop
      if(list_empty(&thread_sleep_insert_ordered)){
          break;
      }

      // check if there are still some sleeping threads in the thread sleeping list and awake them as well
      front_thread_ptr = list_entry(
        list_front(&thread_sleep_insert_ordered),
        struct thread,
        elem_ptr
      );
    }
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
