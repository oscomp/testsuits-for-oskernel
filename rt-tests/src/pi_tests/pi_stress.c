/*
   pi_stress - Priority Inheritance stress test

   Copyright (C) 2006, 2007 Clark Williams <williams@redhat.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
   USA */

/* This program stress tests pthreads priority inheritance mutexes

   The logic is built upon the state machine that performs the "classic_pi"
   deadlock scenario. A state machine or "inversion group" is a group of three
   threads as described below.

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
#include <getopt.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <termios.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>

#include "rt-sched.h"
#include "rt-utils.h"

#include "error.h"

/* conversions */
#define USEC_PER_SEC 	1000000
#define NSEC_PER_SEC 	1000000000
#define USEC_TO_NSEC(u) ((u) * 1000)
#define USEC_TO_SEC(u) 	((u) / USEC_PER_SEC)
#define NSEC_TO_USEC(n) ((n) / 1000)
#define SEC_TO_NSEC(s) 	((s) * NSEC_PER_SEC)
#define SEC_TO_USEC(s) 	((s) * USEC_PER_SEC)

/* test timeout */
#define TIMEOUT 2

#if PTHREAD_PRIO_INHERIT == 1
#define _POSIX_THREAD_PRIO_INHERIT 1
#endif

/* determine if the C library supports Priority Inheritance mutexes */
#if defined(_POSIX_THREAD_PRIO_INHERIT) && _POSIX_THREAD_PRIO_INHERIT != -1
#define HAVE_PI_MUTEX 1
#else
#define HAVE_PI_MUTEX 0
#endif

#if HAVE_PI_MUTEX == 0
#error "Can't run this test without PI Mutex support"
#endif

#define SUCCESS 0
#define FAILURE 1

/* cursor control */
#define UP_ONE "\033[1A"
#define DOWN_ONE "\033[1B"

#define pi_info(fmt, arg...) \
	do { if (verbose) info(fmt, ## arg); } while (0)
#define pi_debug(fmt, arg...) \
	do { if (debugging) debug(fmt, ## arg); } while (0)
#define pi_error(fmt, arg...) \
	do { err_msg(fmt, ## arg); have_errors = 1; } while (0)

/* the length of the test */
/* default is infinite */
int duration = -1;

/* times for starting and finishing the stress test */
time_t start, finish;

/* the number of groups to create */
int ngroups = 0;

/* the number of times a group causes a priority inversion situation */
/* default to infinite */
int inversions = -1;

/* turn on lots of prints */
int verbose = 0;

/* turn on pi_debugging prints */
int debugging = 0;

int quiet = 0;	/* turn off all prints, default = 0 (off) */

/* prompt to start test */
int prompt = 0;

/* report interval */
unsigned long report_interval = (unsigned long)SEC_TO_USEC(0.75);

int shutdown = 0;		/* global indicating we should shut down */
pthread_mutex_t shutdown_mtx;	/* associated mutex */

/* indicate if errors have occurred */
int have_errors = 0;

/* indicated that keyboard interrupt has happened */
int interrupted = 0;

/* force running on one cpu */
int uniprocessor = 0;

/* lock all memory */
int lockall = 0;

/* command line options */
struct option options[] = {
	{"duration", required_argument, NULL, 't'},
	{"verbose", no_argument, NULL, 'v'},
	{"quiet", no_argument, NULL, 'q'},
	{"groups", required_argument, NULL, 'g'},
	{"inversions", required_argument, NULL, 'i'},
	{"rr", no_argument, NULL, 'r'},
	{"sched", required_argument, NULL, 's'},
	{"uniprocessor", no_argument, NULL, 'u'},
	{"prompt", no_argument, NULL, 'p'},
	{"debug", no_argument, NULL, 'd'},
	{"version", no_argument, NULL, 'V'},
	{"mlockall", no_argument, NULL, 'm'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

#define NUM_TEST_THREADS 3
#define NUM_ADMIN_THREADS 1

pthread_barrier_t all_threads_ready;
pthread_barrier_t all_threads_done;

cpu_set_t test_cpu_mask, admin_cpu_mask;

int policy = SCHED_FIFO;

/* scheduling attributes per thread */
struct sched_attr low_sa;
struct sched_attr med_sa;
struct sched_attr high_sa;
struct sched_attr admin_sa;

#define SA_INIT_LOW	(1 << 0)
#define SA_INIT_MED	(1 << 1)
#define SA_INIT_HIGH	(1 << 2)
#define SA_INIT_ADMIN	(1 << 3)

unsigned int sa_initialized;

struct group_parameters {

	/* group id (index) */
	int id;

	/* cpu this group is bound to */
	long cpu;

	/* threads in the group */
	pthread_t low_tid;
	pthread_t med_tid;
	pthread_t high_tid;

	/* number of machine iterations to perform */
	int inversions;

	/* group mutex */
	pthread_mutex_t mutex;

	/* state barriers */
	pthread_barrier_t start_barrier;
	pthread_barrier_t locked_barrier;
	pthread_barrier_t elevate_barrier;
	pthread_barrier_t finish_barrier;

	/* Either everyone goes through the loop, or else no-ones does */
	pthread_barrier_t loop_barr;
	pthread_mutex_t loop_mtx;	/* Protect access to int loop */
	int loop;	/* boolean, loop or not, connected to shutdown */

	/* state variables */
	volatile int watchdog;

	/* total number of inversions performed */
	unsigned long total;

	/* total watchdog hits */
	int watchdog_hits;

} *groups;

/* number of consecutive watchdog hits before quitting */
#define WATCHDOG_LIMIT 5

/* number of online processors */
long num_processors = 0;

/* forward prototypes */
void *low_priority(void *arg);
void *med_priority(void *arg);
void *high_priority(void *arg);
void *reporter(void *arg);
void *watchdog(void *arg);
int setup_thread_attr(pthread_attr_t * attr, struct sched_attr * sa,
		      cpu_set_t * mask);
int set_cpu_affinity(cpu_set_t * test_mask, cpu_set_t * admin_mask);
void process_command_line(int argc, char **argv);
void usage(void);
int block_signals(void);
int allow_sigterm(void);
void set_shutdown_flag(void);
int initialize_group(struct group_parameters *group);
int create_group(struct group_parameters *group);
unsigned long total_inversions(void);
void banner(void);
void summary(void);
void wait_for_termination(void);
int barrier_init(pthread_barrier_t * b, const pthread_barrierattr_t * attr,
		 unsigned count, const char *name);
void setup_sched_attr(struct sched_attr *attr, int policy, int prio);
void setup_sched_config(int policy);

int main(int argc, char **argv)
{
	int status;
	struct sched_param thread_param;
	int i;
	int retval = FAILURE;
	int core;
	int nthreads;

	/* Make sure we see all message, even those on stdout.  */
	setvbuf(stdout, NULL, _IONBF, 0);

	/* get the number of processors */
	num_processors = sysconf(_SC_NPROCESSORS_ONLN);

	/* calculate the number of inversion groups to run */
	ngroups = num_processors == 1 ? 1 : num_processors - 1;


	/* process command line arguments */
	process_command_line(argc, argv);

	/* set default sched attributes */
	setup_sched_config(policy);

	/* lock memory */
	if (lockall)
		if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
			pi_error("mlockall failed\n");
			return FAILURE;
		}
	/* boost main's priority (so we keep running) :) */
	thread_param.sched_priority = admin_sa.sched_priority;
	status = pthread_setschedparam(pthread_self(), admin_sa.sched_policy,
				       &thread_param);
	if (status) {
		pi_error("main: boosting to max priority: 0x%x\n", status);
		return FAILURE;
	}
	/* block unwanted signals */
	block_signals();

	/* allocate our groups array */
	groups = calloc(ngroups, sizeof(struct group_parameters));
	if (groups == NULL) {
		pi_error("main: failed to allocate %d groups\n", ngroups);
		return FAILURE;
	}
	/* set up CPU affinity masks */
	if (set_cpu_affinity(&test_cpu_mask, &admin_cpu_mask))
		return FAILURE;

	nthreads = ngroups * NUM_TEST_THREADS + NUM_ADMIN_THREADS;

	/* set up our ready barrier */
	if (barrier_init(&all_threads_ready, NULL, nthreads,
			 "all_threads_ready"))
		return FAILURE;

	/* set up our done barrier */
	if (barrier_init(&all_threads_done, NULL, nthreads, "all_threads_done"))
		return FAILURE;

	/* create the groups */
	pi_info("Creating %d test groups\n", ngroups);
	for (core = 0; core < num_processors; core++)
		if (CPU_ISSET(core, &test_cpu_mask))
			break;
	for (i = 0; i < ngroups; i++) {
		groups[i].id = i;
		groups[i].cpu = core++;
		if (core >= num_processors)
			core = 0;
		if (create_group(&groups[i]) != SUCCESS)
			return FAILURE;
	}

	/* prompt if requested */
	if (prompt) {
		printf("Press return to start test: ");
		getchar();
	}
	/* report */
	banner();
	start = time(NULL);

	/* turn loose the threads */
	pi_info("Releasing all threads\n");
	status = pthread_barrier_wait(&all_threads_ready);
	if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
		pi_error("main: pthread_barrier_wait(all_threads_ready): 0x%x\n",
		      status);
		set_shutdown_flag();
		return FAILURE;
	}

	reporter(NULL);

	if (!quiet) {
		fputs(DOWN_ONE, stdout);
		printf("Stopping test\n");
	}
	set_shutdown_flag();

	/* wait for all threads to notice the shutdown flag */
	if (have_errors == 0 && interrupted == 0) {
		pi_info("waiting for all threads to complete\n");
		status = pthread_barrier_wait(&all_threads_done);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("main: pthread_barrier_wait(all_threads_ready): 0x%x\n",
			     status);
			return FAILURE;
		}
		pi_info("All threads terminated!\n");
		retval = SUCCESS;
	} else
		kill(0, SIGTERM);
	finish = time(NULL);
	summary();
	if (lockall)
		munlockall();
	exit(retval);
}

int
setup_thread_attr(pthread_attr_t * attr, struct sched_attr * sa,
		  cpu_set_t * mask)
{
	int status;
	struct sched_param thread_param;

	status = pthread_attr_init(attr);
	if (status) {
		pi_error
		    ("setup_thread_attr: initializing thread attribute: 0x%x\n",
		     status);
		return FAILURE;
	}
	status = pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), mask);
	if (status) {
		pi_error("setup_thread_attr: setting affinity attribute: 0x%x\n",
		      status);
		return FAILURE;
	}

	/* The pthread API does not yet support SCHED_DEADLINE, defer the
	 * thread configuration to setup_thread() */
	if (sa->sched_policy == SCHED_DEADLINE)
		return SUCCESS;

	status = pthread_attr_setschedpolicy(attr, sa->sched_policy);
	if (status) {
		pi_error
		    ("setup_thread_attr: setting attribute policy to %s: 0x%x\n",
		     policy_to_string(sa->sched_policy),
		     status);
		return FAILURE;
	}
	status = pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
	if (status) {
		pi_error
		    ("setup_thread_attr: setting explicit scheduling inheritance: 0x%x\n",
		     status);
		return FAILURE;
	}
	thread_param.sched_priority = sa->sched_priority;
	status = pthread_attr_setschedparam(attr, &thread_param);
	if (status) {
		pi_error("setup_thread_attr: setting scheduler param: 0x%x\n",
		      status);
		return FAILURE;
	}
	return SUCCESS;
}

int set_cpu_affinity(cpu_set_t * test_mask, cpu_set_t * admin_mask)
{
	int status, i, admin_proc;
	cpu_set_t current_mask;

	/* handle uniprocessor case */
	if (num_processors == 1 || uniprocessor) {
		CPU_ZERO(admin_mask);
		CPU_ZERO(test_mask);
		CPU_SET(0, admin_mask);
		CPU_SET(0, test_mask);
		pi_info("admin and test threads running on one processor\n");
		return SUCCESS;
	}
	/* first set our main thread to run on the first
	   scheduleable processor we can find */
	status = sched_getaffinity(0, sizeof(cpu_set_t), &current_mask);
	if (status) {
		pi_error("failed getting CPU affinity mask: 0x%x\n", status);
		return FAILURE;
	}
	for (i = 0; i < num_processors; i++) {
		if (CPU_ISSET(i, &current_mask))
			break;
	}
	if (i >= num_processors) {
		pi_error("No schedulable CPU found for main!\n");
		return FAILURE;
	}
	admin_proc = i;
	CPU_ZERO(admin_mask);
	CPU_SET(admin_proc, admin_mask);
	status = sched_setaffinity(0, sizeof(cpu_set_t), admin_mask);
	if (status) {
		pi_error("set_cpu_affinity: setting CPU affinity mask: 0x%x\n",
		      status);
		return FAILURE;
	}
	pi_info("Admin thread running on processor: %d\n", i);

	/* Set test affinity so that tests run on the non-admin processors */
	CPU_ZERO(test_mask);
	for (i = admin_proc + 1; i < num_processors; i++)
		CPU_SET(i, test_mask);

	if (admin_proc + 1 == num_processors - 1)
		pi_info("Test threads running on processor: %ld\n",
		     num_processors - 1);
	else
		pi_info("Test threads running on processors:  %d-%d\n",
		     admin_proc + 1, (int)num_processors - 1);

	return SUCCESS;
}

/* clear all watchdog counters */
void watchdog_clear(void)
{
	int i;
	for (i = 0; i < ngroups; i++)
		groups[i].watchdog = 0;
}

/* check for zero watchdog counters */
int watchdog_check(void)
{
	int i;
	int failures = 0;
	struct group_parameters *g;

	for (i = 0; i < ngroups; i++) {
		g = &groups[i];
		if (g->watchdog == 0) {
			/* don't report deadlock if group is finished */
			if (g->inversions == g->total)
				continue;
			if (++g->watchdog_hits >= WATCHDOG_LIMIT) {
				pi_error
				    ("WATCHDOG triggered: group %d is deadlocked!\n",
				     i);
				failures++;
			}
		} else
			g->watchdog_hits = 0;
	}
	return failures ? FAILURE : SUCCESS;
}

int pending_interrupt(void)
{
	sigset_t pending;

	if (sigpending(&pending) < 0) {
		pi_error("from sigpending: %s\n", strerror(errno));
		return 0;
	}

	return interrupted = sigismember(&pending, SIGINT);
}

static inline void tsnorm(struct timespec *ts)
{
	while (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_nsec -= NSEC_PER_SEC;
		ts->tv_sec++;
	}
}

/*
 * this routine serves two purposes:
 *   1. report progress
 *   2. check for deadlocks
 */
void *reporter(void *arg)
{
	int status;
	int end = 0;
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = USEC_TO_NSEC(report_interval);

	tsnorm(&ts);

	if (duration >= 0)
		end = duration + time(NULL);

	/* sleep initially to let everything get up and running */
	status = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
	if (status) {
		pi_error("from clock_nanosleep: %s\n", strerror(status));
		return NULL;
	}

	pi_debug("reporter: starting report loop\n");
	pi_info("Press Control-C to stop test\nCurrent Inversions: \n");

	for (;;) {
		pthread_mutex_lock(&shutdown_mtx);
		if (shutdown) {
			pthread_mutex_unlock(&shutdown_mtx);
			break;
		}
		pthread_mutex_unlock(&shutdown_mtx);

		/* wait for our reporting interval */
		status = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
		if (status) {
			pi_error("from clock_nanosleep: %s\n", strerror(status));
			break;
		}

		/* check for signaled shutdown */
		if (!quiet) {
			pthread_mutex_lock(&shutdown_mtx);
			if (shutdown == 0) {
				fputs(UP_ONE, stdout);
				printf("Current Inversions: %lu\n",
						total_inversions());
			}
		}
		pthread_mutex_unlock(&shutdown_mtx);

		/* if we specified a duration, see if it has expired */
		if (end && time(NULL) > end) {
			pi_info("duration reached (%d seconds)\n", duration);
			set_shutdown_flag();
			continue;
		}
		/* check for a pending SIGINT */
		if (pending_interrupt()) {
			pi_info("Keyboard Interrupt!\n");
			break;
		}
		/* check watchdog stuff */
		if ((watchdog_check())) {
			pi_error("reporter stopping due to watchdog event\n");
			set_shutdown_flag();
			break;
		}
		/* clear watchdog counters */
		watchdog_clear();

	}
	pi_debug("reporter: finished\n");
	set_shutdown_flag();
	return NULL;
}

int verify_cpu(int cpu)
{
	int status;
	int err;
	cpu_set_t mask;

	CPU_ZERO(&mask);

	status = sched_getaffinity(0, sizeof(cpu_set_t), &mask);
	if (status == -1) {
		err = errno;
		fprintf(stderr, "sched_getaffinity %s\n", strerror(err));
		exit(1);
	}

	if (CPU_ISSET(cpu, &mask))
		return SUCCESS;
	return FAILURE;
}

void *low_priority(void *arg)
{
	int status;
	int unbounded;
	unsigned long count = 0;
	struct group_parameters *p = (struct group_parameters *)arg;
	pthread_barrier_t *loop_barr = &p->loop_barr;
	pthread_mutex_t *loop_mtx = &p->loop_mtx;
	int *loop = &p->loop;

	allow_sigterm();

	if (verify_cpu(p->cpu) != SUCCESS) {
		pi_error("low_priority[%d]: not bound to %ld\n", p->id, p->cpu);
		return NULL;
	}

	pi_debug("low_priority[%d]: entering ready state\n", p->id);
	/* wait for all threads to be ready */
	status = pthread_barrier_wait(&all_threads_ready);
	if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
		pi_error
		    ("low_priority[%d]: pthread_barrier_wait(all_threads_ready): %x",
		     p->id, status);
		return NULL;
	}

	unbounded = (p->inversions < 0);

	pi_debug("low_priority[%d]: starting inversion loop\n", p->id);

	for (;;) {
		/*
		   We can't set the 'loop' boolean here, because some flags
		   may have already reached the loop_barr
		 */
		if (!unbounded && (p->total >= p->inversions)) {
			set_shutdown_flag();
		}

		/* Either all threads go through the loop_barr, or none do */
		pthread_mutex_lock(loop_mtx);
		if (*loop == 0) {
			pthread_mutex_unlock(loop_mtx);
			break;
		}
		pthread_mutex_unlock(loop_mtx);

		status = pthread_barrier_wait(loop_barr);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error("%s[%d]: pthread_barrier_wait(loop): %x\n",
			      __func__, p->id, status);
			return NULL;
		}

		/* Only one Thread needs to check the shutdown status */
		if (status == PTHREAD_BARRIER_SERIAL_THREAD) {
			pthread_mutex_lock(&shutdown_mtx);
			if (shutdown) {
				pthread_mutex_lock(loop_mtx);
				*loop = 0;
				pthread_mutex_unlock(loop_mtx);
			}
			pthread_mutex_unlock(&shutdown_mtx);
		}

		/* initial state */
		pi_debug("low_priority[%d]: entering start wait (%d)\n", p->id,
		      count++);
		status = pthread_barrier_wait(&p->start_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("low_priority[%d]: pthread_barrier_wait(start): %x\n",
			     p->id, status);
			return NULL;
		}

		pi_debug("low_priority[%d]: claiming mutex\n", p->id);
		pthread_mutex_lock(&p->mutex);
		pi_debug("low_priority[%d]: mutex locked\n", p->id);

		pi_debug("low_priority[%d]: entering locked wait\n", p->id);
		status = pthread_barrier_wait(&p->locked_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
				("low_priority[%d]: pthread_barrier_wait(locked): %x\n",
				 p->id, status);
			/* release the mutex */
			pi_debug("low_priority[%d]: unlocking mutex\n", p->id);
			pthread_mutex_unlock(&p->mutex);
			return NULL;
		}

		/* wait for priority boost */
		pi_debug("low_priority[%d]: entering elevated wait\n", p->id);
		status = pthread_barrier_wait(&p->elevate_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
				("low_priority[%d]: pthread_barrier_wait(elevate): %x\n",
				 p->id, status);
			/* release the mutex */
			pi_debug("low_priority[%d]: unlocking mutex\n", p->id);
			pthread_mutex_unlock(&p->mutex);
			return NULL;
		}

		/* release the mutex */
		pi_debug("low_priority[%d]: unlocking mutex\n", p->id);
		pthread_mutex_unlock(&p->mutex);

		/* finish state */
		pi_debug("low_priority[%d]: entering finish wait\n", p->id);
		status = pthread_barrier_wait(&p->finish_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("low_priority[%d]: pthread_barrier_wait(elevate): %x\n",
			     p->id, status);
			return NULL;
		}
	}
	set_shutdown_flag();
	pi_debug("low_priority[%d]: entering done barrier\n", p->id);
	/* wait for all threads to finish */
	status = pthread_barrier_wait(&all_threads_done);
	if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
		pi_error
		    ("low_priority[%d]: pthread_barrier_wait(all_threads_done): %x",
		     p->id, status);
		return NULL;
	}
	pi_debug("low_priority[%d]: exiting\n", p->id);
	return NULL;
}

void *med_priority(void *arg)
{
	int status;
	int unbounded;
	unsigned long count = 0;
	struct group_parameters *p = (struct group_parameters *)arg;
	pthread_barrier_t *loop_barr = &p->loop_barr;
	pthread_mutex_t *loop_mtx = &p->loop_mtx;
	int *loop = &p->loop;

	allow_sigterm();

	if (verify_cpu(p->cpu) != SUCCESS) {
		pi_error("med_priority[%d]: not bound to %ld\n", p->id, p->cpu);
		return NULL;
	}

	pi_debug("med_priority[%d]: entering ready state\n", p->id);
	/* wait for all threads to be ready */
	status = pthread_barrier_wait(&all_threads_ready);
	if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
		pi_error
		    ("med_priority[%d]: pthread_barrier_wait(all_threads_ready): %x",
		     p->id, status);
		return NULL;
	}

	unbounded = (p->inversions < 0);

	pi_debug("med_priority[%d]: starting inversion loop\n", p->id);
	for (;;) {
		if (!unbounded && (p->total >= p->inversions)) {
			set_shutdown_flag();
		}
		/* Either all threads go through the loop_barr, or none do */
		pthread_mutex_lock(loop_mtx);
		if (*loop == 0) {
			pthread_mutex_unlock(loop_mtx);
			break;
		}
		pthread_mutex_unlock(loop_mtx);

		status = pthread_barrier_wait(loop_barr);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error("%s[%d]: pthread_barrier_wait(loop): %x\n",
			      __func__, p->id, status);
			return NULL;
		}

		/* Only one Thread needs to check the shutdown status */
		if (status == PTHREAD_BARRIER_SERIAL_THREAD) {
			pthread_mutex_lock(&shutdown_mtx);
			if (shutdown) {
				pthread_mutex_lock(loop_mtx);
				*loop = 0;
				pthread_mutex_unlock(loop_mtx);
			}
			pthread_mutex_unlock(&shutdown_mtx);
		}

		/* start state */
		pi_debug("med_priority[%d]: entering start state (%d)\n", p->id,
		      count++);
		status = pthread_barrier_wait(&p->start_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("med_priority[%d]: pthread_barrier_wait(start): %x",
			     p->id, status);
			return NULL;
		}
		pi_debug("med_priority[%d]: entering elevate state\n", p->id);
		status = pthread_barrier_wait(&p->elevate_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error ("med_priority[%d]: pthread_barrier_wait(elevate): %x", p->id, status);
			return NULL;
		}

		pi_debug("med_priority[%d]: entering finish state\n", p->id);
		status = pthread_barrier_wait(&p->finish_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("med_priority[%d]: pthread_barrier_wait(finished): %x",
			     p->id, status);
			return NULL;
		}
	}
	set_shutdown_flag();

	pi_debug("med_priority[%d]: entering done barrier\n", p->id);
	/* wait for all threads to finish */
	if (have_errors == 0) {
		status = pthread_barrier_wait(&all_threads_done);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("med_priority[%d]: pthread_barrier_wait(all_threads_done): %x",
			     p->id, status);
			return NULL;
		}
	}
	/* exit */
	pi_debug("med_priority[%d]: exiting\n", p->id);
	return NULL;
}

void *high_priority(void *arg)
{
	int status;
	int unbounded;
	unsigned long count = 0;
	struct group_parameters *p = (struct group_parameters *)arg;
	pthread_barrier_t *loop_barr = &p->loop_barr;
	pthread_mutex_t *loop_mtx = &p->loop_mtx;
	int *loop = &p->loop;
	cpu_set_t cpu_mask;
	int i;

	if (high_sa.sched_policy == SCHED_DEADLINE) {
		CPU_ZERO(&cpu_mask);
		for (i = 0; i < num_processors; i++)
			CPU_SET(i, &cpu_mask);
		status = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask);
		if (status < 0) {
			pi_error
			    ("high_priority[%d]: set cpu affinity*dl): %x\n",
			    p->id, status);
			return NULL;
		}

		status = sched_setattr(gettid(), &high_sa, 0);
		if (status < 0) {
			pi_error
			    ("high_priority[%d]: sched_setattr(dl): %x\n",
			    p->id, status);
			return NULL;
		}
	}

	allow_sigterm();
	if (verify_cpu(p->cpu) != SUCCESS) {
		pi_error("high_priority[%d]: not bound to %ld\n", p->id, p->cpu);
		return NULL;
	}

	pi_debug("high_priority[%d]: entering ready state\n", p->id);

	/* wait for all threads to be ready */
	status = pthread_barrier_wait(&all_threads_ready);
	if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
		pi_error
		    ("high_priority[%d]: pthread_barrier_wait(all_threads_ready): %x",
		     p->id, status);
		return NULL;
	}
	unbounded = (p->inversions < 0);
	pi_debug("high_priority[%d]: starting inversion loop\n", p->id);
	for (;;) {
		if (!unbounded && (p->total >= p->inversions)) {
			set_shutdown_flag();
		}

		/* Either all threads go through the loop_barr, or none do */
		pthread_mutex_lock(loop_mtx);
		if (*loop == 0) {
			pthread_mutex_unlock(loop_mtx);
			break;
		}
		pthread_mutex_unlock(loop_mtx);

		status = pthread_barrier_wait(loop_barr);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error("%s[%d]: pthread_barrier_wait(loop): %x\n",
			      __func__, p->id, status);
			return NULL;
		}

		/* Only one Thread needs to check the shutdown status */
		if (status == PTHREAD_BARRIER_SERIAL_THREAD) {
			pthread_mutex_lock(&shutdown_mtx);
			if (shutdown) {
				pthread_mutex_lock(loop_mtx);
				*loop = 0;
				pthread_mutex_unlock(loop_mtx);
			}
			pthread_mutex_unlock(&shutdown_mtx);
		}
		pi_debug("high_priority[%d]: entering start state (%d)\n", p->id,
		      count++);
		status = pthread_barrier_wait(&p->start_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("high_priority[%d]: pthread_barrier_wait(start): %x",
			     p->id, status);
			return NULL;
		}

		pi_debug("high_priority[%d]: entering running state\n", p->id);
		status = pthread_barrier_wait(&p->locked_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("high_priority[%d]: pthread_barrier_wait(running): %x",
			     p->id, status);
			return NULL;
		}
		pi_debug("high_priority[%d]: locking mutex\n", p->id);
		pthread_mutex_lock(&p->mutex);
		pi_debug("high_priority[%d]: got mutex\n", p->id);

		pi_debug("high_priority[%d]: unlocking mutex\n", p->id);
		pthread_mutex_unlock(&p->mutex);
		pi_debug("high_priority[%d]: entering finish state\n", p->id);

		status = pthread_barrier_wait(&p->finish_barrier);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("high_priority[%d]: pthread_barrier_wait(finish): %x",
			     status);
			return NULL;
		}
		/* update the group stats */
		p->total++;

		/* update the watchdog counter */
		p->watchdog++;

	}
	set_shutdown_flag();

	pi_debug("high_priority[%d]: entering done barrier\n", p->id);

	if (have_errors == 0) {
		/* wait for all threads to finish */
		status = pthread_barrier_wait(&all_threads_done);
		if (status && status != PTHREAD_BARRIER_SERIAL_THREAD) {
			pi_error
			    ("high_priority[%d]: pthread_barrier_wait(all_threads_done): %x",
			     p->id, status);
			return NULL;
		}
	}
	/* exit */
	pi_debug("high_priority[%d]: exiting\n", p->id);
	return NULL;
}

void usage(void)
{
	printf("usage: pi_stress <options>\n");
	printf("    options:\n");
	printf("\t--verbose\t- lots of output\n");
	printf("\t--quiet\t\t- suppress running output\n");
	printf
	    ("\t--duration=<n>- length of the test run in seconds [infinite]\n");
	printf("\t--groups=<n>\t- set the number of inversion groups [%d]\n",
	       ngroups);
	printf
	    ("\t--inversions=<n>- number of inversions per group [infinite]\n");
	printf("\t--report=<path>\t- output to file [/dev/null]\n");
	printf("\t--rr\t\t- use SCHED_RR for test threads [SCHED_FIFO]\n");
	printf("\t--sched\t\t- scheduling options per thread type:\n");
	printf("\t\tid=[high|med|low]\t\t\t- select thread\n");
	printf("\t\t,policy=[fifo,rr],priority=<n>\t\t- SCHED_FIFO or SCHED_RR\n");
	printf("\t\t,policy=deadline,runtime=<n>,deadline=<n>,period=<n>\t- SCHED_DEADLINE\n");
	printf("\t--prompt\t- prompt before starting the test\n");
	printf
	    ("\t--uniprocessor\t- force all threads to run on one processor\n");
	printf("\t--mlockall\t- lock current and future memory\n");
	printf("\t--debug\t\t- turn on debug prints\n");
	printf("\t--version\t- print version number on output\n");
	printf("\t--help\t\t- print this message\n");
}

/* block all signals (called from main) */
int block_signals(void)
{
	int status;
	sigset_t sigset;

	/* mask off all signals */
	status = sigfillset(&sigset);
	if (status) {
		pi_error("setting up full signal set %s\n", strerror(status));
		return FAILURE;
	}
	status = pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	if (status) {
		pi_error("setting signal mask: %s\n", strerror(status));
		return FAILURE;
	}
	return SUCCESS;
}

/* allow SIGTERM delivery (called from worker threads) */
int allow_sigterm(void)
{
	int status;
	sigset_t sigset;

	status = sigemptyset(&sigset);
	if (status) {
		pi_error("creating empty signal set: %s\n", strerror(status));
		return FAILURE;
	}
	status = sigaddset(&sigset, SIGTERM);
	if (status) {
		pi_error("adding SIGTERM to signal set: %s\n", strerror(status));
		return FAILURE;
	}
	status = pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
	if (status) {
		pi_error("unblocking SIGTERM: %s\n", strerror(status));
		return FAILURE;
	}
	return SUCCESS;
}

/* clean up before exiting */
void set_shutdown_flag(void)
{
	pthread_mutex_lock(&shutdown_mtx);
	if (shutdown == 0) {
		/* tell anyone that's looking that we're done */
		pi_info("setting shutdown flag\n");
		shutdown = 1;
	}
	pthread_mutex_unlock(&shutdown_mtx);
}

/* set up a test group */
int initialize_group(struct group_parameters *group)
{
	int status;
	pthread_mutexattr_t mutex_attr;

	group->inversions = inversions;

	/* setup default attributes for the group mutex */
	/* (make it a PI mutex) */
	status = pthread_mutexattr_init(&mutex_attr);
	if (status) {
		pi_error("initializing mutex attribute: %s\n", strerror(status));
		return FAILURE;
	}

	/* set priority inheritance attribute for mutex */
	status = pthread_mutexattr_setprotocol(&mutex_attr,
					       PTHREAD_PRIO_INHERIT);
	if (status) {
		pi_error("setting mutex attribute policy: %s\n", strerror(status));
		return FAILURE;
	}
	/* initialize the group mutex */
	status = pthread_mutex_init(&group->mutex, &mutex_attr);
	if (status) {
		pi_error("initializing mutex: %s\n", strerror(status));
		return FAILURE;
	}

	/* initialize the group barriers */
	if (barrier_init(&group->start_barrier, NULL, NUM_TEST_THREADS,
			 "start_barrier"))
		return FAILURE;

	if (barrier_init(&group->locked_barrier, NULL, 2, "locked_barrier"))
		return FAILURE;

	if (barrier_init(&group->elevate_barrier, NULL, 2, "elevate_barrier"))
		return FAILURE;

	if (barrier_init
	    (&group->finish_barrier, NULL, NUM_TEST_THREADS, "finish_barrier"))
		return FAILURE;

	if (barrier_init(&group->loop_barr, NULL, NUM_TEST_THREADS,
			 "loop_barrier"))
		return FAILURE;

	if ((status = pthread_mutex_init(&group->loop_mtx, NULL)) != 0) {
		pi_error("pthread_mutex_init, status = %d\n", status);
		return FAILURE;
	}

	if ((status = pthread_mutex_lock(&group->loop_mtx)) != 0) {
		pi_error("pthread_mutex_lock, status = %d\n", status);
		return FAILURE;
	}

	group->loop = 1;

	if ((status = pthread_mutex_unlock(&group->loop_mtx)) != 0) {
		pi_error("pthread_mutex_unlock, status = %d\n", status);
		return FAILURE;
	}

	return SUCCESS;
}

/* setup and create a groups threads */
int create_group(struct group_parameters *group)
{
	int status;
	pthread_attr_t thread_attr;
	cpu_set_t mask;

	/* initialize group structure */
	status = initialize_group(group);
	if (status) {
		pi_error("initializing group %d\n", group->id);
		return FAILURE;
	}

	CPU_ZERO(&mask);
	CPU_SET(group->cpu, &mask);

	pi_debug("group %d bound to cpu %ld\n", group->id, group->cpu);

	/* start the low priority thread */
	pi_debug("creating low priority thread\n");
	if (setup_thread_attr(&thread_attr, &low_sa, &mask))
		return FAILURE;
	status = pthread_create(&group->low_tid,
				&thread_attr, low_priority, group);
	if (status != 0) {
		pi_error("creating low_priority thread: %s\n", strerror(status));
		return FAILURE;
	}

	/* create the medium priority thread */
	pi_debug("creating medium priority thread\n");
	if (setup_thread_attr(&thread_attr, &med_sa, &mask))
		return FAILURE;
	status = pthread_create(&group->med_tid,
				&thread_attr, med_priority, group);
	if (status != 0) {
		pi_error("creating med_priority thread: %s\n", strerror(status));
		return FAILURE;
	}

	/* create the high priority thread */
	pi_debug("creating high priority thread\n");
	if (setup_thread_attr(&thread_attr, &high_sa, &mask))
		return FAILURE;
	status = pthread_create(&group->high_tid,
				&thread_attr, high_priority, group);
	if (status != 0) {
		pi_error("creating high_priority thread: %s\n", strerror(status));
		set_shutdown_flag();
		return FAILURE;
	}
	return SUCCESS;
}

unsigned long parse_unsigned(const char *str)
{
	unsigned long n;
	char *p;

	errno = 0;
	n = strtoul(str, &p, 10);

	if ((errno == ERANGE && n == ULONG_MAX)
			|| (errno != 0 && n == 0)) {
		pi_error("parsing number failed: %s\n", str);
		exit(EXIT_FAILURE);
	}

	return n;
}

long parse_signed(const char *str)
{
	long n;
	char *p;

	errno = 0;
	n = strtol(str, &p, 10);

	if ((errno == ERANGE && (n == LONG_MAX || n == LONG_MIN))
			|| (errno != 0 && n == 0)) {
		pi_error("parsing number failed: %s\n", str);
		exit(EXIT_FAILURE);
	}

	return n;
}

int process_sched_line(const char *arg)
{
	char *buf, *k, *v;
	const char del[] = ",=";
	struct sched_attr sa = { 0, };
	char *id = NULL;
	int retval = SUCCESS;

	buf = strdupa(arg);

	k = strsep(&buf, del);
	while (k) {
		v = strsep(&buf, del);
		if (!v)
			break;

		if (!strcmp(k, "id"))
			id = v;
		else if (!strcmp(k, "policy"))
			sa.sched_policy = string_to_policy(v);
		else if (!strcmp(k, "nice"))
			sa.sched_nice = parse_signed(v);
		else if (!strcmp(k, "priority"))
			sa.sched_priority = parse_unsigned(v);
		else if (!strcmp(k, "runtime"))
			sa.sched_runtime = parse_unsigned(v);
		else if (!strcmp(k, "deadline"))
			sa.sched_deadline = parse_unsigned(v);
		else if (!strcmp(k, "period"))
			sa.sched_period = parse_unsigned(v);

		k = strsep(&buf, del);
	}

	if (!id) {
		free(buf);
		return FAILURE;
	}

	/* We do not validate the options, instead we pass all garbage
	 * to the kernel and see what's happening */

	if (!strcmp(id, "low")) {
		memcpy(&low_sa, &sa, sizeof(struct sched_attr));
		sa_initialized |= SA_INIT_LOW;
	} else if (!strcmp(id, "med")) {
		memcpy(&med_sa, &sa, sizeof(struct sched_attr));
		sa_initialized |= SA_INIT_MED;
	} else if (!strcmp(id, "high")) {
		memcpy(&high_sa, &sa, sizeof(struct sched_attr));
		sa_initialized |= SA_INIT_HIGH;
	} else {
		retval = FAILURE;
	}

	free(buf);
	return retval;
}

void process_command_line(int argc, char **argv)
{
	int opt;
	while ((opt = getopt_long(argc, argv, "+", options, NULL)) != -1) {
		switch (opt) {
		case '?':
		case 'h':
			usage();
			exit(0);
		case 't':
			duration = strtol(optarg, NULL, 10);
			break;
		case 'v':
			verbose = 1;
			quiet = 0;
			break;
		case 'q':
			verbose = 0;
			quiet = 1;
			break;
		case 'i':
			inversions = strtol(optarg, NULL, 10);
			pi_info("doing %d inversion per group\n", inversions);
			break;
		case 'g':
			ngroups = strtol(optarg, NULL, 10);
			pi_info("number of groups set to %d\n", ngroups);
			break;
		case 'r':
			policy = SCHED_RR;
			break;
		case 's':
			if (process_sched_line(optarg))
				pi_error("ignoring invalid options '%s'\n", optarg);
			break;
		case 'p':
			prompt = 1;
			break;
		case 'd':
			debugging = 1;
			break;
		case 'V':
			printf("pi_stress v%1.2f ", VERSION);
			exit(0);
		case 'u':
			uniprocessor = 1;
			break;
		case 'm':
			lockall = 1;
			break;
		}
	}
}

/* total the number of inversions that have been performed */
unsigned long total_inversions(void)
{
	int i;
	unsigned long total = 0;

	for (i = 0; i < ngroups; i++)
		total += groups[i].total;
	return total;
}

void print_sched_attr(const char *name, struct sched_attr * sa)
{
	printf("    %6s thread", name);
	printf(" %s", policy_to_string(sa->sched_policy));

	switch(sa->sched_policy) {
	case SCHED_OTHER:
		printf(" nice %d\n", sa->sched_nice);
		break;
	case SCHED_FIFO:
	case SCHED_RR:
		printf(" priority %d\n", sa->sched_priority);
		break;
	case SCHED_DEADLINE:
		printf(" runtime %" PRIu64 " deadline %" PRIu64 " period %" PRIu64 "\n",
			sa->sched_runtime, sa->sched_deadline,
			sa->sched_period);
		break;
	}
}

void banner(void)
{
	if (quiet)
		return;

	printf("Starting PI Stress Test\n");
	printf("Number of thread groups: %d\n", ngroups);
	if (duration >= 0)
		printf("Duration of test run: %d seconds\n", duration);
	else
		printf("Duration of test run: infinite\n");
	if (inversions < 0)
		printf("Number of inversions per group: unlimited\n");
	else
		printf("Number of inversions per group: %d\n", inversions);
	print_sched_attr("Admin", &admin_sa);
	printf("%d groups of 3 threads will be created\n", ngroups);
	print_sched_attr("High", &high_sa);
	print_sched_attr("Med", &med_sa);
	print_sched_attr("Low", &low_sa);
	printf("\n");
}

void summary(void)
{
	time_t interval = finish - start;
	struct tm *t = gmtime(&interval);

	printf("Total inversion performed: %lu\n", total_inversions());
	printf("Test Duration: %d days, %d hours, %d minutes, %d seconds\n",
	       t->tm_yday, t->tm_hour, t->tm_min, t->tm_sec);
}

int
barrier_init(pthread_barrier_t * b, const pthread_barrierattr_t * attr,
	     unsigned count, const char *name)
{
	int status;

	if ((status = pthread_barrier_init(b, attr, count)) != 0) {
		pi_error("barrier_init: failed to initialize: %s\n", name);
		pi_error("status = %d\n", status);
		return FAILURE;
	}

	return SUCCESS;
}

void setup_sched_attr(struct sched_attr *attr, int policy, int prio)
{
	attr->sched_policy = policy;
	attr->sched_priority = prio;
}

void setup_sched_config(int policy)
{
	int prio_min;

	prio_min = sched_get_priority_min(policy);

	if (!(sa_initialized & SA_INIT_LOW))
		setup_sched_attr(&low_sa,   policy, prio_min + 0);
	if (!(sa_initialized & SA_INIT_MED))
		setup_sched_attr(&med_sa,   policy, prio_min + 1);
	if (!(sa_initialized & SA_INIT_HIGH))
		setup_sched_attr(&high_sa,  policy, prio_min + 2);
	if (!(sa_initialized & SA_INIT_ADMIN))
		setup_sched_attr(&admin_sa, policy, prio_min + 3);
}
