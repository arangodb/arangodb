/*
    Copyright (c) 2010-2011 250bpm s.r.o.
    Copyright (c) 2010-2011 Other contributors as noted in the AUTHORS file

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include "../include/zmq.h"
#include "../include/zmq_utils.h"

extern "C"
{
    void *worker(void *ctx)
    {
        //  Worker thread connects after delay of 1 second. Then it waits
        //  for 1 more second, so that async connect has time to succeed.
        zmq_sleep (1);
        void *sc = zmq_socket (ctx, ZMQ_PUSH);
        assert (sc);
        int rc = zmq_connect (sc, "inproc://timeout_test");
        assert (rc == 0);
        zmq_sleep (1);
        rc = zmq_close (sc);
        assert (rc == 0);
        return NULL;
    }
}

int main (int argc, char *argv [])
{
    fprintf (stderr, "test_timeo running...\n");

    void *ctx = zmq_init (1);
    assert (ctx);

    //  Create a disconnected socket.
    void *sb = zmq_socket (ctx, ZMQ_PULL);
    assert (sb);
    int rc = zmq_bind (sb, "inproc://timeout_test");
    assert (rc == 0);

    //  Check whether non-blocking recv returns immediately.
    zmq_msg_t msg;
    zmq_msg_init (&msg);
    rc = zmq_recv (sb, &msg, ZMQ_NOBLOCK);
    assert (rc == -1);
    assert (zmq_errno() == EAGAIN);

    //  Check whether recv timeout is honoured.
    int timeout = 500;
    size_t timeout_size = sizeof timeout;
    rc = zmq_setsockopt(sb, ZMQ_RCVTIMEO, &timeout, timeout_size);
    assert (rc == 0);    
    void *watch = zmq_stopwatch_start ();
    rc = zmq_recv (sb, &msg, 0);
    assert (rc == -1);
    assert (zmq_errno () == EAGAIN);
    unsigned long elapsed = zmq_stopwatch_stop (watch);
    assert (elapsed > 440000 && elapsed < 550000);

    //  Check whether connection during the wait doesn't distort the timeout.
    timeout = 2000;
    rc = zmq_setsockopt(sb, ZMQ_RCVTIMEO, &timeout, timeout_size);
    assert (rc == 0);
    pthread_t thread;
    rc = pthread_create (&thread, NULL, worker, ctx);
    assert (rc == 0);
    watch = zmq_stopwatch_start ();
    rc = zmq_recv (sb, &msg, 0);
    assert (rc == -1);
    assert (zmq_errno () == EAGAIN);
    elapsed = zmq_stopwatch_stop (watch);
    assert (elapsed > 1900000 && elapsed < 2100000);
    rc = pthread_join (thread, NULL);
    assert (rc == 0);

    //  Check that timeouts don't break normal message transfer.
    void *sc = zmq_socket (ctx, ZMQ_PUSH);
    assert (sc);
    rc = zmq_setsockopt(sb, ZMQ_RCVTIMEO, &timeout, timeout_size);
    assert (rc == 0);
    rc = zmq_setsockopt(sb, ZMQ_SNDTIMEO, &timeout, timeout_size);
    assert (rc == 0);
    rc = zmq_connect (sc, "inproc://timeout_test");
    assert (rc == 0);

    zmq_msg_init (&msg);
    zmq_msg_init_size (&msg, 32);
    memcpy (zmq_msg_data (&msg), "12345678ABCDEFGH12345678abcdefgh", 32);
    rc = zmq_send (sc, &msg, 0);
    assert (rc == 0);
    
    rc = zmq_recv (sb, &msg, 0);
    assert (rc == 0);
    assert (32 == zmq_msg_size(&msg));

    //  Clean-up.
    rc = zmq_close (sc);
    assert (rc == 0);
    rc = zmq_close (sb);
    assert (rc == 0);
    rc = zmq_term (ctx);
    assert (rc == 0);

    return 0 ;
}

