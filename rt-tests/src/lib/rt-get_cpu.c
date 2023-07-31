/*
 * Copyright (C) 2009 John Kacur <jkacur@redhat.com>
 */
#include "rt-get_cpu.h"
#ifdef __NR_getcpu
#elif defined(__GLIBC__) && defined(__GLIBC_MINOR__)	\
	&& __GLIBC__>=2 && __GLIBC_MINOR__>=6
#else
int (*get_cpu)(void);
int (*get_cpu_vdsop)(unsigned int *, unsigned int *, void *);

int get_cpu_setup(void)
{
	void *handle = dlopen("linux-vdso.so.1", RTLD_LAZY);
	get_cpu_vdsop = NULL;
	if (handle) {
		get_cpu_vdsop = dlsym(handle, "getcpu");
		dlclose(handle);
		if (get_cpu_vdsop) {
			get_cpu = getcpu_vdso;
			return 0;
		}
	}
	return -1;
}

#endif
