#ifndef BIONIC_H
#define BIONIC_H

#ifdef PTHREAD_BIONIC
#warning Program is being compiled with PTHREAD_BIONIC, some options may behave erratically.

/*
 * We do not have pthread_barrier_t available, but since we are not
 * going to use them for anything useful, just typedef them to int
 */
typedef int pthread_barrier_t;
typedef int pthread_barrierattr_t;

#ifndef PTHREAD_BARRIER_SERIAL_THREAD
#define PTHREAD_BARRIER_SERIAL_THREAD 0
#endif

static inline int pthread_barrier_wait(pthread_barrier_t *barrier)
{
        return PTHREAD_BARRIER_SERIAL_THREAD;
}

static inline int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
        return 0;
}
static inline int pthread_barrier_init(pthread_barrier_t * barrier,
                                       const pthread_barrierattr_t * attr,
                                       unsigned count)
{
        return 0;
}

static inline int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize,
                                         const cpu_set_t *cpuset)
{
    return sched_setaffinity(0, cpusetsize, cpuset);
}

#endif	/* PTHREAD_BIONIC */

#endif /* BIONIC_H */
