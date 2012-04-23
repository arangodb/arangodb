/*
 * $Id: t-test2.c,v 1.2 2006/03/27 16:06:20 wg Exp $
 * by Wolfram Gloger 1996-1999, 2001, 2004
 * A multi-thread test for malloc performance, maintaining a single
 * global pool of allocated bins.
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

#if !USE_MALLOC
#include <malloc.h>
#else
#include "malloc-2.8.3.h"
#endif

#include "lran2.h"
#include "t-test.h"

struct user_data {
	int max;
	unsigned long size;
	long seed;
};
#include "thread-st.h"
#include "malloc-machine.h" /* for mutex */

#define N_TOTAL		10
#ifndef N_THREADS
#define N_THREADS	2
#endif
#ifndef N_TOTAL_PRINT
#define N_TOTAL_PRINT 50
#endif
#define STACKSIZE	32768
#ifndef MEMORY
#define MEMORY		8000000l
#endif
#define SIZE		10000
#define I_MAX		10000
#define BINS_PER_BLOCK 20

#define RANDOM(d,s)	(lran2(d) % (s))

struct block {
	struct bin b[BINS_PER_BLOCK];
	mutex_t mutex;
} *blocks;

int n_blocks;

#if TEST > 0

void
bin_test(void)
{
	int b, i;

	for(b=0; b<n_blocks; b++) {
		mutex_lock(&blocks[b].mutex);
		for(i=0; i<BINS_PER_BLOCK; i++) {
			if(mem_check(blocks[b].b[i].ptr, blocks[b].b[i].size)) {
				printf("memory corrupt!\n");
				exit(1);
			}
		}
		mutex_unlock(&blocks[b].mutex);
	}
}

#endif

void
malloc_test(struct thread_st *st)
{
	struct block *bl;
	int i, b, r;
	struct lran2_st ld; /* data for random number generator */
	unsigned long rsize[BINS_PER_BLOCK];
	int rnum[BINS_PER_BLOCK];

	lran2_init(&ld, st->u.seed);
	for(i=0; i<=st->u.max;) {
#if TEST > 1
		bin_test();
#endif
		bl = &blocks[RANDOM(&ld, n_blocks)];
		r = RANDOM(&ld, 1024);
		if(r < 200) { /* free only */
			mutex_lock(&bl->mutex);
			for(b=0; b<BINS_PER_BLOCK; b++)
				bin_free(&bl->b[b]);
			mutex_unlock(&bl->mutex);
			i += BINS_PER_BLOCK;
		} else { /* alloc/realloc */
			/* Generate random numbers in advance. */
			for(b=0; b<BINS_PER_BLOCK; b++) {
				rsize[b] = RANDOM(&ld, st->u.size) + 1;
				rnum[b] = lran2(&ld);
			}
			mutex_lock(&bl->mutex);
			for(b=0; b<BINS_PER_BLOCK; b++)
				bin_alloc(&bl->b[b], rsize[b], rnum[b]);
			mutex_unlock(&bl->mutex);
			i += BINS_PER_BLOCK;
		}
#if TEST > 2
		bin_test();
#endif
	}
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

int
main(int argc, char *argv[])
{
	int i, j, bins;
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

	bins = MEMORY/size;
	if(argc > 5) bins = atoi(argv[5]);
	if(bins < BINS_PER_BLOCK) bins = BINS_PER_BLOCK;

	n_blocks = bins/BINS_PER_BLOCK;
	blocks = (struct block *)malloc(n_blocks*sizeof(*blocks));
	if(!blocks)
		exit(1);

	thread_init();
	printf("total=%d threads=%d i_max=%d size=%ld bins=%d\n",
		   n_total_max, n_thr, i_max, size, n_blocks*BINS_PER_BLOCK);

	for(i=0; i<n_blocks; i++) {
		mutex_init(&blocks[i].mutex);
		for(j=0; j<BINS_PER_BLOCK; j++) blocks[i].b[j].size = 0;
	}

	st = (struct thread_st *)malloc(n_thr*sizeof(*st));
	if(!st) exit(-1);

#if !defined NO_THREADS && (defined __sun__ || defined sun)
	/* I know of no other way to achieve proper concurrency with Solaris. */
	thr_setconcurrency(n_thr);
#endif

	/* Start all n_thr threads. */
	for(i=0; i<n_thr; i++) {
		st[i].u.max = i_max;
		st[i].u.size = size;
		st[i].u.seed = ((long)i_max*size + i) ^ n_blocks;
		st[i].sp = 0;
		st[i].func = malloc_test;
		if(thread_create(&st[i])) {
			printf("Creating thread #%d failed.\n", i);
			n_thr = i;
			break;
		}
		printf("Created thread %lx.\n", (long)st[i].id);
	}

	for(n_running=n_total=n_thr; n_running>0;) {
		wait_for_thread(st, n_thr, my_end_thread);
	}

	for(i=0; i<n_blocks; i++) {
		for(j=0; j<BINS_PER_BLOCK; j++)
			bin_free(&blocks[i].b[j]);
	}

	for(i=0; i<n_thr; i++) {
		free(st[i].sp);
	}
	free(st);
	free(blocks);
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
