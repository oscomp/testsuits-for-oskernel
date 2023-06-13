/*
  sigtest - simple little program to verify signal behavior
  
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

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <memory.h>
#include <errno.h>
#include <pthread.h>

#define TIMER_SIGNAL (SIGRTMIN+1)
#define SUCCESS 0
#define FAILURE -1

int
setup_timer(timer_t *timer)
{
	int status;
	struct sigevent sigev;

	memset(&sigev, 0, sizeof(sigev));
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = TIMER_SIGNAL;
	status = timer_create(CLOCK_MONOTONIC, &sigev, timer);
	if (status) {
		fprintf(stderr,"error from timer_create: %s\n", strerror(errno));
		return FAILURE;
	}
	return SUCCESS;
}
	
int
start_timer(timer_t t, int sec, int nsec)
{
	int status;
	struct itimerspec it;

	// set up as a one-shot
	memset(&it, 0, sizeof(it));
	it.it_value.tv_sec = sec;
	it.it_value.tv_nsec = nsec;
	status = timer_settime(t, 0, &it, NULL);
	if (status)
		fprintf(stderr,"starting timer: %s\n", strerror(errno));
	return status;
}

int
wait_for_signal(void)
{
	int signo;
	sigset_t sigset;

	if (sigemptyset(&sigset)) {
		fprintf(stderr,"creating empty signal wait set: %s\n", strerror(errno));
		return -1;
	}
	if (sigaddset(&sigset, SIGINT)) {
		fprintf(stderr,"adding SIGINT to signal set: %s\n", strerror(errno));
		return -1;
	}
	if (sigaddset(&sigset, TIMER_SIGNAL)) {
		fprintf(stderr,"adding TIMER_SIGNAL to signal set: %s\n", strerror(errno));
		return -1;
	}
	
	if (sigwait(&sigset, &signo)) {
		fprintf(stderr,"waiting for signal: %s\n", strerror(errno));
		return -1;
	}
	return signo;
}

int
block_signals(void)
{
	int status;
	sigset_t sigset;

	// mask off all signals
	status = sigfillset(&sigset);
	if (status) {
		fprintf(stderr,"setting up full signal set %d\n", status);
		return FAILURE;
	}
	status = pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	if (status) {
		fprintf(stderr,"setting signal mask: %d\n", status);
		return FAILURE;
	}
	return SUCCESS;
}

int
main(int argc, char **argv)
{
	int status;
	timer_t timer;
	unsigned long count = 0;
	int stop_test = 0;

	block_signals();
	setup_timer(&timer);
	printf("Press Ctrl-C to stop\n");
	while (stop_test == 0) {
		if (start_timer(timer, 0, 500000000)) {
			stop_test = 1;
			continue;
		}
		status = wait_for_signal();
		if (status == SIGINT) {
			fputs("\033[1B", stdout);
			stop_test = 1;
		}
		else if (status == TIMER_SIGNAL) {
			printf("count: %lu\n", ++count);
			fputs("\033[1A", stdout);
		}
		else {
			fprintf(stderr, "WTF?\n");
			return -1;
		}
	}
	return 0;
}	
