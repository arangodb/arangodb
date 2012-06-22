/*  =========================================================================
    zctx - working with 0MQ contexts

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
    The zctx class wraps 0MQ contexts. It manages open sockets in the context
    and automatically closes these before terminating the context. It provides
    a simple way to set the linger timeout on sockets, and configure contexts
    for number of I/O threads. Sets-up signal (interrrupt) handling for the
    process.

    The zctx class has these main features:

    * Tracks all open sockets and automatically closes them before calling
    zmq_term(). This avoids an infinite wait on open sockets.

    * Automatically configures sockets with a ZMQ_LINGER timeout you can
    define, and which defaults to zero. The default behavior of zctx is
    therefore like 0MQ/2.0, immediate termination with loss of any pending
    messages. You can set any linger timeout you like by calling the
    zctx_set_linger() method.

    * Moves the iothreads configuration to a separate method, so that default
    usage is 1 I/O thread. Lets you configure this value.

    * Sets up signal (SIGINT and SIGTERM) handling so that blocking calls
    such as zmq_recv() and zmq_poll() will return when the user presses
    Ctrl-C.

@discuss
@end
*/

#include "../include/czmq_prelude.h"
#include "../include/zlist.h"
#include "../include/zstr.h"
#include "../include/zframe.h"
#include "../include/zsockopt.h"
#include "../include/zctx.h"
#include "../include/zsocket.h"

//  Structure of our class

struct _zctx_t {
    void *context;              //  Our 0MQ context
    zlist_t *sockets;           //  Sockets held by this thread
    Bool main;                  //  TRUE if we're the main thread
    int iothreads;              //  Number of IO threads, default 1
    int linger;                 //  Linger timeout, default 0
};


//  ---------------------------------------------------------------------
//  Signal handling
//

//  This is a global variable accessible to CZMQ application code
int zctx_interrupted = 0;
#if defined (__UNIX__)
static void s_signal_handler (int signal_value)
{
    zctx_interrupted = 1;
}
#endif

//  --------------------------------------------------------------------------
//  Constructor

zctx_t *
zctx_new (void)
{
    zctx_t
        *self;

    self = (zctx_t *) zmalloc (sizeof (zctx_t));
    self->sockets = zlist_new ();
    self->iothreads = 1;
    self->main = TRUE;

#if defined (__UNIX__)
    //  Install signal handler for SIGINT and SIGTERM
    struct sigaction action;
    action.sa_handler = s_signal_handler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGTERM, &action, NULL);
#endif

    return self;
}


//  --------------------------------------------------------------------------
//  Destructor

void
zctx_destroy (zctx_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zctx_t *self = *self_p;
        while (zlist_size (self->sockets))
            zctx__socket_destroy (self, zlist_first (self->sockets));
        zlist_destroy (&self->sockets);
        if (self->main && self->context)
            zmq_term (self->context);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Create new shadow context, returns context object

zctx_t *
zctx_shadow (zctx_t *ctx)
{
    zctx_t
        *self;

    //  Shares same 0MQ context but has its own list of sockets so that
    //  we create, use, and destroy sockets only within a single thread.
    self = (zctx_t *) zmalloc (sizeof (zctx_t));
    self->sockets = zlist_new ();
    self->context = ctx->context;
    return self;
}


//  --------------------------------------------------------------------------
//  Configure number of I/O threads in context, only has effect if called
//  before creating first socket. Default I/O threads is 1, sufficient for
//  all except very high volume applications.

void
zctx_set_iothreads (zctx_t *self, int iothreads)
{
    assert (self);
    self->iothreads = iothreads;
}


//  --------------------------------------------------------------------------
//  Configure linger timeout in msecs. Call this before destroying sockets or
//  context. Default is no linger, i.e. any pending messages or connects will
//  be lost.

void
zctx_set_linger (zctx_t *self, int linger)
{
    assert (self);
    self->linger = linger;
}


//  --------------------------------------------------------------------------
//  Create socket within this context, for CZMQ use only

void *
zctx__socket_new (zctx_t *self, int type)
{
    assert (self);
    //  Initialize context now if necessary
    if (self->context == NULL)
        self->context = zmq_init (self->iothreads);
    assert (self->context);
    //  Create and register socket
    void *socket = zmq_socket (self->context, type);
    if (socket) {
        assert (socket);
        zlist_push (self->sockets, socket);
    }
    return socket;
}


//  --------------------------------------------------------------------------
//  Destroy socket within this context, for CZMQ use only

void
zctx__socket_destroy (zctx_t *self, void *socket)
{
    assert (self);
    assert (socket);
    zsockopt_set_linger (socket, self->linger);
    zmq_close (socket);
    zlist_remove (self->sockets, socket);
}


//  --------------------------------------------------------------------------
//  Selftest

int
zctx_test (Bool verbose)
{
    printf (" * zctx: ");

    //  @selftest
    //  Create and destroy a context without using it
    zctx_t *ctx = zctx_new ();
    assert (ctx);
    zctx_destroy (&ctx);
    assert (ctx == NULL);

    //  Create a context with many busy sockets, destroy it
    ctx = zctx_new ();
    zctx_set_iothreads (ctx, 1);
    zctx_set_linger (ctx, 5);       //  5 msecs
    void *s1 = zctx__socket_new (ctx, ZMQ_PAIR);
    void *s2 = zctx__socket_new (ctx, ZMQ_XREQ);
    void *s3 = zctx__socket_new (ctx, ZMQ_REQ);
    void *s4 = zctx__socket_new (ctx, ZMQ_REP);
    void *s5 = zctx__socket_new (ctx, ZMQ_PUB);
    void *s6 = zctx__socket_new (ctx, ZMQ_SUB);
    zsocket_connect (s1, "tcp://127.0.0.1:5555");
    zsocket_connect (s2, "tcp://127.0.0.1:5555");
    zsocket_connect (s3, "tcp://127.0.0.1:5555");
    zsocket_connect (s4, "tcp://127.0.0.1:5555");
    zsocket_connect (s5, "tcp://127.0.0.1:5555");
    zsocket_connect (s6, "tcp://127.0.0.1:5555");

    //  Everything should be cleanly closed now
    zctx_destroy (&ctx);
    //  @end

    printf ("OK\n");
    return 0;
}
