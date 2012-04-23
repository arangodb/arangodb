/*
 * $Id:$
 * sproc version
 * by Wolfram Gloger 2001, 2004, 2006
 */

#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/prctl.h>

#ifndef STACKSIZE
#define STACKSIZE	32768
#endif

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
	printf("Using sproc() threads.\n");
}

static void
thread_wrapper(void *ptr, size_t stack_len)
{
	struct thread_st *st = (struct thread_st*)ptr;

	/*printf("begin %p\n", st->sp);*/
	st->func(st);
	/*printf("end %p\n", st->sp);*/
}

/* Create a thread. */
static int
thread_create(struct thread_st *st)
{
	st->flags = 0;
	if(!st->sp)
		st->sp = malloc(STACKSIZE);
	if(!st->sp) return -1;
	st->id = sprocsp(thread_wrapper, PR_SALL, st, st->sp+STACKSIZE, STACKSIZE);
	if(st->id < 0) {
		return -1;
	}
	return 0;
}

/* Wait for one of several subthreads to finish. */
static void
wait_for_thread(struct thread_st st[], int n_thr,
				int (*end_thr)(struct thread_st*))
{
	int i;
	int id;

		int status = 0;
		id = wait(&status);
		if(status != 0) {
			if(WIFSIGNALED(status))
				printf("thread %id terminated by signal %d\n",
					   id, WTERMSIG(status));
			else
				printf("thread %id exited with status %d\n",
					   id, WEXITSTATUS(status));
		}
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
