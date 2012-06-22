/*  =========================================================================
    zthread - working with system threads

    -------------------------------------------------------------------------
    Copyright (c) 1991-2011 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of CZMQ, the high-level C binding for 0MQ:
    http://czmq.zeromq.org.

    This is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or (at
    your option) any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program. If not, see
    <http://www.gnu.org/licenses/>.
    =========================================================================
*/

/*
@header
    The zthread class wraps OS thread creation. It creates detached threads
    that look like normal OS threads, or attached threads that share the
    caller's 0MQ context, and get a pipe to talk back to the parent thread.
@discuss
    One problem is when our application needs child threads. If we simply
    use pthreads_create() we're faced with several issues. First, it's not
    portable to legacy OSes like win32. Second, how can a child thread get
    access to our zctx object? If we just pass it around, we'll end up
    sharing the pipe socket (which we use to talk to the agent) between
    threads, and that will then crash 0MQ. Sockets cannot be used from more
    than one thread at a time.

    So each child thread needs its own pipe to the agent. For the agent,
    this is fine, it can talk to a million threads. But how do we create
    those pipes in the child thread? We can't, not without help from the
    main thread. The solution is to wrap thread creation, like we wrap
    socket creation. To create a new thread, the app calls zctx_thread_new()
    and this method creates a dedicated zctx object, with a pipe, and then
    it passes that object to the newly minted child thread.

    The neat thing is we can hide non-portable aspects. Windows is really a
    mess when it comes to threads. Three different APIs, none of which is
    really right, so you have to do rubbish like manually cleaning up when
    a thread finishes. Anyhow, it's hidden in this class so you don't need
    to worry.

    Second neat thing about wrapping thread creation is we can make it a
    more enriching experience for all involved. One thing I do often is use
    a PAIR-PAIR pipe to talk from a thread to/from its parent. So this class
    will automatically create such a pair for each thread you start.
@end
*/

#include "../include/czmq_prelude.h"
#include "../include/zclock.h"
#include "../include/zctx.h"
#include "../include/zsocket.h"
#include "../include/zsockopt.h"
#include "../include/zstr.h"
#include "../include/zthread.h"


//  --------------------------------------------------------------------------
//  Thread creation code, wrapping POSIX and Win32 thread APIs

typedef struct {
    //  Two thread handlers, one will be set, one NULL
    zthread_attached_fn *attached;
    zthread_detached_fn *detached;
    void *args;                 //  Application arguments
    zctx_t *ctx;                //  Context object if any
    void *pipe;                 //  Pipe, if any, back to parent
#if defined (__WINDOWS__)
    HANDLE handle;              //  Win32 thread handle
#endif
} shim_t;

#if defined (__UNIX__)
//  Thread shim for UNIX calls the real thread and cleans up afterwards.
void *
s_thread_shim (void *args)
{
    assert (args);
    shim_t *shim = (shim_t *) args;
    if (shim->attached)
        shim->attached (shim->args, shim->ctx, shim->pipe);
    else
        shim->detached (shim->args);
    zctx_destroy (&shim->ctx);
    free (shim);
    return NULL;
}

#elif defined (__WINDOWS__)
//  Thread shim for Windows that wraps a POSIX-style thread handler
//  and does the _endthreadex for us automatically.
unsigned __stdcall
s_thread_shim (void *args)
{
    assert (args);
    shim_t *shim = (shim_t *) args;
    if (shim->attached)
        shim->attached (shim->args, shim->ctx, shim->pipe);
    else
        shim->detached (shim->args);

    zctx_destroy (&shim->ctx);  //  Close any dangling sockets
    free (shim);
    _endthreadex (0);           //  Terminates thread
    return 0;
}
#endif

static void
s_thread_start (shim_t *shim)
{
#if defined (__UNIX__)
    pthread_t thread;
    pthread_create (&thread, NULL, s_thread_shim, shim);
    pthread_detach (thread);

#elif defined (__WINDOWS__)
    shim->handle = (HANDLE)_beginthreadex(
        NULL,                   //  Handle is private to this process
        0,                      //  Use a default stack size for new thread
        &s_thread_shim,         //  Start real thread function via this shim
        shim,                   //  Which gets the current object as argument
        CREATE_SUSPENDED,       //  Set thread priority before starting it
        NULL);                  //  We don't use the thread ID

    assert (shim->handle);
    //  Set child thread priority to same as current
    int priority = GetThreadPriority (GetCurrentThread ());
    SetThreadPriority (shim->handle, priority);
    //  Now start thread
    ResumeThread (shim->handle);
#endif
}


//  --------------------------------------------------------------------------
//  Create a detached thread. A detached thread operates autonomously
//  and is used to simulate a separate process. It gets no ctx, and no
//  pipe.

void
zthread_new (zthread_detached_fn *thread_fn, void *args)
{
    //  Prepare argument shim for child thread
    shim_t *shim = (shim_t *) zmalloc (sizeof (shim_t));
    shim->detached = thread_fn;
    shim->args = args;
    s_thread_start (shim);
}


//  --------------------------------------------------------------------------
//  Create an attached thread. An attached thread gets a ctx and a PAIR
//  pipe back to its parent. It must monitor its pipe, and exit if the
//  pipe becomes unreadable.

void *
zthread_fork (zctx_t *ctx, zthread_attached_fn *thread_fn, void *args)
{
    //  Create our end of the pipe
    //  Pipe has HWM of 1 at both sides to block runaway writers
    void *pipe = zsocket_new (ctx, ZMQ_PAIR);
    assert (pipe);
    zsockopt_set_hwm (pipe, 1);
    zsocket_bind (pipe, "inproc://zctx-pipe-%p", pipe);

    //  Prepare argument shim for child thread
    shim_t *shim = (shim_t *) zmalloc (sizeof (shim_t));
    shim->attached = thread_fn;
    shim->args = args;
    shim->ctx = zctx_shadow (ctx);

    //  Connect child pipe to our pipe
    shim->pipe = zsocket_new (shim->ctx, ZMQ_PAIR);
    assert (shim->pipe);
    zsockopt_set_hwm (shim->pipe, 1);
    zsocket_connect (shim->pipe, "inproc://zctx-pipe-%p", pipe);

    s_thread_start (shim);
    return pipe;
}


//  --------------------------------------------------------------------------
//  Selftest

//  @selftest
static void *
s_test_detached (void *args)
{
    //  Create a socket to check it'll be automatically deleted
    zctx_t *ctx = zctx_new ();
    void *push = zsocket_new (ctx, ZMQ_PUSH);
    zctx_destroy (&ctx);
    return NULL;
}

static void
s_test_attached (void *args, zctx_t *ctx, void *pipe)
{
    //  Create a socket to check it'll be automatically deleted
    zsocket_new (ctx, ZMQ_PUSH);
    //  Wait for our parent to ping us, and pong back
    free (zstr_recv (pipe));
    zstr_send (pipe, "pong");
}

//  @end

int
zthread_test (Bool verbose)
{
    printf (" * zthread: ");

    //  @selftest
    zctx_t *ctx = zctx_new ();

    //  Create a detached thread, let it run
    zthread_new (s_test_detached, NULL);
    zclock_sleep (100);

    //  Create an attached thread, check it's safely alive
    void *pipe = zthread_fork (ctx, s_test_attached, NULL);
    zstr_send (pipe, "ping");
    char *pong = zstr_recv (pipe);
    assert (streq (pong, "pong"));
    free (pong);

    //  Everything should be cleanly closed now
    zctx_destroy (&ctx);
    //  @end

    printf ("OK\n");
    return 0;
}
