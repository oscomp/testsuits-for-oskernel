#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

size_t b_malloc_sparse(void *dummy)
{
	void *p[10000];
	size_t i;
	for (i=0; i<sizeof p/sizeof *p; i++) {
		p[i] = malloc(4000);
		memset(p[i], 0, 4000);
	}
	for (i=0; i<sizeof p/sizeof *p; i++)
		if (i%150) free(p[i]);
	return 0;
}

size_t b_malloc_bubble(void *dummy)
{
	void *p[10000];
	size_t i;
	for (i=0; i<sizeof p/sizeof *p; i++) {
		p[i] = malloc(4000);
		memset(p[i], 0, 4000);
	}
	for (i=0; i<sizeof p/sizeof *p-1; i++)
		free(p[i]);
	return 0;
}

size_t b_malloc_tiny1(void *dummy)
{
	void *p[10000];
	size_t i;
	for (i=0; i<sizeof p/sizeof *p; i++) {
		p[i] = malloc((i%4+1)*16);
	}
	for (i=0; i<sizeof p/sizeof *p; i++) {
		free(p[i]);
	}
	return 0;
}

size_t b_malloc_tiny2(void *dummy)
{
	void *p[10000];
	size_t i;
	for (i=0; i<sizeof p/sizeof *p; i++) {
		p[i] = malloc((i%4+1)*16);
	}
	for (i=1; i; i = (i+57)%(sizeof p/sizeof *p))
		free(p[i]);
	return 0;
}

size_t b_malloc_big1(void *dummy)
{
	void *p[2000];
	size_t i;
	for (i=0; i<sizeof p/sizeof *p; i++) {
		p[i] = malloc((i%4+1)*16384);
	}
	for (i=0; i<sizeof p/sizeof *p; i++) {
		free(p[i]);
	}
	return 0;
}

size_t b_malloc_big2(void *dummy)
{
	void *p[2000];
	size_t i;
	for (i=0; i<sizeof p/sizeof *p; i++) {
		p[i] = malloc((i%4+1)*16384);
	}
	for (i=1; i; i = (i+57)%(sizeof p/sizeof *p))
		free(p[i]);
	return 0;
}


#define LOOPS 100000
#define SH_COUNT 300
#define PV_COUNT 300
#define MAX_SZ 500
#define DEF_SZ 40

struct foo {
	pthread_mutex_t lock;
	void *mem;
};

static unsigned rng(unsigned *r)
{
	return *r = *r * 1103515245 + 12345;
}


static void *stress(void *arg)
{
	struct foo *foo = arg;
	unsigned r = (unsigned)pthread_self();
	int i, j;
	size_t sz;
	void *p;

	for (i=0; i<LOOPS; i++) {
		j = rng(&r) % SH_COUNT;
		sz = rng(&r) % MAX_SZ;
		pthread_mutex_lock(&foo[j].lock);
		p = foo[j].mem;
		foo[j].mem = 0;
		pthread_mutex_unlock(&foo[j].lock);
		free(p);
		if (!p) {
			p = malloc(sz);
			pthread_mutex_lock(&foo[j].lock);
			if (!foo[j].mem) foo[j].mem = p, p = 0;
			pthread_mutex_unlock(&foo[j].lock);
			free(p);
		}
	}
	return 0;
}

size_t b_malloc_thread_stress(void *dummy)
{
	struct foo foo[SH_COUNT] = {0};
	pthread_t td1, td2;
	void *res;
	int i;

	pthread_create(&td1, 0, stress, foo);
	pthread_create(&td2, 0, stress, foo);
	pthread_join(td1, &res);
	pthread_join(td2, &res);
	return 0;
}

size_t b_malloc_thread_local(void *dummy)
{
	struct foo foo1[SH_COUNT] = {0};
	struct foo foo2[SH_COUNT] = {0};
	pthread_t td1, td2;
	void *res;
	int i;

	pthread_create(&td1, 0, stress, foo1);
	pthread_create(&td2, 0, stress, foo2);
	pthread_join(td1, &res);
	pthread_join(td2, &res);
	return 0;
}
