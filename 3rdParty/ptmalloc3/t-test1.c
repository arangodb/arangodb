/*
 * $Id: t-test1.c,v 1.2 2006/03/27 16:05:13 wg Exp $
 * by Wolfram Gloger 1996-1999, 2001, 2004, 2006
 * A multi-thread test for malloc performance, maintaining one pool of
 * allocated bins per thread.
 */

#if (defined __STDC__ && __STDC__) || defined __cplusplus
# include <stdlib.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/mman.h>

#if !USE_MALLOC
#include <malloc.h>
#else
#include "malloc-2.8.3.h"
#endif

#include "lran2.h"
#include "t-test.h"

struct user_data {
	int bins, max;
	unsigned long size;
	long seed;
};
#include "thread-st.h"

#define N_TOTAL		10
#ifndef N_THREADS
#define N_THREADS	2
#endif
#ifndef N_TOTAL_PRINT
#define N_TOTAL_PRINT 50
#endif
#ifndef MEMORY
#define MEMORY		8000000l
#endif
#define SIZE		10000
#define I_MAX		10000
#define ACTIONS_MAX	30
#ifndef TEST_FORK
#define TEST_FORK 0
#endif

#define RANDOM(d,s)	(lran2(d) % (s))

struct bin_info {
	struct bin *m;
	unsigned long size, bins;
};

#if TEST > 0

void
bin_test(struct bin_info *p)
{
	int b;

	for(b=0; b<p->bins; b++) {
		if(mem_check(p->m[b].ptr, p->m[b].size)) {
			printf("memory corrupt!\n");
			abort();
		}
	}
}

#endif

void
malloc_test(struct thread_st *st)
{
	int b, i, j, actions, pid = 1;
	struct bin_info p;
	struct lran2_st ld; /* data for random number generator */

	lran2_init(&ld, st->u.seed);
#if TEST_FORK>0
	if(RANDOM(&ld, TEST_FORK) == 0) {
		int status;

#if !USE_THR
		pid = fork();
#else
		pid = fork1();
#endif
		if(pid > 0) {
		    /*printf("forked, waiting for %d...\n", pid);*/
			waitpid(pid, &status, 0);
			printf("done with %d...\n", pid);
			if(!WIFEXITED(status)) {
				printf("child term with signal %d\n", WTERMSIG(status));
				exit(1);
			}
			return;
		}
		exit(0);
	}
#endif
	p.m = (struct bin *)malloc(st->u.bins*sizeof(*p.m));
	p.bins = st->u.bins;
	p.size = st->u.size;
	for(b=0; b<p.bins; b++) {
		p.m[b].size = 0;
		p.m[b].ptr = NULL;
		if(RANDOM(&ld, 2) == 0)
			bin_alloc(&p.m[b], RANDOM(&ld, p.size) + 1, lran2(&ld));
	}
	for(i=0; i<=st->u.max;) {
#if TEST > 1
		bin_test(&p);
#endif
		actions = RANDOM(&ld, ACTIONS_MAX);
#if USE_MALLOC && MALLOC_DEBUG
		if(actions < 2) { mallinfo(); }
#endif
		for(j=0; j<actions; j++) {
			b = RANDOM(&ld, p.bins);
			bin_free(&p.m[b]);
		}
		i += actions;
		actions = RANDOM(&ld, ACTIONS_MAX);
		for(j=0; j<actions; j++) {
			b = RANDOM(&ld, p.bins);
			bin_alloc(&p.m[b], RANDOM(&ld, p.size) + 1, lran2(&ld));
#if TEST > 2
			bin_test(&p);
#endif
		}
#if 0 /* Test illegal free()s while setting MALLOC_CHECK_ */
		for(j=0; j<8; j++) {
			b = RANDOM(&ld, p.bins);
			if(p.m[b].ptr) {
			  int offset = (RANDOM(&ld, 11) - 5)*8;
			  char *rogue = (char*)(p.m[b].ptr) + offset;
			  /*printf("p=%p rogue=%p\n", p.m[b].ptr, rogue);*/
			  free(rogue);
			}
		}
#endif
		i += actions;
	}
	for(b=0; b<p.bins; b++)
		bin_free(&p.m[b]);
	free(p.m);
	if(pid == 0)
		exit(0);
}

int n_total=0, n_total_max=N_TOTAL, n_running;

int
my_end_thread(struct thread_st *st)
{
	/* Thread st has finished.  Start a new one. */
#if 0
	printf("Thread %lx terminated.\n", (long)st->id);
#endif
	if(n_total >= n_total_max) {
		n_running--;
	} else if(st->u.seed++, thread_create(st)) {
		printf("Creating thread #%d failed.\n", n_total);
	} else {
		n_total++;
		if(n_total%N_TOTAL_PRINT == 0)
			printf("n_total = %d\n", n_total);
	}
	return 0;
}

#if 0
/* Protect address space for allocation of n threads by LinuxThreads.  */
static void
protect_stack(int n)
{
	char buf[2048*1024];
	char* guard;
	size_t guard_size = 2*2048*1024UL*(n+2);

	buf[0] = '\0';
	guard = (char*)(((unsigned long)buf - 4096)& ~4095UL) - guard_size;
	printf("Setting up stack guard at %p\n", guard);
	if(mmap(guard, guard_size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,
			-1, 0)
	   != guard)
		printf("failed!\n");
}
#endif

int
main(int argc, char *argv[])
{
	int i, bins;
	int n_thr=N_THREADS;
	int i_max=I_MAX;
	unsigned long size=SIZE;
	struct thread_st *st;

#if USE_MALLOC && USE_STARTER==2
	ptmalloc_init();
	printf("ptmalloc_init\n");
#endif

	if(argc > 1) n_total_max = atoi(argv[1]);
	if(n_total_max < 1) n_thr = 1;
	if(argc > 2) n_thr = atoi(argv[2]);
	if(n_thr < 1) n_thr = 1;
	if(n_thr > 100) n_thr = 100;
	if(argc > 3) i_max = atoi(argv[3]);

	if(argc > 4) size = atol(argv[4]);
	if(size < 2) size = 2;

	bins = MEMORY/(size*n_thr);
	if(argc > 5) bins = atoi(argv[5]);
	if(bins < 4) bins = 4;

	/*protect_stack(n_thr);*/

	thread_init();
	printf("total=%d threads=%d i_max=%d size=%ld bins=%d\n",
		   n_total_max, n_thr, i_max, size, bins);

	st = (struct thread_st *)malloc(n_thr*sizeof(*st));
	if(!st) exit(-1);

#if !defined NO_THREADS && (defined __sun__ || defined sun)
	/* I know of no other way to achieve proper concurrency with Solaris. */
	thr_setconcurrency(n_thr);
#endif

	/* Start all n_thr threads. */
	for(i=0; i<n_thr; i++) {
		st[i].u.bins = bins;
		st[i].u.max = i_max;
		st[i].u.size = size;
		st[i].u.seed = ((long)i_max*size + i) ^ bins;
		st[i].sp = 0;
		st[i].func = malloc_test;
		if(thread_create(&st[i])) {
			printf("Creating thread #%d failed.\n", i);
			n_thr = i;
			break;
		}
		printf("Created thread %lx.\n", (long)st[i].id);
	}

	/* Start an extra thread so we don't run out of stacks. */
	if(0) {
		struct thread_st lst;
		lst.u.bins = 10; lst.u.max = 20; lst.u.size = 8000; lst.u.seed = 8999;
		lst.sp = 0;
		lst.func = malloc_test;
		if(thread_create(&lst)) {
			printf("Creating thread #%d failed.\n", i);
		} else {
			wait_for_thread(&lst, 1, NULL);
		}
	}

	for(n_running=n_total=n_thr; n_running>0;) {
		wait_for_thread(st, n_thr, my_end_thread);
	}
	for(i=0; i<n_thr; i++) {
		free(st[i].sp);
	}
	free(st);
#if USE_MALLOC
	malloc_stats();
#endif
	printf("Done.\n");
	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * End:
 */
