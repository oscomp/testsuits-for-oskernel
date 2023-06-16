#ifndef __PIP_STRESS_H
#define __PIP_STRESS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sched.h>
#include <rt-utils.h>
#include "error.h"

void low(pid_t pid);	/* low priority process */
void medium(void);	/* medium priority process */
void high(pid_t pid);	/* high priority process */
void init_state(void);

void *mmap_page(void);
long process_shared_mutex_available(void);
void Pthread_mutexattr_init(pthread_mutexattr_t *attr);
void Pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
void Pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol);
void Pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
void Pthread_mutex_lock(pthread_mutex_t *mutex);
void Pthread_mutex_unlock(pthread_mutex_t *mutex);

void init_shared_pthread_mutex(pthread_mutex_t *mutex, int protocol, int policy);
int set_rt_prio(pid_t pid, int prio, int policy);
int get_rt_prio(pid_t pid);

#define PROTRW PROT_READ|PROT_WRITE
#define MMAP_FLAGS MAP_SHARED|MAP_ANONYMOUS

#endif	/* __PIP_STRESS_H */
