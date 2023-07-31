#ifndef __RT_GET_CPU_H
#define __RT_GET_CPU_H
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sched.h>
#include <dlfcn.h>
#ifdef __NR_getcpu
static inline int get_cpu_setup(void) { return 0; }
static inline int get_cpu(void)
{
        int c,s;
	/* Show the source of get_cpu */
#ifdef DEBUG
	fprintf(stderr, "__NR_getcpu\n");
#endif
        s = syscall(__NR_getcpu, &c, NULL, NULL);
        return (s == -1) ? s : c;
}
#elif defined(__GLIBC__) && defined(__GLIBC_MINOR__)	\
	&& __GLIBC__>=2 && __GLIBC_MINOR__>=6
#include <utmpx.h>
static inline int get_cpu_setup(void) { return 0; }
static inline int get_cpu(void) { return sched_getcpu(); }
#else
extern int get_cpu_setup(void);
extern int (*get_cpu)(void);
extern int (*get_cpu_vdsop)(unsigned int *, unsigned int *, void *);

static inline int getcpu_vdso(void)
{
	unsigned int c,s;
	/* Show the source of get_cpu */
#ifdef DEBUG
	fprintf(stderr, "getcpu_vdso\n");
#endif
	s = get_cpu_vdsop(&c, NULL, NULL);
        return (s == -1) ? s : c;
}

#endif

#endif	/* __RT_GET_CPU_H */

