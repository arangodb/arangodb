/*
 * $Id:$
 * Solaris version
 * by Wolfram Gloger 2004
 */

#include <thread.h>
#include <stdio.h>

#ifndef STACKSIZE
#define STACKSIZE	32768
#endif

struct thread_st {
	char *sp;							/* stack pointer, can be 0 */
	void (*func)(struct thread_st* st);	/* must be set by user */
	thread_id id;
	int flags;
	struct user_data u;
};

static void
thread_init(void)
{
	printf("Using Solaris threads.\n");
}

static void *
thread_wrapper(void *ptr)
{
	struct thread_st *st = (struct thread_st*)ptr;

	/*printf("begin %p\n", st->sp);*/
	st->func(st);
	/*printf("end %p\n", st->sp);*/
	return NULL;
}

/* Create a thread. */
static int
thread_create(struct thread_st *st)
{
	st->flags = 0;
	if(!st->sp)
		st->sp = malloc(STACKSIZE);
	if(!st->sp) return -1;
	thr_create(st->sp, STACKSIZE, thread_wrapper, st, THR_NEW_LWP, &st->id);
	return 0;
}

/* Wait for one of several subthreads to finish. */
static void
wait_for_thread(struct thread_st st[], int n_thr,
				int (*end_thr)(struct thread_st*))
{
	int i;
	thread_t id;

	thr_join(0, &id, NULL);
	for(i=0; i<n_thr; i++)
		if(id == st[i].id) {
			if(end_thr)
				end_thr(&st[i]);
			break;
		}
}

/*
 * Local variables:
 * tab-width: 4
 * End:
 */
