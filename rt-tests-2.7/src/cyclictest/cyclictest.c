// SPDX-License-Identifier: GPL-2.0-only
/*
 * High resolution timer test software
 *
 * (C) 2013      Clark Williams <williams@redhat.com>
 * (C) 2013      John Kacur <jkacur@redhat.com>
 * (C) 2008-2012 Clark Williams <williams@redhat.com>
 * (C) 2005-2007 Thomas Gleixner <tglx@linutronix.de>
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include "rt_numa.h"

#include "rt-utils.h"
#include "rt-numa.h"
#include "rt-error.h"
#include "histogram.h"

#include <bionic.h>

#define DEFAULT_INTERVAL 1000
#define DEFAULT_DISTANCE 500

#ifndef SCHED_IDLE
#define SCHED_IDLE 5
#endif
#ifndef SCHED_NORMAL
#define SCHED_NORMAL SCHED_OTHER
#endif
#ifndef LOONGARCH_MUSL
#define sigev_notify_thread_id _sigev_un._tid
#endif

#ifdef __UCLIBC__
#define MAKE_PROCESS_CPUCLOCK(pid, clock) \
	((~(clockid_t) (pid) << 3) | (clockid_t) (clock))
#define CPUCLOCK_SCHED          2

static int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *req,
			   struct timespec *rem)
{
	if (clock_id == CLOCK_THREAD_CPUTIME_ID)
		return -EINVAL;
	if (clock_id == CLOCK_PROCESS_CPUTIME_ID)
		clock_id = MAKE_PROCESS_CPUCLOCK(0, CPUCLOCK_SCHED);

	return syscall(__NR_clock_nanosleep, clock_id, flags, req, rem);
}

int sched_setaffinity(__pid_t __pid, size_t __cpusetsize,
		       __const cpu_set_t *__cpuset)
{
	return -EINVAL;
}

#undef CPU_SET
#undef CPU_ZERO
#define CPU_SET(cpu, cpusetp)
#define CPU_ZERO(cpusetp)

#else
extern int clock_nanosleep(clockid_t __clock_id, int __flags,
			   __const struct timespec *__req,
			   struct timespec *__rem);
#endif	/* __UCLIBC__ */

#define HIST_MAX		1000000

#define MODE_CYCLIC		0
#define MODE_CLOCK_NANOSLEEP	1
#define MODE_SYS_ITIMER		2
#define MODE_SYS_NANOSLEEP	3
#define MODE_SYS_OFFSET		2

#define TIMER_RELTIME		0

/* Must be power of 2 ! */
#define VALBUF_SIZE		16384

#if (defined(__i386__) || defined(__x86_64__))
#define ARCH_HAS_SMI_COUNTER
#endif

#define MSR_SMI_COUNT		0x00000034
#define MSR_SMI_COUNT_MASK	0xFFFFFFFF

static char *policyname(int policy);

/* Struct to transfer parameters to the thread */
struct thread_param {
	int prio;
	int policy;
	int mode;
	int timermode;
	int signal;
	int clock;
	unsigned long max_cycles;
	struct thread_stat *stats;
	int bufmsk;
	unsigned long interval;
	int cpu;
	int node;
	int tnum;
	int msr_fd;
};

/* Struct for statistics */
struct thread_stat {
	unsigned long cycles;
	unsigned long cyclesread;
	long min;
	long max;
	long act;
	double avg;
	long *values;
	long *smis;
	struct histogram *hist;
	pthread_t thread;
	int threadstarted;
	int tid;
	long reduce;
	long redmax;
	long cycleofmax;
	unsigned long smi_count;
};

static pthread_mutex_t trigger_lock = PTHREAD_MUTEX_INITIALIZER;

static int trigger = 0;	/* Record spikes > trigger, 0 means don't record */
static int trigger_list_size = 1024;	/* Number of list nodes */

/* Info to store when the diff is greater than the trigger */
struct thread_trigger {
	int cpu;
	int tnum;	/* thread number */
	int64_t  ts;	/* time-stamp */
	int diff;
	struct thread_trigger *next;
};

struct thread_trigger *head = NULL;
struct thread_trigger *tail = NULL;
struct thread_trigger *current = NULL;
static int spikes;	/* count of the number of spikes */

static int trigger_init();
static void trigger_print();
static void trigger_update(struct thread_param *par, int diff, int64_t ts);

static int shutdown;
static int tracelimit = 0;
static int trace_marker = 0;
static int verbose = 0;
static int oscope_reduction = 1;
static int lockall = 0;
static int histogram = 0;
static int histofall = 0;
static int duration = 0;
static int use_nsecs = 0;
static int refresh_on_max;
static int force_sched_other;
static int priospread = 0;
static int check_clock_resolution;
static int ct_debug;
static int use_fifo = 0;
static pthread_t fifo_threadid;
static int laptop = 0;
static int power_management = 0;
static int use_histfile = 0;

#ifdef ARCH_HAS_SMI_COUNTER
static int smi = 0;
#else
#define smi	0
#endif

static pthread_cond_t refresh_on_max_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t refresh_on_max_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t break_thread_id_lock = PTHREAD_MUTEX_INITIALIZER;
static pid_t break_thread_id = 0;
static uint64_t break_thread_value = 0;

static int aligned = 0;
static int secaligned = 0;
static int offset = 0;
static pthread_barrier_t align_barr;
static pthread_barrier_t globalt_barr;
static struct timespec globalt;

static char fifopath[MAX_PATH];
static char histfile[MAX_PATH];
static char jsonfile[MAX_PATH];

static struct thread_param **parameters;
static struct thread_stat **statistics;
static struct histoset hset;

static void print_stat(FILE *fp, struct thread_param *par, int index, int verbose, int quiet);
static void rstat_print_stat(struct thread_param *par, int index, int verbose, int quiet);
static void rstat_setup(void);

static int latency_target_fd = -1;
static int32_t latency_target_value = 0;

static int rstat_ftruncate(int fd, off_t len);
static int rstat_fd = -1;
/* strlen("/cyclictest") + digits in max pid len + '\0' */
#define SHM_BUF_SIZE 19
static char shm_name[SHM_BUF_SIZE];

/* Latency trick
 * if the file /dev/cpu_dma_latency exists,
 * open it and write a zero into it. This will tell
 * the power management system not to transition to
 * a high cstate (in fact, the system acts like idle=poll)
 * When the fd to /dev/cpu_dma_latency is closed, the behavior
 * goes back to the system default.
 *
 * Documentation/power/pm_qos_interface.txt
 */
static void set_latency_target(void)
{
	struct stat s;
	int err;

	if (laptop) {
		warn("not setting cpu_dma_latency to save battery power\n");
		return;
	}

	if (power_management) {
		warn("not setting cpu_dma_latency from cyclictest\n");
		return;
	}

	errno = 0;
	err = stat("/dev/cpu_dma_latency", &s);
	if (err == -1) {
		err_msg_n(errno, "WARN: stat /dev/cpu_dma_latency failed");
		return;
	}

	errno = 0;
	latency_target_fd = open("/dev/cpu_dma_latency", O_RDWR);
	if (latency_target_fd == -1) {
		err_msg_n(errno, "WARN: open /dev/cpu_dma_latency");
		return;
	}

	errno = 0;
	err = write(latency_target_fd, &latency_target_value, 4);
	if (err < 1) {
		err_msg_n(errno, "# error setting cpu_dma_latency to %d!", latency_target_value);
		close(latency_target_fd);
		return;
	}
	printf("# /dev/cpu_dma_latency set to %dus\n", latency_target_value);
}


enum {
	ERROR_GENERAL	= -1,
	ERROR_NOTFOUND	= -2,
};

/*
 * Raise the soft priority limit up to prio, if that is less than or equal
 * to the hard limit
 * if a call fails, return the error
 * if successful return 0
 * if fails, return -1
 */
static int raise_soft_prio(int policy, const struct sched_param *param)
{
	int err;
	int policy_max;	/* max for scheduling policy such as SCHED_FIFO */
	int soft_max;
	int hard_max;
	int prio;
	struct rlimit rlim;

	prio = param->sched_priority;

	policy_max = sched_get_priority_max(policy);
	if (policy_max == -1) {
		err = errno;
		err_msg("WARN: no such policy\n");
		return err;
	}

	err = getrlimit(RLIMIT_RTPRIO, &rlim);
	if (err) {
		err = errno;
		err_msg_n(err, "WARN: getrlimit failed");
		return err;
	}

	soft_max = (rlim.rlim_cur == RLIM_INFINITY) ? (unsigned int)policy_max
						    : rlim.rlim_cur;
	hard_max = (rlim.rlim_max == RLIM_INFINITY) ? (unsigned int)policy_max
						    : rlim.rlim_max;

	if (prio > soft_max && prio <= hard_max) {
		rlim.rlim_cur = prio;
		err = setrlimit(RLIMIT_RTPRIO, &rlim);
		if (err) {
			err = errno;
			err_msg_n(err, "WARN: setrlimit failed");
			/* return err; */
		}
	} else {
		err = -1;
	}

	return err;
}

/*
 * Check the error status of sched_setscheduler
 * If an error can be corrected by raising the soft limit priority to
 * a priority less than or equal to the hard limit, then do so.
 */
static int setscheduler(pid_t pid, int policy, const struct sched_param *param)
{
	int err = 0;

try_again:
	err = sched_setscheduler(pid, policy, param);
	if (err) {
		err = errno;
		if (err == EPERM) {
			int err1;
			err1 = raise_soft_prio(policy, param);
			if (!err1)
				goto try_again;
		}
	}

	return err;
}

#ifdef ARCH_HAS_SMI_COUNTER
static int open_msr_file(int cpu)
{
	int fd;
	char pathname[32];

	/* SMI needs thread affinity */
	sprintf(pathname, "/dev/cpu/%d/msr", cpu);
	fd = open(pathname, O_RDONLY);
	if (fd < 0)
		warn("%s open failed, try chown or chmod +r "
		       "/dev/cpu/*/msr, or run as root\n", pathname);

	return fd;
}

static int get_msr(int fd, off_t offset, unsigned long long *msr)
{
	ssize_t retval;

	retval = pread(fd, msr, sizeof *msr, offset);

	if (retval != sizeof *msr)
		return 1;

	return 0;
}

static int get_smi_counter(int fd, unsigned long *counter)
{
	int retval;
	unsigned long long msr;

	retval = get_msr(fd, MSR_SMI_COUNT, &msr);
	if (retval)
		return retval;

	*counter = (unsigned long) (msr & MSR_SMI_COUNT_MASK);

	return 0;
}

#include <cpuid.h>

/* Guess if the CPU has an SMI counter in the model-specific registers (MSR).
 *
 * This is true for Intel x86 CPUs after Nehalem (2008). However, it's not
 * possible to detect the feature directly, (or at least, turbostat.c doesn't
 * know how to do it either) so we assume it's true for all x86 CPUs.
 * Previously, this function had an explicit allowlist, which required updates
 * every time a new CPU generation was released.
 */
static int has_smi_counter(void)
{
	unsigned int ebx, ecx, edx, max_level;
	unsigned int fms, family, model;

	fms = family = model = ebx = ecx = edx = 0;

	__get_cpuid(0, &max_level, &ebx, &ecx, &edx);

	/* check genuine intel */
	if (!(ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e))
		return 0;

	__get_cpuid(1, &fms, &ebx, &ecx, &edx);
	family = (fms >> 8) & 0xf;

	if (family != 6)
		return 0;

	/* no MSR */
	if (!(edx & (1 << 5)))
		return 0;

	return 1;
}
#else
static int open_msr_file(int cpu)
{
	return -1;
}

static int get_smi_counter(int fd, unsigned long *counter)
{
	return 1;
}
static int has_smi_counter(void)
{
	return 0;
}
#endif

/*
 * timer thread
 *
 * Modes:
 * - clock_nanosleep based
 * - cyclic timer based
 *
 * Clock:
 * - CLOCK_MONOTONIC
 * - CLOCK_REALTIME
 *
 */
static void *timerthread(void *param)
{
	struct thread_param *par = param;
	struct sched_param schedp;
	struct sigevent sigev;
	sigset_t sigset;
	timer_t timer;
	struct timespec now, next, interval, stop = { 0 };
	struct itimerval itimer;
	struct itimerspec tspec;
	struct thread_stat *stat = par->stats;
	int stopped = 0;
	cpu_set_t mask;
	pthread_t thread;
	unsigned long smi_now, smi_old = 0;

	/* if we're running in numa mode, set our memory node */
	if (par->node != -1)
		rt_numa_set_numa_run_on_node(par->node, par->cpu);

	if (par->cpu != -1) {
		CPU_ZERO(&mask);
		CPU_SET(par->cpu, &mask);
		thread = pthread_self();
		if (pthread_setaffinity_np(thread, sizeof(mask), &mask) != 0)
			warn("Could not set CPU affinity to CPU #%d\n",
			     par->cpu);
	}

	interval.tv_sec = par->interval / USEC_PER_SEC;
	interval.tv_nsec = (par->interval % USEC_PER_SEC) * 1000;

	stat->tid = gettid();

	sigemptyset(&sigset);
	sigaddset(&sigset, par->signal);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	if (par->mode == MODE_CYCLIC) {
		sigev.sigev_notify = SIGEV_THREAD_ID | SIGEV_SIGNAL;
		sigev.sigev_signo = par->signal;
		sigev.sigev_notify_thread_id = stat->tid;
		timer_create(par->clock, &sigev, &timer);
		tspec.it_interval = interval;
	}

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = par->prio;
	if (setscheduler(0, par->policy, &schedp))
		fatal("timerthread%d: failed to set priority to %d\n",
		      par->cpu, par->prio);

	if (smi) {
		par->msr_fd = open_msr_file(par->cpu);
		if (par->msr_fd < 0)
			fatal("Could not open MSR interface, errno: %d\n",
				errno);
		/* get current smi count to use as base value */
		if (get_smi_counter(par->msr_fd, &smi_old))
			fatal("Could not read SMI counter, errno: %d\n",
				par->cpu, errno);
	}

	/* Get current time */
	if (aligned || secaligned) {
		pthread_barrier_wait(&globalt_barr);
		if (par->tnum == 0) {
			clock_gettime(par->clock, &globalt);
			if (secaligned) {
				/* Ensure that the thread start timestamp is not
				 * in the past */
				if (globalt.tv_nsec > 900000000)
					globalt.tv_sec += 2;
				else
					globalt.tv_sec++;
				globalt.tv_nsec = 0;
			}
		}
		pthread_barrier_wait(&align_barr);
		now = globalt;
		if (offset) {
			if (aligned)
				now.tv_nsec += offset * par->tnum;
			else
				now.tv_nsec += offset;
			tsnorm(&now);
		}
	} else
		clock_gettime(par->clock, &now);

	next = now;
	next.tv_sec += interval.tv_sec;
	next.tv_nsec += interval.tv_nsec;
	tsnorm(&next);

	if (duration) {
		stop = now;
		stop.tv_sec += duration;
	}
	if (par->mode == MODE_CYCLIC) {
		if (par->timermode == TIMER_ABSTIME)
			tspec.it_value = next;
		else
			tspec.it_value = interval;
		timer_settime(timer, par->timermode, &tspec, NULL);
	}

	if (par->mode == MODE_SYS_ITIMER) {
		itimer.it_interval.tv_sec = interval.tv_sec;
		itimer.it_interval.tv_usec = interval.tv_nsec / 1000;
		itimer.it_value = itimer.it_interval;
		setitimer(ITIMER_REAL, &itimer, NULL);
	}

	stat->threadstarted++;

	while (!shutdown) {

		uint64_t diff;
		unsigned long diff_smi = 0;
		int sigs, ret;

		/* Wait for next period */
		switch (par->mode) {
		case MODE_CYCLIC:
		case MODE_SYS_ITIMER:
			if (sigwait(&sigset, &sigs) < 0)
				goto out;
			break;

		case MODE_CLOCK_NANOSLEEP:
			if (par->timermode == TIMER_ABSTIME) {
				ret = clock_nanosleep(par->clock, TIMER_ABSTIME,
						      &next, NULL);
				if (ret != 0) {
					if (ret != EINTR)
						warn("clock_nanosleep failed. errno: %d\n", errno);
					goto out;
				}
			} else {
				ret = clock_gettime(par->clock, &now);
				if (ret != 0) {
					if (ret != EINTR)
						warn("clock_gettime() failed: %s", strerror(errno));
					goto out;
				}
				ret = clock_nanosleep(par->clock,
					TIMER_RELTIME, &interval, NULL);
				if (ret != 0) {
					if (ret != EINTR)
						warn("clock_nanosleep() failed. errno: %d\n", errno);
					goto out;
				}
				next.tv_sec = now.tv_sec + interval.tv_sec;
				next.tv_nsec = now.tv_nsec + interval.tv_nsec;
				tsnorm(&next);
			}
			break;

		case MODE_SYS_NANOSLEEP:
			ret = clock_gettime(par->clock, &now);
			if (ret != 0) {
				if (ret != EINTR)
					warn("clock_gettime() failed: errno %d\n", errno);
				goto out;
			}
			if (nanosleep(&interval, NULL)) {
				if (errno != EINTR)
					warn("nanosleep failed. errno: %d\n",
					     errno);
				goto out;
			}
			next.tv_sec = now.tv_sec + interval.tv_sec;
			next.tv_nsec = now.tv_nsec + interval.tv_nsec;
			tsnorm(&next);
			break;
		}
		ret = clock_gettime(par->clock, &now);
		if (ret != 0) {
			if (ret != EINTR)
				warn("clock_gettime() failed. errno: %d\n",
				     errno);
			goto out;
		}

		if (smi) {
			if (get_smi_counter(par->msr_fd, &smi_now)) {
				warn("Could not read SMI counter, errno: %d\n",
					par->cpu, errno);
				goto out;
			}
			diff_smi = smi_now - smi_old;
			stat->smi_count += diff_smi;
			smi_old = smi_now;
		}

		if (use_nsecs)
			diff = calcdiff_ns(now, next);
		else
			diff = calcdiff(now, next);
		if (diff < stat->min)
			stat->min = diff;
		if (diff > stat->max) {
			stat->max = diff;
			if (refresh_on_max)
				pthread_cond_signal(&refresh_on_max_cond);
		}
		stat->avg += (double) diff;

		if (trigger && (diff > trigger))
			trigger_update(par, diff, calctime(now));

		if (duration && (calcdiff(now, stop) >= 0))
			shutdown++;

		if (!stopped && tracelimit && (diff > tracelimit)) {
			stopped++;
			shutdown++;
			pthread_mutex_lock(&break_thread_id_lock);
			if (break_thread_id == 0) {
				break_thread_id = stat->tid;
				tracemark("hit latency threshold (%llu > %d)",
					  (unsigned long long) diff, tracelimit);
				break_thread_value = diff;
			}
			pthread_mutex_unlock(&break_thread_id_lock);
		}
		stat->act = diff;

		if (par->bufmsk) {
			stat->values[stat->cycles & par->bufmsk] = diff;
			if (smi)
				stat->smis[stat->cycles & par->bufmsk] = diff_smi;
		}

		/* Update the histogram */
		if (histogram)
			hist_sample(stat->hist, diff);

		stat->cycles++;

		next.tv_sec += interval.tv_sec;
		next.tv_nsec += interval.tv_nsec;
		if (par->mode == MODE_CYCLIC) {
			int overrun_count = timer_getoverrun(timer);
			next.tv_sec += overrun_count * interval.tv_sec;
			next.tv_nsec += overrun_count * interval.tv_nsec;
		}
		tsnorm(&next);

		while (tsgreater(&now, &next)) {
			next.tv_sec += interval.tv_sec;
			next.tv_nsec += interval.tv_nsec;
			tsnorm(&next);
		}

		if (par->max_cycles && par->max_cycles == stat->cycles)
			break;
	}

out:
	if (refresh_on_max) {
		pthread_mutex_lock(&refresh_on_max_lock);
		/* We could reach here with both shutdown and allstopped unset (0).
		 * Set shutdown with synchronization to notify the main
		 * thread not to be blocked when it should exit.
		 */
		shutdown++;
		pthread_cond_signal(&refresh_on_max_cond);
		pthread_mutex_unlock(&refresh_on_max_lock);
	}

	if (par->mode == MODE_CYCLIC)
		timer_delete(timer);

	if (par->mode == MODE_SYS_ITIMER) {
		itimer.it_value.tv_sec = 0;
		itimer.it_value.tv_usec = 0;
		itimer.it_interval.tv_sec = 0;
		itimer.it_interval.tv_usec = 0;
		setitimer(ITIMER_REAL, &itimer, NULL);
	}

	/* close msr file */
	if (smi)
		close(par->msr_fd);
	/* switch to normal */
	schedp.sched_priority = 0;
	sched_setscheduler(0, SCHED_OTHER, &schedp);
	stat->threadstarted = -1;

	return NULL;
}


/* Print usage information */
static void display_help(int error)
{
	printf("cyclictest V %1.2f\n", VERSION);
	printf("Usage:\n"
	       "cyclictest <options>\n\n"
	       "-a [CPUSET] --affinity     Run thread #N on processor #N, if possible, or if CPUSET\n"
	       "                           given, pin threads to that set of processors in round-\n"
	       "                           robin order.  E.g. -a 2 pins all threads to CPU 2,\n"
	       "                           but -a 3-5,0 -t 5 will run the first and fifth\n"
	       "                           threads on CPU (0),thread #2 on CPU 3, thread #3\n"
	       "                           on CPU 4, and thread #5 on CPU 5.\n"
	       "-A USEC  --aligned=USEC    align thread wakeups to a specific offset\n"
	       "-b USEC  --breaktrace=USEC send break trace command when latency > USEC\n"
	       "-c CLOCK --clock=CLOCK     select clock\n"
	       "                           0 = CLOCK_MONOTONIC (default)\n"
	       "                           1 = CLOCK_REALTIME\n"
	       "         --default-system  Don't attempt to tune the system from cyclictest.\n"
	       "                           Power management is not suppressed.\n"
	       "                           This might give poorer results, but will allow you\n"
	       "                           to discover if you need to tune the system\n"
	       "-d DIST  --distance=DIST   distance of thread intervals in us, default=500\n"
	       "-D       --duration=TIME   specify a length for the test run.\n"
	       "                           Append 'm', 'h', or 'd' to specify minutes, hours or days.\n"
	       "-F       --fifo=<path>     create a named pipe at path and write stats to it\n"
	       "-h       --histogram=US    dump a latency histogram to stdout after the run\n"
	       "                           US is the max latency time to be tracked in microseconds\n"
	       "			   This option runs all threads at the same priority.\n"
	       "-H       --histofall=US    same as -h except with an additional summary column\n"
	       "	 --histfile=<path> dump the latency histogram to <path> instead of stdout\n"
	       "-i INTV  --interval=INTV   base interval of thread in us default=1000\n"
	       "         --json=FILENAME   write final results into FILENAME, JSON formatted\n"
	       "	 --laptop	   Save battery when running cyclictest\n"
	       "			   This will give you poorer realtime results\n"
	       "			   but will not drain your battery so quickly\n"
	       "         --latency=PM_QOS  power management latency target value\n"
	       "                           This value is written to /dev/cpu_dma_latency\n"
	       "                           and affects c-states. The default is 0\n"
	       "-l LOOPS --loops=LOOPS     number of loops: default=0(endless)\n"
	       "         --mainaffinity=CPUSET\n"
	       "			   Run the main thread on CPU #N. This only affects\n"
	       "                           the main thread and not the measurement threads\n"
	       "-m       --mlockall        lock current and future memory allocations\n"
	       "-M       --refresh_on_max  delay updating the screen until a new max\n"
	       "			   latency is hit. Useful for low bandwidth.\n"
	       "-N       --nsecs           print results in ns instead of us (default us)\n"
	       "-o RED   --oscope=RED      oscilloscope mode, reduce verbose output by RED\n"
	       "-p PRIO  --priority=PRIO   priority of highest prio thread\n"
	       "	 --policy=NAME     policy of measurement thread, where NAME may be one\n"
	       "                           of: other, normal, batch, idle, fifo or rr.\n"
	       "	 --priospread      spread priority levels starting at specified value\n"
	       "-q       --quiet           print a summary only on exit\n"
	       "-r       --relative        use relative timer instead of absolute\n"
	       "-R       --resolution      check clock resolution, calling clock_gettime() many\n"
	       "                           times.  List of clock_gettime() values will be\n"
	       "                           reported with -X\n"
	       "         --secaligned [USEC] align thread wakeups to the next full second\n"
	       "                           and apply the optional offset\n"
	       "-s       --system          use sys_nanosleep and sys_setitimer\n"
	       "-S       --smp             Standard SMP testing: options -a -t and same priority\n"
	       "                           of all threads\n"
	       "	--spike=<trigger>  record all spikes > trigger\n"
	       "	--spike-nodes=[num of nodes]\n"
	       "			   These are the maximum number of spikes we can record.\n"
	       "			   The default is 1024 if not specified\n"
#ifdef ARCH_HAS_SMI_COUNTER
               "         --smi             Enable SMI counting\n"
#endif
	       "-t       --threads         one thread per available processor\n"
	       "-t [NUM] --threads=NUM     number of threads:\n"
	       "                           without NUM, threads = max_cpus\n"
	       "                           without -t default = 1\n"
	       "         --tracemark       write a trace mark when -b latency is exceeded\n"
	       "-u       --unbuffered      force unbuffered output for live processing\n"
	       "-v       --verbose         output values on stdout for statistics\n"
	       "                           format: n:c:v n=tasknum c=count v=value in us\n"
	       "	 --dbg_cyclictest  print info useful for debugging cyclictest\n"
	       "-x	 --posix_timers    use POSIX timers instead of clock_nanosleep.\n"
		);
	if (error)
		exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
}

static int use_nanosleep = MODE_CLOCK_NANOSLEEP;
static int timermode = TIMER_ABSTIME;
static int use_system;
static int priority;
static int policy = SCHED_OTHER;	/* default policy if not specified */
static int num_threads = 1;
static int max_cycles;
static int clocksel = 0;
static int quiet;
static int interval = DEFAULT_INTERVAL;
static int distance = -1;
static struct bitmask *affinity_mask = NULL;
static struct bitmask *main_affinity_mask = NULL;
static int smp = 0;
static int setaffinity = AFFINITY_UNSPECIFIED;

static int clocksources[] = {
	CLOCK_MONOTONIC,
	CLOCK_REALTIME,
};

static void handlepolicy(char *polname)
{
	if (strncasecmp(polname, "other", 5) == 0)
		policy = SCHED_OTHER;
	else if (strncasecmp(polname, "batch", 5) == 0)
		policy = SCHED_BATCH;
	else if (strncasecmp(polname, "idle", 4) == 0)
		policy = SCHED_IDLE;
	else if (strncasecmp(polname, "fifo", 4) == 0)
		policy = SCHED_FIFO;
	else if (strncasecmp(polname, "rr", 2) == 0)
		policy = SCHED_RR;
	else	/* default policy if we don't recognize the request */
		policy = SCHED_OTHER;
}

static char *policyname(int policy)
{
	char *policystr = "";

	switch(policy) {
	case SCHED_OTHER:
		policystr = "other";
		break;
	case SCHED_FIFO:
		policystr = "fifo";
		break;
	case SCHED_RR:
		policystr = "rr";
		break;
	case SCHED_BATCH:
		policystr = "batch";
		break;
	case SCHED_IDLE:
		policystr = "idle";
		break;
	}
	return policystr;
}


enum option_values {
	OPT_AFFINITY=1, OPT_BREAKTRACE, OPT_CLOCK,
	OPT_DEFAULT_SYSTEM, OPT_DISTANCE, OPT_DURATION, OPT_LATENCY,
	OPT_FIFO, OPT_HISTOGRAM, OPT_HISTOFALL, OPT_HISTFILE,
	OPT_INTERVAL, OPT_JSON, OPT_MAINAFFINITY, OPT_LOOPS, OPT_MLOCKALL,
	OPT_REFRESH, OPT_NANOSLEEP, OPT_NSECS, OPT_OSCOPE, OPT_PRIORITY,
	OPT_QUIET, OPT_PRIOSPREAD, OPT_RELATIVE, OPT_RESOLUTION,
	OPT_SYSTEM, OPT_SMP, OPT_THREADS, OPT_TRIGGER,
	OPT_TRIGGER_NODES, OPT_UNBUFFERED, OPT_NUMA, OPT_VERBOSE,
	OPT_DBGCYCLIC, OPT_POLICY, OPT_HELP, OPT_NUMOPTS,
	OPT_ALIGNED, OPT_SECALIGNED, OPT_LAPTOP, OPT_SMI,
	OPT_TRACEMARK, OPT_POSIX_TIMERS,
};

/* Process commandline options */
static void process_options(int argc, char *argv[], int max_cpus)
{
	int error = 0;
	int option_affinity = 0;

	for (;;) {
		int option_index = 0;
		/*
		 * Options for getopt
		 * Ordered alphabetically by single letter name
		 */
		static struct option long_options[] = {
			{"affinity",         optional_argument, NULL, OPT_AFFINITY},
			{"aligned",          optional_argument, NULL, OPT_ALIGNED },
			{"breaktrace",       required_argument, NULL, OPT_BREAKTRACE },
			{"clock",            required_argument, NULL, OPT_CLOCK },
			{"default-system",   no_argument,       NULL, OPT_DEFAULT_SYSTEM },
			{"distance",         required_argument, NULL, OPT_DISTANCE },
			{"duration",         required_argument, NULL, OPT_DURATION },
			{"latency",          required_argument, NULL, OPT_LATENCY },
			{"fifo",             required_argument, NULL, OPT_FIFO },
			{"histogram",        required_argument, NULL, OPT_HISTOGRAM },
			{"histofall",        required_argument, NULL, OPT_HISTOFALL },
			{"histfile",	     required_argument, NULL, OPT_HISTFILE },
			{"interval",         required_argument, NULL, OPT_INTERVAL },
			{"json",             required_argument, NULL, OPT_JSON },
			{"laptop",	     no_argument,	NULL, OPT_LAPTOP },
			{"loops",            required_argument, NULL, OPT_LOOPS },
			{"mainaffinity",     required_argument, NULL, OPT_MAINAFFINITY},
			{"mlockall",         no_argument,       NULL, OPT_MLOCKALL },
			{"refresh_on_max",   no_argument,       NULL, OPT_REFRESH },
			{"nsecs",            no_argument,       NULL, OPT_NSECS },
			{"oscope",           required_argument, NULL, OPT_OSCOPE },
			{"priority",         required_argument, NULL, OPT_PRIORITY },
			{"quiet",            no_argument,       NULL, OPT_QUIET },
			{"priospread",       no_argument,       NULL, OPT_PRIOSPREAD },
			{"relative",         no_argument,       NULL, OPT_RELATIVE },
			{"resolution",       no_argument,       NULL, OPT_RESOLUTION },
			{"secaligned",       optional_argument, NULL, OPT_SECALIGNED },
			{"system",           no_argument,       NULL, OPT_SYSTEM },
			{"smi",              no_argument,       NULL, OPT_SMI },
			{"smp",              no_argument,       NULL, OPT_SMP },
			{"spike",	     required_argument, NULL, OPT_TRIGGER },
			{"spike-nodes",	     required_argument, NULL, OPT_TRIGGER_NODES },
			{"threads",          optional_argument, NULL, OPT_THREADS },
			{"tracemark",	     no_argument,	NULL, OPT_TRACEMARK },
			{"unbuffered",       no_argument,       NULL, OPT_UNBUFFERED },
			{"verbose",          no_argument,       NULL, OPT_VERBOSE },
			{"dbg_cyclictest",   no_argument,       NULL, OPT_DBGCYCLIC },
			{"policy",           required_argument, NULL, OPT_POLICY },
			{"help",             no_argument,       NULL, OPT_HELP },
			{"posix_timers",     no_argument,	NULL, OPT_POSIX_TIMERS },
			{NULL, 0, NULL, 0 },
		};
		int c = getopt_long(argc, argv, "a::A::b:c:d:D:F:h:H:i:l:MNo:p:mqrRsSt::uvD:x",
				    long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
		case OPT_AFFINITY:
			option_affinity = 1;
			/* smp sets AFFINITY_USEALL in OPT_SMP */
			if (smp)
				break;
			numa = 0;//numa_initialize();
			if (optarg) {
				parse_cpumask(optarg, max_cpus, &affinity_mask);
				setaffinity = AFFINITY_SPECIFIED;
			} else if (optind < argc &&
				   (atoi(argv[optind]) ||
				    argv[optind][0] == '0' ||
				    argv[optind][0] == '!' ||
				    argv[optind][0] == '+' ||
				    argv[optind][0] == 'a')) {
				parse_cpumask(argv[optind], max_cpus, &affinity_mask);
				setaffinity = AFFINITY_SPECIFIED;
			} else {
				setaffinity = AFFINITY_USEALL;
			}

			if (setaffinity == AFFINITY_SPECIFIED && !affinity_mask)
				display_help(1);
			if (verbose && affinity_mask)
				printf("Using %u cpus.\n",
					numa_bitmask_weight(affinity_mask));
			break;
		case 'A':
		case OPT_ALIGNED:
			aligned = 1;
			if (optarg != NULL)
				offset = atoi(optarg) * 1000;
			else if (optind < argc && atoi(argv[optind]))
				offset = atoi(argv[optind]) * 1000;
			else
				offset = 0;
			break;
		case 'b':
		case OPT_BREAKTRACE:
			tracelimit = atoi(optarg); break;
		case 'c':
		case OPT_CLOCK:
			clocksel = atoi(optarg); break;
		case OPT_DEFAULT_SYSTEM:
			power_management = 1; break;
		case 'd':
		case OPT_DISTANCE:
			distance = atoi(optarg); break;
		case 'D':
		case OPT_DURATION:
			duration = parse_time_string(optarg); break;
		case 'F':
		case OPT_FIFO:
			use_fifo = 1;
			strncpy(fifopath, optarg, strnlen(optarg, MAX_PATH-1));
			break;
		case 'H':
		case OPT_HISTOFALL:
			histofall = 1; /* fall through */
		case 'h':
		case OPT_HISTOGRAM:
			histogram = atoi(optarg);
			if (!histogram)
				display_help(1);
			break;
		case OPT_HISTFILE:
			use_histfile = 1;
			strncpy(histfile, optarg, strnlen(optarg, MAX_PATH-1));
			break;
		case 'i':
		case OPT_INTERVAL:
			interval = atoi(optarg); break;
		case OPT_JSON:
			strncpy(jsonfile, optarg, strnlen(optarg, MAX_PATH-1));
			break;
		case 'l':
		case OPT_LOOPS:
			max_cycles = atoi(optarg); break;
		case OPT_MAINAFFINITY:
			if (optarg) {
				parse_cpumask(optarg, max_cpus, &main_affinity_mask);
			} else if (optind < argc &&
			           (atoi(argv[optind]) ||
			            argv[optind][0] == '0' ||
			            argv[optind][0] == '!')) {
				parse_cpumask(argv[optind], max_cpus, &main_affinity_mask);
			}
			break;
		case 'm':
		case OPT_MLOCKALL:
			lockall = 1; break;
		case 'M':
		case OPT_REFRESH:
			refresh_on_max = 1; break;
		case 'N':
		case OPT_NSECS:
			use_nsecs = 1; break;
		case 'o':
		case OPT_OSCOPE:
			oscope_reduction = atoi(optarg); break;
		case 'p':
		case OPT_PRIORITY:
			priority = atoi(optarg);
			if (policy != SCHED_FIFO && policy != SCHED_RR)
				policy = SCHED_FIFO;
			break;
		case 'q':
		case OPT_QUIET:
			quiet = 1; break;
		case 'r':
		case OPT_RELATIVE:
			timermode = TIMER_RELTIME; break;
		case 'R':
		case OPT_RESOLUTION:
			check_clock_resolution = 1; break;
		case OPT_SECALIGNED:
			secaligned = 1;
			if (optarg != NULL)
				offset = atoi(optarg) * 1000;
			else if (optind < argc && atoi(argv[optind]))
				offset = atoi(argv[optind]) * 1000;
			else
				offset = 0;
			break;
		case 's':
		case OPT_SYSTEM:
			use_system = MODE_SYS_OFFSET; break;
		case 'S':
		case OPT_SMP: /* SMP testing */
			if (numa)
				fatal("numa and smp options are mutually exclusive\n");
			smp = 1;
			num_threads = -1; /* update after parsing */
			setaffinity = AFFINITY_USEALL;
			break;
		case 't':
		case OPT_THREADS:
			if (smp) {
				warn("-t ignored due to smp mode\n");
				break;
			}
			if (optarg != NULL)
				num_threads = atoi(optarg);
			else if (optind < argc && atoi(argv[optind]))
				num_threads = atoi(argv[optind]);
			else
				num_threads = -1; /* update after parsing */
			break;
		case OPT_TRIGGER:
			trigger = atoi(optarg);
			break;
		case OPT_TRIGGER_NODES:
			if (trigger)
				trigger_list_size = atoi(optarg);
			break;
		case 'u':
		case OPT_UNBUFFERED:
			setvbuf(stdout, NULL, _IONBF, 0); break;
		case 'v':
		case OPT_VERBOSE: verbose = 1; break;
		case 'x':
		case OPT_POSIX_TIMERS:
			use_nanosleep = MODE_CYCLIC; break;
		case '?':
		case OPT_HELP:
			display_help(0); break;

		/* long only options */
		case OPT_PRIOSPREAD:
			priospread = 1; break;
		case OPT_LATENCY:
                          /* power management latency target value */
			  /* note: default is 0 (zero) */
			latency_target_value = atoi(optarg);
			if (latency_target_value < 0)
				latency_target_value = 0;
			break;
		case OPT_POLICY:
			handlepolicy(optarg); break;
		case OPT_DBGCYCLIC:
			ct_debug = 1; break;
		case OPT_LAPTOP:
			laptop = 1; break;
		case OPT_SMI:
#ifdef ARCH_HAS_SMI_COUNTER
			smi = 1;
#else
			fatal("--smi is not available on your arch\n");
#endif
			break;
		case OPT_TRACEMARK:
			trace_marker = 1; break;
		}
	}

	if ((use_system == MODE_SYS_OFFSET) && (use_nanosleep == MODE_CYCLIC)) {
		warn("The system option requires clock_nanosleep\n");
		warn("and is not compatible with posix_timers\n");
		warn("Using clock_nanosleep\n");
		use_nanosleep = MODE_CLOCK_NANOSLEEP;
	}

	/* if smp wasn't requested, test for numa automatically */
	if (!smp) {
	  numa = 0;//numa_initialize();
	}

	if (option_affinity) {
		if (smp)
			warn("-a ignored due to smp mode\n");
	}

	if (smi) {
		if (setaffinity == AFFINITY_UNSPECIFIED)
			fatal("SMI counter relies on thread affinity\n");

		if (!has_smi_counter())
			fatal("SMI counter is not supported "
			      "on this processor\n");
	}

	if (clocksel < 0 || clocksel > ARRAY_SIZE(clocksources))
		error = 1;

	if (oscope_reduction < 1)
		error = 1;

	if (oscope_reduction > 1 && !verbose) {
		warn("-o option only meaningful, if verbose\n");
		error = 1;
	}

	if (histogram < 0)
		error = 1;

	if (histogram > HIST_MAX)
		histogram = HIST_MAX;

	if (histogram && distance != -1)
		warn("distance is ignored and set to 0, if histogram enabled\n");
	if (distance == -1)
		distance = DEFAULT_DISTANCE;

	if (priority < 0 || priority > 99)
		error = 1;

	if (num_threads == -1)
		num_threads = get_available_cpus(affinity_mask);

	if (priospread && priority == 0) {
		fprintf(stderr, "defaulting realtime priority to %d\n",
			num_threads+1);
		priority = num_threads+1;
	}

	if (priority && (policy != SCHED_FIFO && policy != SCHED_RR)) {
		fprintf(stderr, "policy and priority don't match: setting policy to SCHED_FIFO\n");
		policy = SCHED_FIFO;
	}

	if ((policy == SCHED_FIFO || policy == SCHED_RR) && priority == 0) {
		fprintf(stderr, "defaulting realtime priority to %d\n",
			num_threads+1);
		priority = num_threads+1;
	}

	if (num_threads < 1)
		error = 1;

	if (aligned && secaligned)
		error = 1;

	if (aligned || secaligned) {
		pthread_barrier_init(&globalt_barr, NULL, num_threads);
		pthread_barrier_init(&align_barr, NULL, num_threads);
	}
	if (error) {
		if (affinity_mask)
			rt_bitmask_free(affinity_mask);
		display_help(1);
	}
}

static int check_timer(void)
{
	struct timespec ts;

	if (clock_getres(CLOCK_MONOTONIC, &ts))
		return 1;

	return (ts.tv_sec != 0 || ts.tv_nsec != 1);
}

static void sighand(int sig)
{
	if (sig == SIGUSR1) {
		int i;
		int oldquiet = quiet;

		quiet = 0;
		fprintf(stderr, "#---------------------------\n");
		fprintf(stderr, "# cyclictest current status:\n");
		for (i = 0; i < num_threads; i++)
			print_stat(stderr, parameters[i], i, 0, 0);
		fprintf(stderr, "#---------------------------\n");
		quiet = oldquiet;
		return;
	} else if (sig == SIGUSR2) {
		int i;
		int oldquiet = quiet;

		if (rstat_fd == -1) {
			fprintf(stderr, "ERROR: rstat_fd not valid\n");
			return;
		}
		rstat_ftruncate(rstat_fd, 0);
		quiet = 0;
		dprintf(rstat_fd, "#---------------------------\n");
		dprintf(rstat_fd, "# cyclictest current status:\n");
		for (i = 0; i < num_threads; i++)
			rstat_print_stat(parameters[i], i, 0, 0);
		dprintf(rstat_fd, "#---------------------------\n");
		quiet = oldquiet;
		return;
	}
	shutdown = 1;
	if (refresh_on_max)
		pthread_cond_signal(&refresh_on_max_cond);
}

static void print_tids(struct thread_param *par[], int nthreads)
{
	int i;

	printf("# Thread Ids:");
	for (i = 0; i < nthreads; i++)
		printf(" %05d", par[i]->stats->tid);
	printf("\n");
}

static void print_hist(struct thread_param *par[], int nthreads)
{
	int i, j;
	unsigned long maxmax, alloverflows;
	FILE *fd;

	if (use_histfile) {
		fd = fopen(histfile, "w");
		if (!fd) {
			perror("opening histogram file:");
			return;
		}
	} else {
		fd = stdout;
	}

	fprintf(fd, "# Histogram\n");
	for (i = 0; i < histogram; i++) {
		unsigned long flags = 0;
		char buf[64];

		snprintf(buf, sizeof(buf), "%06d ", i);

		if (histofall)
			flags |= HSET_PRINT_SUM;
		hset_print_bucket(&hset, fd, buf, i, flags);
	}
	fprintf(fd, "# Min Latencies:");
	for (j = 0; j < nthreads; j++)
		fprintf(fd, " %05lu", par[j]->stats->min);
	fprintf(fd, "\n");
	fprintf(fd, "# Avg Latencies:");
	for (j = 0; j < nthreads; j++)
		fprintf(fd, " %05lu", par[j]->stats->cycles ?
		       (long)(par[j]->stats->avg/par[j]->stats->cycles) : 0);
	fprintf(fd, "\n");
	fprintf(fd, "# Max Latencies:");
	maxmax = 0;
	for (j = 0; j < nthreads; j++) {
		fprintf(fd, " %05lu", par[j]->stats->max);
		if (par[j]->stats->max > maxmax)
			maxmax = par[j]->stats->max;
	}
	if (histofall && nthreads > 1)
		fprintf(fd, " %05lu", maxmax);
	fprintf(fd, "\n");
	fprintf(fd, "# Histogram Overflows:");
	alloverflows = 0;
	for (j = 0; j < nthreads; j++) {
		fprintf(fd, " %05lu", par[j]->stats->hist->oflow_count);
		alloverflows += par[j]->stats->hist->oflow_count;
	}
	if (histofall && nthreads > 1)
		fprintf(fd, " %05lu", alloverflows);
	fprintf(fd, "\n");

	fprintf(fd, "# Histogram Overflow at cycle number:\n");
	for (i = 0; i < nthreads; i++) {
		fprintf(fd, "# Thread %d: ", i);
		hist_print_oflows(par[i]->stats->hist, fd);
		fprintf(fd, "\n");
	}
	if (smi) {
		fprintf(fd, "# SMIs:");
		for (i = 0; i < nthreads; i++)
			fprintf(fd, " %05lu", par[i]->stats->smi_count);
		fprintf(fd, "\n");
	}

	fprintf(fd, "\n");

	if (use_histfile)
		fclose(fd);
}

static void print_stat(FILE *fp, struct thread_param *par, int index, int verbose, int quiet)
{
	struct thread_stat *stat = par->stats;

	if (!verbose) {
		if (quiet != 1) {
			char *fmt;
			if (use_nsecs)
				fmt = "T:%2d (%5d) P:%2d I:%ld C:%7lu "
				        "Min:%7ld Act:%8ld Avg:%8ld Max:%8ld";
			else
				fmt = "T:%2d (%5d) P:%2d I:%ld C:%7lu "
				        "Min:%7ld Act:%5ld Avg:%5ld Max:%8ld";

			fprintf(fp, fmt, index, stat->tid, par->prio,
				par->interval, stat->cycles, stat->min,
				stat->act, stat->cycles ?
				(long)(stat->avg/stat->cycles) : 0, stat->max);

			if (smi)
				fprintf(fp, " SMI:%8ld", stat->smi_count);

			fprintf(fp, "\n");
		}
	} else {
		while (stat->cycles != stat->cyclesread) {
			unsigned long diff_smi;
			long diff = stat->values
			    [stat->cyclesread & par->bufmsk];

			if (smi)
				diff_smi = stat->smis
				[stat->cyclesread & par->bufmsk];

			if (diff > stat->redmax) {
				stat->redmax = diff;
				stat->cycleofmax = stat->cyclesread;
			}
			if (++stat->reduce == oscope_reduction) {
				if (!smi)
					fprintf(fp, "%8d:%8lu:%8ld\n", index,
						stat->cycleofmax, stat->redmax);
				else
					fprintf(fp, "%8d:%8lu:%8ld%8ld\n",
						index, stat->cycleofmax,
						stat->redmax, diff_smi);

				stat->reduce = 0;
				stat->redmax = 0;
			}
			stat->cyclesread++;
		}
	}
}

static void rstat_print_stat(struct thread_param *par, int index, int verbose, int quiet)
{
	struct thread_stat *stat = par->stats;
	int fd = rstat_fd;

	if (!verbose) {
		if (quiet != 1) {
			char *fmt;
			if (use_nsecs)
				fmt = "T:%2d (%5d) P:%2d I:%ld C:%7lu "
				        "Min:%7ld Act:%8ld Avg:%8ld Max:%8ld";
			else
				fmt = "T:%2d (%5d) P:%2d I:%ld C:%7lu "
				        "Min:%7ld Act:%5ld Avg:%5ld Max:%8ld";

			dprintf(fd, fmt, index, stat->tid, par->prio,
				par->interval, stat->cycles, stat->min,
				stat->act, stat->cycles ?
				(long)(stat->avg/stat->cycles) : 0, stat->max);

			if (smi)
				dprintf(fd, " SMI:%8ld", stat->smi_count);

			dprintf(fd, "\n");
		}
	} else {
		while (stat->cycles != stat->cyclesread) {
			unsigned long diff_smi;
			long diff = stat->values
			    [stat->cyclesread & par->bufmsk];

			if (smi)
				diff_smi = stat->smis
				[stat->cyclesread & par->bufmsk];

			if (diff > stat->redmax) {
				stat->redmax = diff;
				stat->cycleofmax = stat->cyclesread;
			}
			if (++stat->reduce == oscope_reduction) {
				if (!smi)
					dprintf(fd, "%8d:%8lu:%8ld\n", index,
						stat->cycleofmax, stat->redmax);
				else
					dprintf(fd, "%8d:%8lu:%8ld%8ld\n",
						index, stat->cycleofmax,
						stat->redmax, diff_smi);

				stat->reduce = 0;
				stat->redmax = 0;
			}
			stat->cyclesread++;
		}
	}
}


/*
 * thread that creates a named fifo and hands out run stats when someone
 * reads from the fifo.
 */
static void *fifothread(void *param __attribute__ ((unused)))
{
	int ret;
	int fd;
	FILE *fp;
	int i;

	unlink(fifopath);
	ret = mkfifo(fifopath, 0666);
	if (ret) {
		fprintf(stderr, "Error creating fifo %s: %s\n", fifopath, strerror(errno));
		return NULL;
	}
	while (!shutdown) {
		fd = open(fifopath, O_WRONLY|O_NONBLOCK);
		if (fd < 0) {
			usleep(500000);
			continue;
		}
		fp = fdopen(fd, "w");
		for (i=0; i < num_threads; i++)
			print_stat(fp, parameters[i], i, 0, 0);
		fclose(fp);
		usleep(250);
	}
	unlink(fifopath);
	return NULL;
}

static int trigger_init()
{
	int i;
	int size = trigger_list_size;
	struct thread_trigger *trig = NULL;
	for(i=0; i<size; i++) {
		trig = malloc(sizeof(struct thread_trigger));
		if (trig != NULL) {
			if  (head == NULL) {
				head = trig;
				tail = trig;
			} else {
				tail->next = trig;
				tail = trig;
			}
			trig->tnum = i;
			trig->next = NULL;
		} else {
			return -1;
		}
	}
	current = head;
	return 0;
}

static void trigger_print()
{
	struct thread_trigger *trig = head;
	char *fmt = "T:%2d Spike:%8ld: TS: %12ld\n";

	if (current == head)
		return;
	printf("\n");
	while (trig->next != current) {
		fprintf(stdout, fmt,  trig->tnum, trig->diff, trig->ts);
		trig = trig->next;
	}
		fprintf(stdout, fmt,  trig->tnum, trig->diff, trig->ts);
		printf("spikes = %d\n\n", spikes);
}

static void trigger_update(struct thread_param *par, int diff, int64_t ts)
{
	pthread_mutex_lock(&trigger_lock);
	if (current != NULL) {
		current->tnum = par->tnum;
		current->ts = ts;
		current->diff = diff;
		current = current->next;
	}
	spikes++;
	pthread_mutex_unlock(&trigger_lock);
}

/* Running status shared memory open */
static int rstat_shm_open(void)
{
	int fd;
	pid_t pid;

	pid = getpid();

	snprintf(shm_name, SHM_BUF_SIZE, "%s%d", "/cyclictest", pid);

	errno = 0;
	fd = shm_unlink(shm_name);

	if ((fd == -1) && (errno != ENOENT)) {
		fprintf(stderr, "ERROR: shm_unlink %s\n", strerror(errno));
		return fd;
	}

	errno = 9;
	fd = shm_open(shm_name, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if (fd == -1)
		fprintf(stderr, "ERROR: shm_open %s\n", strerror(errno));

	rstat_fd = fd;

	return fd;
}

static int rstat_ftruncate(int fd, off_t len)
{
	int err;

	errno = 0;
	err = ftruncate(fd, len);
	if (err)
		fprintf(stderr, "ftruncate error %s\n", strerror(errno));

	return err;
}

static void *rstat_mmap(int fd)
{
	void *mptr;

	errno = 0;
	mptr = mmap(0, _SC_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if (mptr == (void*)-1)
		fprintf(stderr, "ERROR: mmap, %s\n", strerror(errno));

	return mptr;
}

static int rstat_mlock(void *mptr)
{
	int err;

	errno = 0;
	err = mlock(mptr, _SC_PAGE_SIZE);
	if (err == -1)
		fprintf(stderr, "ERROR, mlock %s\n", strerror(errno));

	return err;
}

static void rstat_setup(void)
{
	int res;
	void *mptr = NULL;

	int sfd = rstat_shm_open();
	if (sfd < 0)
		goto rstat_err;

	res = rstat_ftruncate(sfd, _SC_PAGE_SIZE);
	if (res)
		goto rstat_err1;

	mptr = rstat_mmap(sfd);
	if (mptr == MAP_FAILED)
		goto rstat_err1;

	res = rstat_mlock(mptr);
	if (res)
		goto rstat_err2;

	return;

rstat_err2:
	munmap(mptr, _SC_PAGE_SIZE);
rstat_err1:
	close(sfd);
	shm_unlink(shm_name);
rstat_err:
	rstat_fd = -1;
	return;
}

static void write_stats(FILE *f, void *data __attribute__ ((unused)))
{
	struct thread_param **par = parameters;
	int i;
	struct thread_stat *s;

	fprintf(f, "  \"num_threads\": %d,\n", num_threads);
	fprintf(f, "  \"resolution_in_ns\": %u,\n", use_nsecs);
	fprintf(f, "  \"thread\": {\n");
	for (i = 0; i < num_threads; i++) {
		s = par[i]->stats;
		fprintf(f, "    \"%u\": {\n", i);
		if (s->hist) {
			fprintf(f, "      \"histogram\": {");
			hist_print_json(s->hist, f);
			fprintf(f, "      },\n");
		}
		fprintf(f, "      \"cycles\": %ld,\n", s->cycles);
		fprintf(f, "      \"min\": %ld,\n", s->min);
		fprintf(f, "      \"max\": %ld,\n", s->max);
		fprintf(f, "      \"avg\": %.2f,\n", s->avg/s->cycles);
		fprintf(f, "      \"cpu\": %d,\n", par[i]->cpu);
		fprintf(f, "      \"node\": %d\n", par[i]->node);
		fprintf(f, "    }%s\n", i == num_threads - 1 ? "" : ",");
	}
	fprintf(f, "  }\n");
}

static void set_main_thread_affinity(struct bitmask *cpumask)
{
	int res;

	errno = 0;
	res = numa_sched_setaffinity(getpid(), cpumask);
	if (res != 0)
		warn("Couldn't setaffinity in main thread: %s\n",
		     strerror(errno));
}

int main(int argc, char **argv)
{
	sigset_t sigset;
	int signum = SIGALRM;
	int mode;
	int cpu;
	int max_cpus = sysconf(_SC_NPROCESSORS_CONF);
	int online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	int i, ret = -1;
	int status;

	rt_init(argc, argv);
	process_options(argc, argv, max_cpus);

	if (check_privs())
		exit(EXIT_FAILURE);

	if (verbose) {
		printf("Max CPUs = %d\n", max_cpus);
		printf("Online CPUs = %d\n", online_cpus);
	}

	if (affinity_mask != NULL) {
		set_main_thread_affinity(affinity_mask);
		if (verbose)
			printf("Using %u cpus.\n",
				numa_bitmask_weight(affinity_mask));
	}

	if (trigger) {
		int retval;
		retval = trigger_init();
		if (retval != 0) {
			fprintf(stderr, "trigger_init() failed\n");
			exit(EXIT_FAILURE);
		}
	}

	/* lock all memory (prevent swapping) */
	if (lockall)
		if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
			perror("mlockall");
			goto out;
		}

	/* use the /dev/cpu_dma_latency trick if it's there */
	set_latency_target();

	if (tracelimit && trace_marker)
		enable_trace_mark();

	if (check_timer())
		warn("High resolution timers not available\n");

	if (check_clock_resolution) {
		int clock;
		uint64_t diff;
		int k;
		uint64_t min_non_zero_diff = UINT64_MAX;
		struct timespec now;
		struct timespec prev;
		uint64_t reported_resolution = UINT64_MAX;
		struct timespec res;
		struct timespec *time;
		int times;

		clock = clocksources[clocksel];

		if (clock_getres(clock, &res))
			warn("clock_getres failed");
		else
			reported_resolution = (NSEC_PER_SEC * res.tv_sec) + res.tv_nsec;


		/*
		 * Calculate how many calls to clock_gettime are needed.
		 * Then call it that many times.
		 * Goal is to collect timestamps for ~ 0.001 sec.
		 * This will reliably capture resolution <= 500 usec.
		 */
		times = 1000;
		clock_gettime(clock, &prev);
		for (k=0; k < times; k++)
			clock_gettime(clock, &now);

		diff = calcdiff_ns(now, prev);
		if (diff == 0) {
			/*
			 * No clock rollover occurred.
			 * Use the default value for times.
			 */
			times = -1;
		} else {
			int call_time;
			call_time = diff / times;         /* duration 1 call */
			times = NSEC_PER_SEC / call_time; /* calls per second */
			times /= 1000;                    /* calls per msec */
			if (times < 1000)
				times = 1000;
		}
		/* sanity check */
		if ((times <= 0) || (times > 100000))
			times = 100000;

		time = calloc(times, sizeof(*time));

		for (k=0; k < times; k++)
			clock_gettime(clock, &time[k]);

		info(ct_debug, "For %d consecutive calls to clock_gettime():\n", times);
		info(ct_debug, "time, delta time (nsec)\n");

		prev = time[0];
		for (k=1; k < times; k++) {

			diff = calcdiff_ns(time[k], prev);
			prev = time[k];

			if (diff && (diff < min_non_zero_diff))
				min_non_zero_diff = diff;

			info(ct_debug, "%ld.%06ld  %5llu\n",
					time[k].tv_sec, time[k].tv_nsec,
					(unsigned long long)diff);
		}

		free(time);


		if (verbose ||
		    (min_non_zero_diff && (min_non_zero_diff > reported_resolution))) {
			/*
			 * Measured clock resolution includes the time to call
			 * clock_gettime(), so it will be slightly larger than
			 * actual resolution.
			 */
			warn("reported clock resolution: %llu nsec\n",
			     (unsigned long long)reported_resolution);
			warn("measured clock resolution approximately: %llu nsec\n",
			     (unsigned long long)min_non_zero_diff);
		}

	}


	mode = use_nanosleep + use_system;

	sigemptyset(&sigset);
	sigaddset(&sigset, signum);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	signal(SIGINT, sighand);
	signal(SIGTERM, sighand);
	signal(SIGUSR1, sighand);
	signal(SIGUSR2, sighand);

	/* Set-up shm */
	rstat_setup();

	if (histogram && hset_init(&hset, num_threads, 1, histogram, histogram))
		fatal("failed to allocate histogram of size %d for %d threads\n",
		      histogram, num_threads);

	parameters = calloc(num_threads, sizeof(struct thread_param *));
	if (!parameters)
		goto out;
	statistics = calloc(num_threads, sizeof(struct thread_stat *));
	if (!statistics)
		goto outpar;

	for (i = 0; i < num_threads; i++) {
		pthread_attr_t attr;
		int node;
		struct thread_param *par;
		struct thread_stat *stat;

		status = pthread_attr_init(&attr);
		if (status != 0)
			fatal("error from pthread_attr_init for thread %d: %s\n", i, strerror(status));

		switch (setaffinity) {
		case AFFINITY_UNSPECIFIED: cpu = -1; break;
		case AFFINITY_SPECIFIED:
			cpu = cpu_for_thread_sp(i, max_cpus, affinity_mask);
			if (verbose)
				printf("Thread %d using cpu %d.\n", i, cpu);
			break;
		case AFFINITY_USEALL:
			cpu = cpu_for_thread_ua(i, max_cpus);
			break;
		default: cpu = -1;
		}

		node = -1;
		if (numa) {
			void *stack;
			void *currstk;
			size_t stksize;
			int node_cpu = cpu;

			if (node_cpu == -1)
				node_cpu = cpu_for_thread_ua(i, max_cpus);

			/* find the memory node associated with the cpu i */
			node = rt_numa_numa_node_of_cpu(node_cpu);

			/* get the stack size set for this thread */
			if (pthread_attr_getstack(&attr, &currstk, &stksize))
				fatal("failed to get stack size for thread %d\n", i);

			/* if the stack size is zero, set a default */
			if (stksize == 0)
				stksize = PTHREAD_STACK_MIN * 2;

			/*  allocate memory for a stack on appropriate node */
			stack = rt_numa_numa_alloc_onnode(stksize, node, node_cpu);

			/* touch the stack pages to pre-fault them in */
			memset(stack, 0, stksize);

			/* set the thread's stack */
			if (pthread_attr_setstack(&attr, stack, stksize))
				fatal("failed to set stack addr for thread %d to 0x%x\n",
				      i, stack+stksize);
		}

		/* allocate the thread's parameter block  */
		parameters[i] = par = threadalloc(sizeof(struct thread_param), node);
		if (par == NULL)
			fatal("error allocating thread_param struct for thread %d\n", i);
		memset(par, 0, sizeof(struct thread_param));

		/* allocate the thread's statistics block */
		statistics[i] = stat = threadalloc(sizeof(struct thread_stat), node);
		if (stat == NULL)
			fatal("error allocating thread status struct for thread %d\n", i);
		memset(stat, 0, sizeof(struct thread_stat));

		if (histogram)
			stat->hist = &hset.histos[i];

		if (verbose) {
			int bufsize = VALBUF_SIZE * sizeof(long);
			stat->values = threadalloc(bufsize, node);
			if (!stat->values)
				goto outall;
			memset(stat->values, 0, bufsize);
			par->bufmsk = VALBUF_SIZE - 1;
			if (smi) {
				stat->smis = threadalloc(bufsize, node);
				if (!stat->smis)
					goto outall;
				memset(stat->smis, 0, bufsize);
			}
		}

		par->prio = priority;
		if (priority && (policy == SCHED_FIFO || policy == SCHED_RR))
			par->policy = policy;
		else {
			par->policy = SCHED_OTHER;
			force_sched_other = 1;
		}
		if (priospread)
			priority--;
		par->clock = clocksources[clocksel];
		par->mode = mode;
		par->timermode = timermode;
		par->signal = signum;
		par->interval = interval;
		if (!histogram) /* same interval on CPUs */
			interval += distance;
		if (verbose)
			printf("Thread %d Interval: %d\n", i, interval);

		par->max_cycles = max_cycles;
		par->stats = stat;
		par->node = node;
		par->tnum = i;
		par->cpu = cpu;

		stat->min = 1000000;
		stat->max = 0;
		stat->avg = 0.0;
		stat->threadstarted = 1;
		stat->smi_count = 0;
		status = pthread_create(&stat->thread, &attr, timerthread, par);
		if (status)
			fatal("failed to create thread %d: %s\n", i, strerror(status));

	}
	/* Restrict the main pid to the affinity specified by the user */
	if (main_affinity_mask != NULL)
		set_main_thread_affinity(main_affinity_mask);

	if (use_fifo) {
		status = pthread_create(&fifo_threadid, NULL, fifothread, NULL);
		if (status)
			fatal("failed to create fifo thread: %s\n", strerror(status));
	}

	while (!shutdown) {
		char lavg[256];
		int fd, len, allstopped = 0;
		static char *policystr = NULL;
		static char *slash = NULL;
		static char *policystr2;

		if (!policystr)
			policystr = policyname(policy);

		if (!slash) {
			if (force_sched_other) {
				slash = "/";
				policystr2 = policyname(SCHED_OTHER);
			} else
				slash = policystr2 = "";
		}
		if (!verbose && !quiet) {
			fd = open("/proc/loadavg", O_RDONLY, 0666);
			len = read(fd, &lavg, 255);
			close(fd);
			lavg[len-1] = 0x0;
			printf("policy: %s%s%s: loadavg: %s          \n\n",
			       policystr, slash, policystr2, lavg);
		}

		for (i = 0; i < num_threads; i++) {

			print_stat(stdout, parameters[i], i, verbose, quiet);
			if (max_cycles && statistics[i]->cycles >= max_cycles)
				allstopped++;
		}

		usleep(10000);
		if (shutdown || allstopped)
			break;
		if (!verbose && !quiet)
			printf("\033[%dA", num_threads + 2);

		if (refresh_on_max) {
			pthread_mutex_lock(&refresh_on_max_lock);
			if (!shutdown)
				pthread_cond_wait(&refresh_on_max_cond,
						&refresh_on_max_lock);
			pthread_mutex_unlock(&refresh_on_max_lock);
		}
	}
	ret = EXIT_SUCCESS;

 outall:
	shutdown = 1;
	usleep(50000);

	if (!verbose && !quiet && refresh_on_max)
		printf("\033[%dB", num_threads + 2);

	if (strlen(jsonfile) != 0)
		rt_write_json(jsonfile, ret, write_stats, NULL);

	if (quiet)
		quiet = 2;
	for (i = 0; i < num_threads; i++) {
		if (statistics[i]->threadstarted > 0)
			pthread_kill(statistics[i]->thread, SIGTERM);
		if (statistics[i]->threadstarted) {
			pthread_join(statistics[i]->thread, NULL);
			if (quiet && !histogram)
				print_stat(stdout, parameters[i], i, 0, 0);
		}
		if (statistics[i]->values)
			threadfree(statistics[i]->values, VALBUF_SIZE*sizeof(long), parameters[i]->node);
	}

	if (trigger)
		trigger_print();

	if (histogram)
		print_hist(parameters, num_threads);

	if (tracelimit) {
		print_tids(parameters, num_threads);
		if (break_thread_id) {
			printf("# Break thread: %d\n", break_thread_id);
			printf("# Break value: %llu\n", (unsigned long long)break_thread_value);
		}
	}


	for (i=0; i < num_threads; i++) {
		if (!statistics[i])
			continue;
		threadfree(statistics[i], sizeof(struct thread_stat), parameters[i]->node);
	}

 outpar:
	for (i = 0; i < num_threads; i++) {
		if (!parameters[i])
			continue;
		threadfree(parameters[i], sizeof(struct thread_param), parameters[i]->node);
	}
 out:
	/* close any tracer file descriptors */
	disable_trace_mark();

	/* unlock everything */
	if (lockall)
		munlockall();

	/* close the latency_target_fd if it's open */
	if (latency_target_fd >= 0)
		close(latency_target_fd);

	if (affinity_mask)
		rt_bitmask_free(affinity_mask);

	/* Remove running status shared memory file if it exists */
	if (rstat_fd >= 0)
		shm_unlink(shm_name);

	hset_destroy(&hset);
	exit(ret);
}
