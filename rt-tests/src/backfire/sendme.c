/*
 * sendme.c
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
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "rt-utils.h"
#include "rt-get_cpu.h"

#include <utmpx.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#define USEC_PER_SEC		1000000
#define NSEC_PER_SEC		1000000000

#define SIGTEST SIGHUP

enum {
	AFFINITY_UNSPECIFIED,
	AFFINITY_SPECIFIED,
	AFFINITY_USECURRENT
};
static int setaffinity = AFFINITY_UNSPECIFIED;

static int affinity;
static int tracelimit;
static int priority;
static int shutdown;
static int max_cycles;
static volatile struct timeval after;
static int interval = 1000;

static int kernvar(int mode, const char *name, char *value, size_t sizeofvalue)
{
	char filename[128];
	char *fileprefix = get_debugfileprefix();
	int retval = 1;
	int path;
	size_t len_prefix = strlen(fileprefix), len_name = strlen(name);

	if (len_prefix + len_name + 1 > sizeof(filename)) {
		errno = ENOMEM;
		return 1;
	}

	memcpy(filename, fileprefix, len_prefix);
	memcpy(filename + len_prefix, name, len_name + 1);

	path = open(filename, mode);
	if (path >= 0) {
		if (mode == O_RDONLY) {
			int got;
			if ((got = read(path, value, sizeofvalue)) > 0) {
				retval = 0;
				value[got-1] = '\0';
			}
		} else if (mode == O_WRONLY) {
			if (write(path, value, sizeofvalue) == sizeofvalue)
				retval = 0;
		}
		close(path);
	}
	return retval;
}

void signalhandler(int signo)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	after = tv;
	if (signo == SIGINT || signo == SIGTERM)
		shutdown = 1;
}

void stop_tracing(void)
{
	kernvar(O_WRONLY, "tracing_enabled", "0", 1);
}

static void display_help(void)
{
	printf("sendme V %1.2f\n", VERSION);
	puts("Usage: sendme <options>");
	puts("Function: send a signal from driver to userspace");
	puts(
	"Options:\n"
	"-a [NUM] --affinity        pin to current processor\n"
	"                           with NUM pin to processor NUM\n"
	"-b USEC  --breaktrace=USEC send break trace command when latency > USEC\n"
	"-i INTV  --interval=INTV   base interval of thread in us default=1000\n"
	"-l LOOPS --loops=LOOPS     number of loops: default=0(endless)\n"
	"-p PRIO  --prio=PRIO       priority\n");
	exit(1);
}

static void process_options (int argc, char *argv[])
{
	int error = 0;
	int max_cpus = sysconf(_SC_NPROCESSORS_CONF);

	for (;;) {
		int option_index = 0;
		/** Options for getopt */
		static struct option long_options[] = {
			{"affinity", optional_argument, NULL, 'a'},
			{"breaktrace", required_argument, NULL, 'b'},
			{"interval", required_argument, NULL, 'i'},
			{"loops", required_argument, NULL, 'l'},
			{"priority", required_argument, NULL, 'p'},
			{"help", no_argument, NULL, '?'},
			{NULL, 0, NULL, 0}
		};
		int c = getopt_long (argc, argv, "a::b:i:l:p:",
			long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
			if (optarg != NULL) {
				affinity = atoi(optarg);
				setaffinity = AFFINITY_SPECIFIED;
			} else if (optind < argc && atoi(argv[optind])) {
				affinity = atoi(argv[optind]);
				setaffinity = AFFINITY_SPECIFIED;
			} else
				setaffinity = AFFINITY_USECURRENT;
			break;
		case 'b': tracelimit = atoi(optarg); break;
		case 'i': interval = atoi(optarg); break;
		case 'l': max_cycles = atoi(optarg); break;
		case 'p': priority = atoi(optarg); break;
		case '?': error = 1; break;
		}
	}

	if (setaffinity == AFFINITY_SPECIFIED) {
		if (affinity < 0)
			error = 1;
		if (affinity >= max_cpus) {
			fprintf(stderr, "ERROR: CPU #%d not found, only %d CPUs available\n",
			    affinity, max_cpus);
			error = 1;
		}
	}

	if (priority < 0 || priority > 99)
		error = 1;

	if (error)
		display_help ();
}

int main(int argc, char *argv[])
{
	int path;
	cpu_set_t mask;
	int policy = SCHED_FIFO;
	struct sched_param schedp;
	struct flock fl;
	int retval = 0;

	process_options(argc, argv);

	if (check_privs())
		return 1;

	if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
		perror("mlockall");
		return 1;
	}

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = priority;
	sched_setscheduler(0, policy, &schedp);

	if (setaffinity != AFFINITY_UNSPECIFIED) {
		CPU_ZERO(&mask);
		if (setaffinity == AFFINITY_USECURRENT) {
			get_cpu_setup();
			affinity = get_cpu();
		}
		CPU_SET(affinity, &mask);
		if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
			fprintf(stderr,	"WARNING: Could not set CPU affinity "
				"to CPU #%d\n", affinity);
	}

	path = open("/dev/backfire", O_RDWR);
	if (path < 0) {
		fprintf(stderr, "ERROR: Could not access backfire device, "
				"try 'modprobe backfire'\n"
				"If the module backfire can't be loaded, "
				"it may need to be built first.\n"
				"Execute 'cd src/backfire; make' in the "
				"rt-tests directory (requires rt-tests\n"
				"sources and kernel-devel package).\n");
		return 1;
	}
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	if (fcntl(path, F_SETLK, &fl) == -1) {
		fprintf(stderr, "ERRROR: backfire device locked\n");
		retval = 1;
	} else {
		char sigtest[8];
		char timestamp[32];
		struct timeval before, sendtime, diff;
		unsigned int diffno = 0;
		unsigned int mindiff1 = UINT_MAX, maxdiff1 = 0;
		unsigned int mindiff2 = UINT_MAX, maxdiff2 = 0;
		double sumdiff1 = 0.0, sumdiff2 = 0.0;

		if (tracelimit)
			kernvar(O_WRONLY, "tracing_enabled", "1", 1);

		sprintf(sigtest, "%d", SIGTEST);
		signal(SIGTEST, signalhandler);
		signal(SIGINT, signalhandler);
		signal(SIGTERM, signalhandler);

		while (1) {
			struct timespec ts;

			ts.tv_sec = interval / USEC_PER_SEC;
			ts.tv_nsec = (interval % USEC_PER_SEC) * 1000;

			gettimeofday(&before, NULL);
			write(path, sigtest, strlen(sigtest));
			while (after.tv_sec == 0);
			read(path, timestamp, sizeof(timestamp));
			if (sscanf(timestamp, "%lu,%lu\n", &sendtime.tv_sec,
			    &sendtime.tv_usec) != 2)
				break;
			diffno++;
			if(max_cycles && diffno >= max_cycles)
				shutdown = 1;

			printf("Samples: %8d\n", diffno);
			timersub(&sendtime, &before, &diff);
			if (diff.tv_usec < mindiff1)
				mindiff1 = diff.tv_usec;
			if (diff.tv_usec > maxdiff1)
				maxdiff1 = diff.tv_usec;
			sumdiff1 += (double) diff.tv_usec;
			printf("To:   Min %4d, Cur %4d, Avg %4d, Max %4d\n",
				mindiff1, (int) diff.tv_usec,
				(int) ((sumdiff1 / diffno) + 0.5),
				maxdiff1);

			timersub(&after, &sendtime, &diff);
			if (diff.tv_usec < mindiff2)
				mindiff2 = diff.tv_usec;
			if (diff.tv_usec > maxdiff2)
				maxdiff2 = diff.tv_usec;
			sumdiff2 += (double) diff.tv_usec;
			printf("From: Min %4d, Cur %4d, Avg %4d, Max %4d\n",
				mindiff2, (int) diff.tv_usec,
				(int) ((sumdiff2 / diffno) + 0.5),
				maxdiff2);
			after.tv_sec = 0;
			if ((tracelimit && diff.tv_usec > tracelimit) ||
			    shutdown) {
				if (tracelimit)
					stop_tracing();
				break;
			}
			nanosleep(&ts, NULL);
			printf("\033[3A");
		}
	}

	close(path);
	return retval;
}
