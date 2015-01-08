/*    fakethr.h
 *
 *    Copyright (C) 1999, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

typedef int perl_mutex;
typedef int perl_key;

typedef struct perl_thread *perl_os_thread;
/* With fake threads, thr is global(ish) so we don't need dTHR */
#define dTHR extern int errno

struct perl_wait_queue {
    struct perl_thread *	thread;
    struct perl_wait_queue *	next;
};
typedef struct perl_wait_queue *perl_cond;

/* Ask thread.h to include our per-thread extras */
#define HAVE_THREAD_INTERN
struct thread_intern {
    perl_os_thread next_run, prev_run;  /* Linked list of runnable threads */
    perl_cond   wait_queue;             /* Wait queue that we are waiting on */
    IV          private;                /* Holds data across time slices */
    I32         savemark;               /* Holds MARK for thread join values */
};

#define init_thread_intern(t) 				\
    STMT_START {					\
	t->self = (t);					\
	(t)->i.next_run = (t)->i.prev_run = (t);	\
	(t)->i.wait_queue = 0;				\
	(t)->i.private = 0;				\
    } STMT_END

/*
 * Note that SCHEDULE() is only callable from pp code (which
 * must be expecting to be restarted). We'll have to do
 * something a bit different for XS code.
 */

#define SCHEDULE() return schedule(), PL_op

#define MUTEX_LOCK(m)
#define MUTEX_UNLOCK(m)
#define MUTEX_INIT(m)
#define MUTEX_DESTROY(m)
#define COND_INIT(c) perl_cond_init(c)
#define COND_SIGNAL(c) perl_cond_signal(c)
#define COND_BROADCAST(c) perl_cond_broadcast(c)
#define COND_WAIT(c, m)		\
    STMT_START {		\
	perl_cond_wait(c);	\
	SCHEDULE();		\
    } STMT_END
#define COND_DESTROY(c)

#define THREAD_CREATE(t, f)	f((t))
#define THREAD_POST_CREATE(t)	NOOP

#define YIELD	NOOP

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
