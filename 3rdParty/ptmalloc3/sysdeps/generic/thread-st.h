/*
 * $Id:$
 * Generic version: no threads.
 * by Wolfram Gloger 2004
 */

#include <stdio.h>

struct thread_st {
	char *sp;							/* stack pointer, can be 0 */
	void (*func)(struct thread_st* st);	/* must be set by user */
	int id;
	int flags;
	struct user_data u;
};

static void
thread_init(void)
{
	printf("No threads.\n");
}

/* Create a thread. */
static int
thread_create(struct thread_st *st)
{
	st->flags = 0;
	st->id = 1;
	st->func(st);
	return 0;
}

/* Wait for one of several subthreads to finish. */
static void
wait_for_thread(struct thread_st st[], int n_thr,
				int (*end_thr)(struct thread_st*))
{
	int i;
	for(i=0; i<n_thr; i++)
		if(end_thr)
			end_thr(&st[i]);
}

/*
 * Local variables:
 * tab-width: 4
 * End:
 */
