/*
   Classic Priority Inversion deadlock test case
  
   Copyright (C) 2006 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
     Contributed by Clark Williams<williams@redhat.com>, 2006
  
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.
  
   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
  
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */


/* This program tests Priority Inheritance mutexes and their ability
   to avoid Priority Inversion deadlocks
  
   The basic premise here is to set up a deadlock scenario and confirm that PI
   mutexes resolve the situation. Three worker threads will be created from the
   main thread: low, medium and high priority threads that use SCHED_FIFO as 
   their scheduling policy. The low priority thread claims a mutex and then
   starts "working". The medium priority thread starts and preempts the low 
   priority thread. Then the high priority thread runs and attempts to claim
   the mutex owned by the low priority thread. Without priority inheritance, 
   this will deadlock the program. With priority inheritance, the low priority 
   thread receives a priority boost, finishes it's "work" and releases the mutex,
   which allows the high priority thread to run and finish and then the medium
   priority thread finishes. 
   
   That's the theory, anyway... 
   
   CW - 2006  */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

/* test timeout */
#define TIMEOUT 2

/* determine if the C library supports Priority Inheritance mutexes */
#if defined(_POSIX_THREAD_PRIO_INHERIT) && _POSIX_THREAD_PRIO_INHERIT != -1
#define HAVE_PI_MUTEX 1
#else
#define HAVE_PI_MUTEX 0
#endif

int use_pi_mutex = HAVE_PI_MUTEX;

#define SUCCESS 0
#define FAILURE 1

/* the number of times we cause a priority inversion situation */
int inversions = 1;

/* the file handle used by the error reporting routine */
FILE *errout;

#define MAIN do_test
#define TEST_FUNCTION do_test(argc, argv)
#define CMDLINE_OPTIONS { "verbose", no_argument, NULL, 1002 }, \
		        { "no-pi", no_argument, NULL, 1003},
#define CMDLINE_PROCESS  \
	case 1002: verbose=1; break;		\
	case 1003: use_pi_mutex = 0; break;
#define CLEANUP_HANDLER cleanup()
int verbose = 0;

/* define priorities for the threads */
#define SKEL_PRIO(x) (x)
#define MAIN_PRIO(x) (x - 1)
#define HIGH_PRIO(x) (x - 2)
#define MED_PRIO(x)  (x - 3)
#define LOW_PRIO(x)  (x - 4)

enum thread_names
{ LOW = 0, MEDIUM, HIGH, NUM_WORKER_THREADS };

pthread_mutex_t mutex;
pthread_mutexattr_t mutex_attr;

pthread_barrier_t all_threads_ready;
pthread_barrier_t all_threads_done;

/* state barriers */
pthread_barrier_t start_barrier;
pthread_barrier_t locked_barrier;
pthread_barrier_t elevate_barrier;
pthread_barrier_t finish_barrier;

volatile int deadlocked = 0;
volatile int high_has_run = 0;
volatile int low_unlocked = 0;

cpu_set_t cpu_mask;

struct thread_parameters
{
  pthread_t tid;
  int inversions;
} thread_parameters[NUM_WORKER_THREADS];

/* forward prototypes */
void *low_priority (void *arg);
void *med_priority (void *arg);
void *high_priority (void *arg);
int setup_thread_attr (pthread_attr_t * attr, int prio, cpu_set_t * mask);
int set_cpu_affinity (cpu_set_t * mask);
void error (char *, ...);
void info (char *, ...);

void
prepare (int argc, char **argv)
{
  struct sched_param thread_param;
  int max = sched_get_priority_max (SCHED_FIFO);
  int status;

  errout = stdout;

  /* boost test skeleton to max priority (so we keep running) :) */
  thread_param.sched_priority = SKEL_PRIO (max);
  status = pthread_setschedparam (pthread_self (), SCHED_FIFO, &thread_param);
  if (status)
    error ("main: boosting to max priority: 0x%x\n", status);
}

#define PREPARE prepare

int
initialize_barriers (void)
{
  int status;

  status =
    pthread_barrier_init (&all_threads_ready, NULL, NUM_WORKER_THREADS + 1);
  if (status)
    {
      error ("initialize_barriers: failed to initialize all_threads_ready\n");
      return FAILURE;
    }
  status =
    pthread_barrier_init (&all_threads_done, NULL, NUM_WORKER_THREADS + 1);
  if (status)
    {
      error ("initialize_barriers: failed to initialize all_threads_done\n");
      return FAILURE;
    }
  status = pthread_barrier_init (&start_barrier, NULL, NUM_WORKER_THREADS);
  if (status)
    {
      error ("initialize_barriers: failed to initialize start_barrier\n");
      return FAILURE;
    }
  status = pthread_barrier_init (&locked_barrier, NULL, 2);
  if (status)
    {
      error ("initializing_barriers: failed to intialize locked_barrier\n");
      return FAILURE;
    }
  status = pthread_barrier_init (&elevate_barrier, NULL, 2);
  if (status)
    {
      error ("initializing_barriers: failed to initialize elevate_barrier\n");
      return FAILURE;
    }
  status = pthread_barrier_init (&finish_barrier, NULL, NUM_WORKER_THREADS);
  if (status)
    {
      error ("initializing_barriers: failed to initialize finish_barrier\n");
      return FAILURE;
    }
  return SUCCESS;
}

void
cleanup (void)
{
  int i;
  int status;
  for (i = 0; i < NUM_WORKER_THREADS; i++)
    {
      status = pthread_kill (thread_parameters[i].tid, SIGQUIT);
      if (status)
	error ("cleanup: error sending SIGQUIT to thread %d\n",
	       thread_parameters[i].tid);
    }
}

void
handler (int signal)
{
  info ("handler: %s fired\n", sys_siglist[signal]);
  cleanup ();
  if (signal == SIGALRM)
    {
      error ("handler: DEADLOCK detected!\n");
      deadlocked = 1;
    }
}

int
MAIN (int argc, char **argv)
{
  int status;
  int prio_max;
  pthread_attr_t thread_attr;
  struct sched_param thread_param;

  errout = stdout;

  /* initialize default attributes for the mutex */
  status = pthread_mutexattr_init (&mutex_attr);
  if (status)
    {
      error ("main: initializing mutex attribute: 0x%x\n", status);
      return FAILURE;
    }

  if (use_pi_mutex)
    {
      /* set priority inheritance attribute for mutex */
      status = pthread_mutexattr_setprotocol (&mutex_attr,
					      PTHREAD_PRIO_INHERIT);
      if (status)
	{
	  error ("main: setting mutex attribute policy: 0x%x\n", status);
	  return FAILURE;
	}
    }
  info ("main: Priority Inheritance turned %s\n",
	use_pi_mutex ? "on" : "off");

  /* initialize our mutex */
  status = pthread_mutex_init (&mutex, &mutex_attr);
  if (status)
    {
      error ("main: initializing mutex: 0x%x\n", status);
      return FAILURE;
    }

  /* set up our barriers */
  status = initialize_barriers ();
  if (status)
    return FAILURE;

  /* set up CPU affinity so we only use one processor */
  if (set_cpu_affinity (&cpu_mask))
    return FAILURE;

  /* boost us to max priority (so we keep running) :) */
  prio_max = sched_get_priority_max (SCHED_FIFO);
  thread_param.sched_priority = MAIN_PRIO (prio_max);
  status = pthread_setschedparam (pthread_self (), SCHED_FIFO, &thread_param);
  if (status)
    {
      error ("main: boosting to max priority: 0x%x\n", status);
      /* Don't fail if we don't have the right privledges */
      return SUCCESS;
    }

  /* start the low priority thread */
  info ("main: creating low priority thread\n");
  setup_thread_attr (&thread_attr, LOW_PRIO (prio_max), &cpu_mask);
  thread_parameters[LOW].inversions = inversions;
  status = pthread_create (&thread_parameters[LOW].tid,
			   &thread_attr,
			   low_priority, &thread_parameters[LOW]);
  if (status != 0)
    {
      error ("main: creating low_priority thread: 0x%x\n", status);
      return FAILURE;
    }

  /* create the medium priority thread */
  info ("main: creating medium priority thread\n");
  setup_thread_attr (&thread_attr, MED_PRIO (prio_max), &cpu_mask);
  thread_parameters[MEDIUM].inversions = inversions;
  status = pthread_create (&thread_parameters[MEDIUM].tid,
			   &thread_attr,
			   med_priority, &thread_parameters[MEDIUM]);
  if (status != 0)
    {
      error ("main: creating med_priority thread: 0x%x\n", status);
      return FAILURE;
    }

  /* create the high priority thread */
  info ("main: creating high priority thread\n");
  if (setup_thread_attr (&thread_attr, HIGH_PRIO (prio_max), &cpu_mask))
    return FAILURE;
  thread_parameters[HIGH].inversions = inversions;
  status = pthread_create (&thread_parameters[HIGH].tid,
			   &thread_attr,
			   high_priority, &thread_parameters[HIGH]);
  if (status != 0)
    {
      error ("main: creating high_priority thread: 0x%x\n", status);
      cleanup ();
      return FAILURE;
    }

  signal (SIGINT, handler);

  info ("main: releasing all threads\n");
  status = pthread_barrier_wait (&all_threads_ready);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("main: pthread_barrier_wait(all_threads_ready): 0x%x\n", status);
      cleanup ();
      return FAILURE;
    }
  info ("main: all threads initialized\n");

  info ("main: waiting for threads to finish\n");

  status = pthread_barrier_wait (&all_threads_done);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("main: pthread_barrier_wait(all_threads_done): 0x%x\n", status);
      cleanup ();
      return FAILURE;
    }
  info ("main: all threads terminated!\n");
  if (deadlocked)
    {
      info ("main: test failed\n");
      return FAILURE;
    }
  info ("main: test passed\n");
  return SUCCESS;
}


int
setup_thread_attr (pthread_attr_t * attr, int prio, cpu_set_t * mask)
{
  int status;
  struct sched_param thread_param;

  status = pthread_attr_init (attr);
  if (status)
    {
      error ("setup_thread_attr: initializing thread attribute: 0x%x\n",
	     status);
      return FAILURE;
    }
  status = pthread_attr_setschedpolicy (attr, SCHED_FIFO);
  if (status)
    {
      error
	("setup_thread_attr: setting attribute policy to SCHED_FIFO: 0x%x\n",
	 status);
      return FAILURE;
    }
  status = pthread_attr_setinheritsched (attr, PTHREAD_EXPLICIT_SCHED);
  if (status)
    {
      error
	("setup_thread_attr: setting explicit scheduling inheritance: 0x%x\n",
	 status);
      return FAILURE;
    }
  thread_param.sched_priority = prio;
  status = pthread_attr_setschedparam (attr, &thread_param);
  if (status)
    {
      error ("setup_thread_attr: setting scheduler param: 0x%x\n", status);
      return FAILURE;
    }
  status = pthread_attr_setaffinity_np (attr, sizeof (cpu_set_t), mask);
  if (status)
    {
      error ("setup_thread_attr: setting affinity attribute: 0x%x\n", status);
      return FAILURE;
    }
  return SUCCESS;
}

int
set_cpu_affinity (cpu_set_t * cpu_set)
{
  int status, i;
  cpu_set_t current_mask, new_mask;

  /* Now set our CPU affinity to only run one one processor */
  status = sched_getaffinity (0, sizeof (cpu_set_t), &current_mask);
  if (status)
    {
      error ("set_cpu_affinity: getting CPU affinity mask: 0x%x\n", status);
      return FAILURE;
    }
  for (i = 0; i < sizeof (cpu_set_t) * 8; i++)
    {
      if (CPU_ISSET (i, &current_mask))
	break;
    }
  if (i >= sizeof (cpu_set_t) * 8)
    {
      error ("set_cpu_affinity: No schedulable CPU found!\n");
      return FAILURE;
    }
  CPU_ZERO (&new_mask);
  CPU_SET (i, &new_mask);
  status = sched_setaffinity (0, sizeof (cpu_set_t), &new_mask);
  if (status)
    {
      error ("set_cpu_affinity: setting CPU affinity mask: 0x%x\n", status);
      return FAILURE;
    }
  info ("set_cpu_affinity: using processr %d\n", i);
  *cpu_set = new_mask;
  return SUCCESS;
}

void
report_threadinfo (char *name)
{
  int status;
  struct sched_param thread_param;
  int thread_policy;

  status =
    pthread_getschedparam (pthread_self (), &thread_policy, &thread_param);
  if (status)
    {
      error ("report_threadinfo: failed to get scheduler param: 0x%x\n",
	     status);
      pthread_mutex_unlock (&mutex);
      exit (FAILURE);
    }
  info ("%s: running as %s thread at priority %d\n",
	name, thread_policy == SCHED_FIFO ? "FIFO" :
	thread_policy == SCHED_RR ? "RR" : "OTHER",
	thread_param.sched_priority);
}

void *
low_priority (void *arg)
{
  int status;
  struct thread_parameters *p = (struct thread_parameters *) arg;

  report_threadinfo ("low_priority");

  info ("low_priority: entering ready state\n");

  /* wait for all threads to be ready */
  status = pthread_barrier_wait (&all_threads_ready);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("low_priority: pthread_barrier_wait(all_threads_ready): %x",
	     status);
      return NULL;
    }

  info ("low_priority: starting inversion loop (%d)\n", p->inversions);
  while (p->inversions-- > 0)
    {
      /* initial state */
      info ("low_priority: entering start wait (%d)\n", p->inversions + 1);
      status = pthread_barrier_wait (&start_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("low_priority: pthread_barrier_wait(start): %x\n", status);
	  return NULL;
	}
      info ("low_priority: claiming mutex\n");
      pthread_mutex_lock (&mutex);
      info ("low_priority: mutex locked\n");

      info ("low_priority: entering locked wait\n");
      status = pthread_barrier_wait (&locked_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("low_priority: pthread_barrier_wait(locked): %x\n", status);
	  return NULL;
	}

      /* wait for priority boost */
      info ("low_priority: entering elevated wait\n");
      low_unlocked = 0;		/* prevent race with med_priority */
      status = pthread_barrier_wait (&elevate_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("low_priority: pthread_barrier_wait(elevate): %x\n", status);
	  return NULL;
	}
      low_unlocked = 1;

      /* release the mutex */
      info ("low_priority: unlocking mutex\n");
      pthread_mutex_unlock (&mutex);

      /* finish state */
      info ("low_priority: entering finish wait\n");
      status = pthread_barrier_wait (&finish_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("low_priority: pthread_barrier_wait(elevate): %x\n", status);
	  return NULL;
	}

    }
  /* let main know we're done */
  info ("low_priority: entering exit state\n");
  status = pthread_barrier_wait (&all_threads_done);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("low_priority: pthread_barrier_wait(all_threads_done): %x",
	     status);
      return NULL;
    }
  info ("low_priority: exiting\n");
  return NULL;
}

void *
med_priority (void *arg)
{
  int status;
  struct thread_parameters *p = (struct thread_parameters *) arg;

  report_threadinfo ("med_priority");

  info ("med_priority: entering ready state\n");
  /* wait for all threads to be ready */
  status = pthread_barrier_wait (&all_threads_ready);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("med_priority: pthread_barrier_wait(all_threads_ready): %x",
	     status);
      return NULL;
    }

  info ("med_priority: starting inversion loop (%d)\n", p->inversions);
  while (p->inversions-- > 0)
    {
      /* start state */
      info ("med_priority: entering start state (%d)\n", p->inversions + 1);
      status = pthread_barrier_wait (&start_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("med_priority: pthread_barrier_wait(start): %x", status);
	  return NULL;
	}
      info ("med_priority: entering elevate state\n");
      do
	{
	  status = pthread_barrier_wait (&elevate_barrier);
	  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	    {
	      error ("med_priority: pthread_barrier_wait(elevate): %x",
		     status);
	      return NULL;
	    }
	}
      while (!high_has_run && !low_unlocked);
      info ("med_priority: entering finish state\n");
      status = pthread_barrier_wait (&finish_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("med_priority: pthread_barrier_wait(finished): %x", status);
	  return NULL;
	}
    }

  info ("med_priority: entering exit state\n");
  status = pthread_barrier_wait (&all_threads_done);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("med_priority: pthread_barrier_wait(all_threads_done): %x",
	     status);
      return NULL;
    }
  info ("med_priority: exiting\n");
  return NULL;
}

void *
high_priority (void *arg)
{
  int status;
  struct thread_parameters *p = (struct thread_parameters *) arg;

  report_threadinfo ("high_priority");

  info ("high_priority: entering ready state\n");

  /* wait for all threads to be ready */
  status = pthread_barrier_wait (&all_threads_ready);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("high_priority: pthread_barrier_wait(all_threads_ready): %x",
	     status);
      return NULL;
    }

  info ("high_priority: starting inversion loop (%d)\n", p->inversions);
  while (p->inversions-- > 0)
    {
      high_has_run = 0;
      info ("high_priority: entering start state (%d)\n", p->inversions + 1);
      status = pthread_barrier_wait (&start_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("high_priority: pthread_barrier_wait(start): %x", status);
	  return NULL;
	}
      info ("high_priority: entering running state\n");
      status = pthread_barrier_wait (&locked_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("high_priority: pthread_barrier_wait(running): %x", status);
	  return NULL;
	}
      info ("high_priority: locking mutex\n");
      pthread_mutex_lock (&mutex);
      info ("high_priority: got mutex\n");
      high_has_run = 1;
      info ("high_priority: unlocking mutex\n");
      pthread_mutex_unlock (&mutex);
      info ("high_priority: entering finish state\n");
      status = pthread_barrier_wait (&finish_barrier);
      if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
	{
	  error ("high_priority: pthread_barrier_wait(finish): %x", status);
	  return NULL;
	}
    }

  info ("high_priority: entering exit state\n");
  status = pthread_barrier_wait (&all_threads_done);
  if (status && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
      error ("high_priority: pthread_barrier_wait(all_threads_done): %x",
	     status);
      return NULL;
    }
  info ("high_priority: exiting\n");
  return NULL;
}

void
error (char *fmt, ...)
{
  va_list ap;
  fputs ("ERROR: ", errout);
  va_start (ap, fmt);
  vfprintf (errout, fmt, ap);
  va_end (ap);
}

void
info (char *fmt, ...)
{
  if (verbose)
    {
      va_list ap;
      va_start (ap, fmt);
      vprintf (fmt, ap);
      va_end (ap);
    }
}
#include "../test-skeleton.c"
