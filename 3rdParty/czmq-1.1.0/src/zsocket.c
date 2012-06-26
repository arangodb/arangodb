/*  =========================================================================
    zsocket - working with 0MQ sockets

    -------------------------------------------------------------------------
    Copyright (c) 1991-2011 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of czmq, the high-level C binding for 0MQ:
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
    The zsocket class provides helper functions for 0MQ sockets. It doesn't
    wrap the 0MQ socket type, to avoid breaking all libzmq socket-related
    calls. Automatically subscribes SUB sockets to "".
@discuss
@end
*/

#include "../include/czmq_prelude.h"
#include "../include/zctx.h"
#include "../include/zclock.h"
#include "../include/zstr.h"
#include "../include/zsockopt.h"
#include "../include/zsocket.h"


//  --------------------------------------------------------------------------
//  Create a new socket within our czmq context, replaces zmq_socket.
//  If the socket is a SUB socket, automatically subscribes to everything.
//  Use this to get automatic management of the socket at shutdown.

void *
zsocket_new (zctx_t *ctx, int type)
{
    void *socket = zctx__socket_new (ctx, type);
    if (type == ZMQ_SUB)
        zsockopt_set_subscribe (socket, "");
    return socket;
}


//  --------------------------------------------------------------------------
//  Destroy the socket. You must use this for any socket created via the
//  zsocket_new method.

void
zsocket_destroy (zctx_t *ctx, void *socket)
{
    zctx__socket_destroy (ctx, socket);
}


//  --------------------------------------------------------------------------
//  Bind a socket to a formatted endpoint. If the port is specified as
//  '*', binds to any free port from ZSOCKET_DYNFROM to ZSOCKET_DYNTO
//  and returns the actual port number used. Otherwise asserts that the
//  bind succeeded with the specified port number. Always returns the
//  port number if successful.

int
zsocket_bind (void *socket, const char *format, ...)
{
    //  Ephemeral port needs 4 additional characters
    char endpoint [256 + 4];
    va_list argptr;
    va_start (argptr, format);
    int endpoint_size = vsnprintf (endpoint, 256, format, argptr);
    va_end (argptr);

    //  Port must be at end of endpoint
    int rc = 0;
    if (endpoint [endpoint_size - 2] == ':'
    &&  endpoint [endpoint_size - 1] == '*') {
        rc = -1;            //  Unless successful
        int port;
        for (port = ZSOCKET_DYNFROM; port < ZSOCKET_DYNTO; port++) {
            sprintf (endpoint + endpoint_size - 1, "%d", port);
            if (zmq_bind (socket, endpoint) == 0) {
                rc = port;
                break;
            }
        }
    }
    else {
        rc = zmq_bind (socket, endpoint);
        assert (rc == 0);
        //  Return actual port used for binding
        rc = atoi (strrchr (endpoint, ':') + 1);
    }
    return rc;
}


//  --------------------------------------------------------------------------
//  Connect a socket to a formatted endpoint
//  Checks with assertion that the connect was valid

void
zsocket_connect (void *socket, const char *format, ...)
{
    char endpoint [256];
    va_list argptr;
    va_start (argptr, format);
    vsnprintf (endpoint, 256, format, argptr);
    va_end (argptr);
    int rc = zmq_connect (socket, endpoint);
    assert (rc == 0);
}


//  --------------------------------------------------------------------------
//  Returns socket type as printable constant string

char *
zsocket_type_str (void *socket)
{
    char *type_name [] = {
        "PAIR", "PUB", "SUB", "REQ", "REP",
        "DEALER", "ROUTER", "PULL", "PUSH",
        "XPUB", "XSUB"
    };
    int type = zsockopt_type (socket);
    if (type < 0 || type > ZMQ_XSUB)
        return "UNKNOWN";
    else
        return type_name [type];
}


//  --------------------------------------------------------------------------
//  Selftest

int
zsocket_test (Bool verbose)
{
    printf (" * zsocket: ");

    //  @selftest
    zctx_t *ctx = zctx_new ();

    //  Create a detached thread, let it run
    char *interf = "*";
    char *domain = "localhost";
    int service = 5560;

    void *writer = zsocket_new (ctx, ZMQ_PUSH);
    void *reader = zsocket_new (ctx, ZMQ_PULL);
    assert (streq (zsocket_type_str (writer), "PUSH"));
    assert (streq (zsocket_type_str (reader), "PULL"));
    int rc = zsocket_bind (writer, "tcp://%s:%d", interf, service);
    assert (rc == service);
    zsocket_connect (reader, "tcp://%s:%d", domain, service);
    zstr_send (writer, "HELLO");
    char *message = zstr_recv (reader);
    assert (message);
    assert (streq (message, "HELLO"));
    free (message);

    int port = zsocket_bind (writer, "tcp://%s:*", interf);
    assert (port >= ZSOCKET_DYNFROM && port <= ZSOCKET_DYNTO);

    zsocket_destroy (ctx, writer);
    zctx_destroy (&ctx);
    //  @end

    printf ("OK\n");
    return 0;
}
