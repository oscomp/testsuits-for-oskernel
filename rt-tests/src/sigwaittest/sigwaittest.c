/*
 * sigwaittest.c
 *
 * Copyright (C) 2009 Carsten Emde <C.Emde@osadl.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <utmpx.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/unistd.h>
#include <utmpx.h>
#include "rt-utils.h"
#include "rt-get_cpu.h"

#include <pthread.h>

#define gettid() syscall(__NR_gettid)

#define USEC_PER_SEC 1000000

enum {
	AFFINITY_UNSPECIFIED,
	AFFINITY_SPECIFIED,
	AFFINITY_USEALL
};

struct params {
	int num;
	int num_threads;
	int cpu;
	int priority;
	int affinity;
	int sender;
	int samples;
	int max_cycles;
	int tracelimit;
	int tid;
	pid_t pid;
	int shutdown;
	int stopped;
	struct timespec delay;
	unsigned int mindiff, maxdiff;
	double sumdiff;
	struct timeval unblocked, received, diff;
	pthread_t threadid;
	struct params *neighbor;
	char error[MAX_PATH * 2];
};

static int mustfork;
static int wasforked;
static int wasforked_sender = -1;
static int wasforked_threadno = -1;
static int tracelimit;

void *semathread(void *param)
{
	int mustgetcpu = 0;
	struct params *par = param;
	cpu_set_t mask;
	int policy = SCHED_FIFO;
	struct sched_param schedp;

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = par->priority;
	sched_setscheduler(0, policy, &schedp);

	if (par->cpu != -1) {
		CPU_ZERO(&mask);
		CPU_SET(par->cpu, &mask);
		if(sched_setaffinity(0, sizeof(mask), &mask) == -1)
			fprintf(stderr,	"WARNING: Could not set CPU affinity "
				"to CPU #%d\n", par->cpu);
	} else {
        	int max_cpus = sysconf(_SC_NPROCESSORS_CONF);

        	if (max_cpus > 1)
		        mustgetcpu = 1;
	        else
			par->cpu = 0;
	}

	if (!wasforked)
		par->tid = gettid();

	while (!par->shutdown) {
		int sig;
		int first = 1;
		sigset_t sigset;
		struct params *neighbor = NULL;

		if (par->sender) {
			if (wasforked)
				neighbor = par - par->num_threads;
			else
				neighbor = par->neighbor;
			if (first) {
				sigemptyset(&sigset);
				sigaddset(&sigset, SIGUSR1);
				pthread_sigmask(SIG_SETMASK, &sigset, NULL);
				first = 0;
			}

			/* Sending signal: Start of latency measurement ... */
			gettimeofday(&par->unblocked, NULL);
			if (wasforked)
				kill(neighbor->pid, SIGUSR2);
			else
				pthread_kill(neighbor->threadid, SIGUSR2);
			par->samples++;
			if(par->max_cycles && par->samples >= par->max_cycles)
				par->shutdown = 1;

			if (mustgetcpu) {
				par->cpu = get_cpu();
			}
			sigwait(&sigset, &sig);
		} else {
			/* Receiver */
			if (wasforked)
				neighbor = par + par->num_threads;
			else
				neighbor = par->neighbor;
			if (first) {
				sigemptyset(&sigset);
				sigaddset(&sigset, SIGUSR2);
				pthread_sigmask(SIG_SETMASK, &sigset, NULL);
				first = 0;
			}
			sigwait(&sigset, &sig);

			/* ... Signal received: End of latency measurement */
			gettimeofday(&par->received, NULL);
			par->samples++;
			if (par->max_cycles && par->samples >= par->max_cycles)
				par->shutdown = 1;

			if (mustgetcpu) {
				par->cpu = get_cpu();
		        }
			/*
			 * Latency is the time spent between sending and
			 * receiving the signal.
			 */
			timersub(&par->received, &neighbor->unblocked,
			    &par->diff);

			if (par->diff.tv_usec < par->mindiff)
				par->mindiff = par->diff.tv_usec;
			if (par->diff.tv_usec > par->maxdiff)
				par->maxdiff = par->diff.tv_usec;
			par->sumdiff += (double) par->diff.tv_usec;
			if (par->tracelimit && par->maxdiff > par->tracelimit) {
				char tracing_enabled_file[MAX_PATH];

				strcpy(tracing_enabled_file, get_debugfileprefix());
				strcat(tracing_enabled_file, "tracing_enabled");
				int tracing_enabled =
				    open(tracing_enabled_file, O_WRONLY);
				if (tracing_enabled >= 0) {
					write(tracing_enabled, "0", 1);
					close(tracing_enabled);
				} else
					snprintf(par->error, sizeof(par->error),
					    "Could not access %s\n",
					    tracing_enabled_file);
				par->shutdown = 1;
				neighbor->shutdown = 1;
			}

			nanosleep(&par->delay, NULL);

			if (wasforked)
				kill(neighbor->pid, SIGUSR1);
			else
				pthread_kill(neighbor->threadid, SIGUSR1);
		}
	}
	par->stopped = 1;
	return NULL;
}


static void display_help(void)
{
	printf("sigwaittest V %1.2f\n", VERSION);
	puts("Usage: sigwaittest <options>");
	puts("Function: test sigwait() latency");
	puts(
	"Options:\n"
	"-a [NUM] --affinity        run thread #N on processor #N, if possible\n"
	"                           with NUM pin all threads to the processor NUM\n"
	"-b USEC  --breaktrace=USEC send break trace command when latency > USEC\n"
	"-d DIST  --distance=DIST   distance of thread intervals in us default=500\n"
	"-f       --fork            fork new processes instead of creating threads\n"
	"-i INTV  --interval=INTV   base interval of thread in us default=1000\n"
	"-l LOOPS --loops=LOOPS     number of loops: default=0(endless)\n"
	"-p PRIO  --prio=PRIO       priority\n"
	"-t       --threads         one thread per available processor\n"
	"-t [NUM] --threads=NUM     number of threads:\n"
	"                           without NUM, threads = max_cpus\n"
	"                           without -t default = 1\n");
	exit(1);
}


static int setaffinity = AFFINITY_UNSPECIFIED;
static int affinity;
static int priority;
static int num_threads = 1;
static int max_cycles;
static int interval = 1000;
static int distance = 500;

static void process_options (int argc, char *argv[])
{
	int error = 0;
	int max_cpus = sysconf(_SC_NPROCESSORS_CONF);
	int thistracelimit = 0;

	for (;;) {
		int option_index = 0;
		/** Options for getopt */
		static struct option long_options[] = {
			{"affinity", optional_argument, NULL, 'a'},
			{"breaktrace", required_argument, NULL, 'b'},
			{"distance", required_argument, NULL, 'd'},
			{"fork", optional_argument, NULL, 'f'},
			{"interval", required_argument, NULL, 'i'},
			{"loops", required_argument, NULL, 'l'},
			{"priority", required_argument, NULL, 'p'},
			{"threads", optional_argument, NULL, 't'},
			{"help", no_argument, NULL, '?'},
			{NULL, 0, NULL, 0}
		};
		int c = getopt_long (argc, argv, "a::b:d:f::i:l:p:t::",
			long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
			if (optarg != NULL) {
				affinity = atoi(optarg);
				setaffinity = AFFINITY_SPECIFIED;
			} else if (optind<argc && atoi(argv[optind])) {
				affinity = atoi(argv[optind]);
				setaffinity = AFFINITY_SPECIFIED;
			} else {
				setaffinity = AFFINITY_USEALL;
			}
			break;
		case 'b': thistracelimit = atoi(optarg); break;
		case 'd': distance = atoi(optarg); break;
		case 'f':
			if (optarg != NULL) {
				wasforked = 1;
				if (optarg[0] == 's')
					wasforked_sender = 1;
				else if (optarg[0] == 'r')
					wasforked_sender = 0;
				wasforked_threadno = atoi(optarg+1);
			} else
				mustfork = 1;
			break;
		case 'i': interval = atoi(optarg); break;
		case 'l': max_cycles = atoi(optarg); break;
		case 'p': priority = atoi(optarg); break;
		case 't':
			if (optarg != NULL)
				num_threads = atoi(optarg);
			else if (optind<argc && atoi(argv[optind]))
				num_threads = atoi(argv[optind]);
			else
				num_threads = max_cpus;
			break;
		case '?': error = 1; break;
		}
	}

	if (!wasforked) {
		if (setaffinity == AFFINITY_SPECIFIED) {
			if (affinity < 0)
				error = 1;
			if (affinity >= max_cpus) {
				fprintf(stderr, "ERROR: CPU #%d not found, "
				    "only %d CPUs available\n",
				    affinity, max_cpus);
				error = 1;
			}
		}

		if (num_threads < 1 || num_threads > 255)
			error = 1;

		if (priority < 0 || priority > 99)
			error = 1;

		tracelimit = thistracelimit;
	}
	if (error)
		display_help ();
}


static int volatile mustshutdown;

static void sighand(int sig)
{
	mustshutdown = 1;
}


int main(int argc, char *argv[])
{
	int i, totalsize = 0;
	int max_cpus = sysconf(_SC_NPROCESSORS_CONF);
	int oldsamples = 1;
	struct params *receiver = NULL;
	struct params *sender = NULL;
	sigset_t sigset;
	void *param = NULL;
	char f_opt[8];
	struct timespec launchdelay, maindelay;

	process_options(argc, argv);

	if (check_privs())
		return 1;

	if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
		perror("mlockall");
		return 1;
	}

	get_cpu_setup();	/* init get_cpu() */

	if (mustfork) {
		int shmem;

		/*
		 * In fork mode (-f), the shared memory contains two
		 * subsequent arrays, receiver[num_threads] and
		 * sender[num_threads].
		 */
		totalsize = num_threads * sizeof(struct params) * 2;

		shm_unlink("/sigwaittest");
  		shmem = shm_open("/sigwaittest", O_CREAT|O_EXCL|O_RDWR,
		    S_IRUSR|S_IWUSR);
		if (shmem < 0) {
			fprintf(stderr, "Could not create shared memory\n");
			return 1;
		}
		ftruncate(shmem, totalsize);
		param = mmap(0, totalsize, PROT_READ|PROT_WRITE, MAP_SHARED,
		    shmem, 0);
		if (param == MAP_FAILED) {
			fprintf(stderr, "Could not map shared memory\n");
			close(shmem);
			return 1;
		}

		receiver = (struct params *) param;
		sender = receiver + num_threads;
	} else if (wasforked) {
		struct stat buf;
		int shmem, totalsize, expect_totalsize;

		if (wasforked_threadno == -1 || wasforked_sender == -1) {
			fprintf(stderr, "Invalid fork option\n");
			return 1;
		}
		shmem = shm_open("/sigwaittest", O_RDWR, S_IRUSR|S_IWUSR);
		if (fstat(shmem, &buf)) {
			fprintf(stderr,
			    "Could not determine shared memory size\n");
			close(shmem);
			return 1;
		}
		totalsize = buf.st_size;
		param = mmap(0, totalsize, PROT_READ|PROT_WRITE, MAP_SHARED,
		    shmem, 0);
		close(shmem);
		if (param == MAP_FAILED) {
			fprintf(stderr, "Could not map shared memory\n");
			return 1;
		}

		receiver = (struct params *) param;
		expect_totalsize = receiver->num_threads *
		    sizeof(struct params) * 2;
		if (totalsize != expect_totalsize) {
			fprintf(stderr, "Memory size problem (expected %d, "
			    "found %d\n", expect_totalsize, totalsize);
			munmap(param, totalsize);
			return 1;
		}
		sender = receiver + receiver->num_threads;
		if (wasforked_sender)
			semathread(sender + wasforked_threadno);
		else
			semathread(receiver + wasforked_threadno);
		munmap(param, totalsize);
		return 0;
	}

	signal(SIGINT, sighand);
	signal(SIGTERM, sighand);
	sigemptyset(&sigset);
	pthread_sigmask(SIG_SETMASK, &sigset, NULL);

	if (!mustfork && !wasforked) {
		receiver = calloc(num_threads, sizeof(struct params));
		sender = calloc(num_threads, sizeof(struct params));
		if (receiver == NULL || sender == NULL)
			goto nomem;
	}

	launchdelay.tv_sec = 0;
	launchdelay.tv_nsec = 10000000; /* 10 ms */

	maindelay.tv_sec = 0;
	maindelay.tv_nsec = 50000000; /* 50 ms */

	for (i = 0; i < num_threads; i++) {
		receiver[i].mindiff = UINT_MAX;
		receiver[i].maxdiff = 0;
		receiver[i].sumdiff = 0.0;

		receiver[i].num = i;
		receiver[i].cpu = i;
		receiver[i].priority = priority;
		receiver[i].tracelimit = tracelimit;
		if (priority > 0)
			priority--;
		switch (setaffinity) {
		case AFFINITY_UNSPECIFIED: receiver[i].cpu = -1; break;
		case AFFINITY_SPECIFIED: receiver[i].cpu = affinity; break;
		case AFFINITY_USEALL: receiver[i].cpu = i % max_cpus; break;
		}
		receiver[i].delay.tv_sec = interval / USEC_PER_SEC;
		receiver[i].delay.tv_nsec = (interval % USEC_PER_SEC) * 1000;
		interval += distance;
		receiver[i].max_cycles = max_cycles;
		receiver[i].sender = 0;
		receiver[i].neighbor = &sender[i];
		if (mustfork) {
			pid_t pid = fork();
			if (pid == -1) {
				fprintf(stderr, "Could not fork\n");
				return 1;
			} else if (pid == 0) {
				char *args[3];

				receiver[i].num_threads = num_threads;
				receiver[i].pid = getpid();
				sprintf(f_opt, "-fr%d", i);
				args[0] = argv[0];
				args[1] = f_opt;
				args[2] = NULL;
				execvp(args[0], args);
				fprintf(stderr,
				    "Could not execute receiver child process "
				    "#%d\n", i);
			}
		} else
			pthread_create(&receiver[i].threadid, NULL,
			    semathread, &receiver[i]);

		nanosleep(&launchdelay, NULL);

		memcpy(&sender[i], &receiver[i], sizeof(receiver[0]));
		sender[i].sender = 1;
		sender[i].neighbor = &receiver[i];
		if (mustfork) {
			pid_t pid = fork();
			if (pid == -1) {
				fprintf(stderr, "Could not fork\n");
				return 1;
			} else if (pid == 0) {
				char *args[3];

				sender[i].num_threads = num_threads;
				sender[i].pid = getpid();
				sprintf(f_opt, "-fs%d", i);
				args[0] = argv[0];
				args[1] = f_opt;
				args[2] = NULL;
				execvp(args[0], args);
				fprintf(stderr,
				    "Could not execute sender child process "
				    "#%d\n", i);
			}
		} else
			pthread_create(&sender[i].threadid, NULL, semathread,
			    &sender[i]);
	}

	while (!mustshutdown) {
		int printed;
		int errorlines = 0;

		for (i = 0; i < num_threads; i++)
			mustshutdown |= receiver[i].shutdown |
			    sender[i].shutdown;

		if (receiver[0].samples > oldsamples || mustshutdown) {
			for (i = 0; i < num_threads; i++) {
				int receiver_pid, sender_pid;
				if (mustfork) {
					receiver_pid = receiver[i].pid;
					sender_pid = sender[i].pid;
				} else {
					receiver_pid = receiver[i].tid;
					sender_pid = sender[i].tid;
				}
				printf("#%1d: ID%d, P%d, CPU%d, I%ld; #%1d: "
				    "ID%d, P%d, CPU%d, Cycles %d\n",
				    i*2, receiver_pid, receiver[i].priority,
				    receiver[i].cpu, receiver[i].delay.tv_nsec /
				    1000, i*2+1, sender_pid, sender[i].priority,
				    sender[i].cpu, sender[i].samples);
			}
			for (i = 0; i < num_threads; i++) {
				if (receiver[i].mindiff == -1)
					printf("#%d -> #%d (not yet ready)\n",
					    i*2+1, i*2);
				else
					printf("#%d -> #%d, Min %4d, Cur %4d, "
					    "Avg %4d, Max %4d\n",
					    i*2+1, i*2,	receiver[i].mindiff,
					    (int) receiver[i].diff.tv_usec,
					    (int) ((receiver[i].sumdiff /
					    receiver[i].samples) + 0.5),
					    receiver[i].maxdiff);
				if (receiver[i].error[0] != '\0') {
					printf("%s", receiver[i].error);
					receiver[i].error[0] = '\0';
					errorlines++;
				}
				if (sender[i].error[0] != '\0') {
					printf("%s", sender[i].error);
					sender[i].error[0] = '\0';
					errorlines++;
				}
			}
			printed = 1;
		} else
			printed = 0;

		sigemptyset(&sigset);
		sigaddset(&sigset, SIGTERM);
		sigaddset(&sigset, SIGINT);
		pthread_sigmask(SIG_SETMASK, &sigset, NULL);

		nanosleep(&maindelay, NULL);

		sigemptyset(&sigset);
		pthread_sigmask(SIG_SETMASK, &sigset, NULL);

		if (printed && !mustshutdown)
			printf("\033[%dA", num_threads*2 + errorlines);
	}

	for (i = 0; i < num_threads; i++) {
		receiver[i].shutdown = 1;
		sender[i].shutdown = 1;
	}
	nanosleep(&receiver[0].delay, NULL);

	for (i = 0; i < num_threads; i++) {
		if (!receiver[i].stopped) {
			if (mustfork)
				kill(receiver[i].pid, SIGTERM);
			else
				pthread_kill(receiver[i].threadid, SIGTERM);
		}
		if (!sender[i].stopped) {
			if (mustfork)
				kill(sender[i].pid, SIGTERM);
			else
				pthread_kill(sender[i].threadid, SIGTERM);
		}
	}

 	nomem:
	if (mustfork) {
		munmap(param, totalsize);
		shm_unlink("/sigwaittest");
	}

	return 0;
}
