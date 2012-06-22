/*  =========================================================================
    zframe - working with single message frames

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
    The zframe class provides methods to send and receive single message
    frames across 0MQ sockets. A 'frame' corresponds to one zmq_msg_t. When
    you read a frame from a socket, the zframe_more() method indicates if the
    frame is part of an unfinished multipart message. The zframe_send method
    normally destroys the frame, but with the ZFRAME_REUSE flag, you can send
    the same frame many times. Frames are binary, and this class has no
    special support for text data.
@discuss
@end
*/

#include "../include/czmq_prelude.h"
#include "../include/zctx.h"
#include "../include/zsocket.h"
#include "../include/zsockopt.h"
#include "../include/zframe.h"

//  Structure of our class

struct _zframe_t {
    zmq_msg_t zmsg;             //  zmq_msg_t blob for frame
    int more;                   //  More flag, from last read
};


//  --------------------------------------------------------------------------
//  Constructor; if size is >0, allocates frame with that size, and if data
//  is not null, copies data into frame.

zframe_t *
zframe_new (const void *data, size_t size)
{
    zframe_t
        *self;

    self = (zframe_t *) zmalloc (sizeof (zframe_t));
    if (size) {
        zmq_msg_init_size (&self->zmsg, size);
        if (data)
            memcpy (zmq_msg_data (&self->zmsg), data, size);
    }
    else
        zmq_msg_init (&self->zmsg);

    return self;
}


//  --------------------------------------------------------------------------
//  Destructor

void
zframe_destroy (zframe_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zframe_t *self = *self_p;
        zmq_msg_close (&self->zmsg);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Receive frame from socket, returns zframe_t object or NULL if the recv
//  was interrupted. Does a blocking recv, if you want to not block then use
//  zframe_recv_nowait().

zframe_t *
zframe_recv (void *socket)
{
    assert (socket);
    zframe_t *self = zframe_new (NULL, 0);
    if (zmq_recvmsg (socket, &self->zmsg, 0) < 0) {
        zframe_destroy (&self);
        return NULL;            //  Interrupted or terminated
    }
    self->more = zsockopt_rcvmore (socket);
    return self;
}


//  --------------------------------------------------------------------------
//  Receive a new frame off the socket. Returns newly allocated frame, or
//  NULL if there was no input waiting, or if the read was interrupted.

zframe_t *
zframe_recv_nowait (void *socket)
{
    assert (socket);
    zframe_t *self = zframe_new (NULL, 0);
    if (zmq_recvmsg (socket, &self->zmsg, ZMQ_DONTWAIT) < 0) {
        zframe_destroy (&self);
        return NULL;            //  Interrupted or terminated
    }
    self->more = zsockopt_rcvmore (socket);
    return self;
}


//  --------------------------------------------------------------------------
//  Send frame to socket, destroy after sending unless ZFRAME_REUSE is set.

void
zframe_send (zframe_t **self_p, void *socket, int flags)
{
    assert (socket);
    assert (self_p);
    if (*self_p) {
        zframe_t *self = *self_p;
        if (flags & ZFRAME_REUSE) {
            zmq_msg_t copy;
            zmq_msg_init (&copy);
            zmq_msg_copy (&copy, &self->zmsg);
            zmq_sendmsg (socket, &copy, (flags & ZFRAME_MORE)? ZMQ_SNDMORE: 0);
            zmq_msg_close (&copy);
        }
        else {
            zmq_sendmsg (socket, &self->zmsg, (flags & ZFRAME_MORE)? ZMQ_SNDMORE: 0);
            zframe_destroy (self_p);
        }
    }
}


//  --------------------------------------------------------------------------
//  Return size of frame.

size_t
zframe_size (zframe_t *self)
{
    assert (self);
    return zmq_msg_size (&self->zmsg);
}


//  --------------------------------------------------------------------------
//  Return pointer to frame data.

byte *
zframe_data (zframe_t *self)
{
    assert (self);
    return (byte *) zmq_msg_data (&self->zmsg);
}


//  --------------------------------------------------------------------------
//  Create a new frame that duplicates an existing frame

zframe_t *
zframe_dup (zframe_t *self)
{
    assert (self);
    return zframe_new (zframe_data (self), zframe_size (self));
}


//  --------------------------------------------------------------------------
//  Return frame data encoded as printable hex string, useful for 0MQ UUIDs.
//  Caller must free string when finished with it.

char *
zframe_strhex (zframe_t *self)
{
    static char
        hex_char [] = "0123456789ABCDEF";

    size_t size = zframe_size (self);
    byte *data = zframe_data (self);
    char *hex_str = (char *) malloc (size * 2 + 1);

    uint byte_nbr;
    for (byte_nbr = 0; byte_nbr < size; byte_nbr++) {
        hex_str [byte_nbr * 2 + 0] = hex_char [data [byte_nbr] >> 4];
        hex_str [byte_nbr * 2 + 1] = hex_char [data [byte_nbr] & 15];
    }
    hex_str [size * 2] = 0;
    return hex_str;
}


//  --------------------------------------------------------------------------
//  Return frame data copied into freshly allocated string
//  Caller must free string when finished with it.

char *
zframe_strdup (zframe_t *self)
{
    size_t size = zframe_size (self);
    char *string = (char *) malloc (size + 1);
    memcpy (string, zframe_data (self), size);
    string [size] = 0;
    return string;
}


//  --------------------------------------------------------------------------
//  Return TRUE if frame body is equal to string, excluding terminator

Bool
zframe_streq (zframe_t *self, char *string)
{
    assert (self);
    if (zframe_size (self) == strlen (string)
    &&  memcmp (zframe_data (self), string, strlen (string)) == 0)
        return TRUE;
    else
        return FALSE;
}


//  --------------------------------------------------------------------------
//  Return frame MORE indicator (1 or 0), set when reading frame from socket

int
zframe_more (zframe_t *self)
{
    assert (self);
    return self->more;
}


//  --------------------------------------------------------------------------
//  Return TRUE if two frames have identical size and data

Bool
zframe_eq (zframe_t *self, zframe_t *other)
{
    if (!self || !other)
        return FALSE;
    else
    if (zframe_size (self) == zframe_size (other)
    && memcmp (zframe_data (self), zframe_data (other),
               zframe_size (self)) == 0)
        return TRUE;
    else
        return FALSE;
}


//  --------------------------------------------------------------------------
//  Print contents of frame to stderr, prefix is ignored if null.

void
zframe_print (zframe_t *self, char *prefix)
{
    assert (self);
    if (prefix)
        fprintf (stderr, "%s", prefix);
    byte *data = zframe_data (self);
    size_t size = zframe_size (self);

    int is_bin = 0;
    uint char_nbr;
    for (char_nbr = 0; char_nbr < size; char_nbr++)
        if (data [char_nbr] < 9 || data [char_nbr] > 127)
            is_bin = 1;

    fprintf (stderr, "[%03d] ", (int) size);
    size_t max_size = is_bin? 35: 70;
    char *elipsis = "";
    if (size > max_size) {
        size = max_size;
        elipsis = "...";
    }
    for (char_nbr = 0; char_nbr < size; char_nbr++) {
        if (is_bin)
            fprintf (stderr, "%02X", (unsigned char) data [char_nbr]);
        else
            fprintf (stderr, "%c", data [char_nbr]);
    }
    fprintf (stderr, "%s\n", elipsis);
}


//  --------------------------------------------------------------------------
//  Set new contents for frame

void
zframe_reset (zframe_t *self, const void *data, size_t size)
{
    assert (self);
    assert (data);
    zmq_msg_close (&self->zmsg);
    zmq_msg_init_size (&self->zmsg, size);
    memcpy (zmq_msg_data (&self->zmsg), data, size);
}


//  --------------------------------------------------------------------------
//  Selftest

int
zframe_test (Bool verbose)
{
    printf (" * zframe: ");

    //  @selftest
    zctx_t *ctx = zctx_new ();

    void *output = zsocket_new (ctx, ZMQ_PAIR);
    zsocket_bind (output, "inproc://zframe.test");
    void *input = zsocket_new (ctx, ZMQ_PAIR);
    zsocket_connect (input, "inproc://zframe.test");

    //  Send five different frames, test ZFRAME_MORE
    int frame_nbr;
    for (frame_nbr = 0; frame_nbr < 5; frame_nbr++) {
        zframe_t *frame = zframe_new ("Hello", 5);
        zframe_send (&frame, output, ZFRAME_MORE);
    }
    //  Send same frame five times, test ZFRAME_REUSE
    zframe_t *frame = zframe_new ("Hello", 5);
    for (frame_nbr = 0; frame_nbr < 5; frame_nbr++) {
        zframe_send (&frame, output, ZFRAME_MORE + ZFRAME_REUSE);
    }
    assert (frame);
    zframe_t *copy = zframe_dup (frame);
    assert (zframe_eq (frame, copy));
    zframe_destroy (&frame);
    assert (!zframe_eq (frame, copy));
    assert (zframe_size (copy) == 5);
    zframe_destroy (&copy);
    assert (!zframe_eq (frame, copy));

    //  Send END frame
    frame = zframe_new ("NOT", 3);
    zframe_reset (frame, "END", 3);
    char *string = zframe_strhex (frame);
    assert (streq (string, "454E44"));
    free (string);
    string = zframe_strdup (frame);
    assert (streq (string, "END"));
    free (string);
    zframe_send (&frame, output, 0);

    //  Read and count until we receive END
    frame_nbr = 0;
    for (frame_nbr = 0;; frame_nbr++) {
        zframe_t *frame = zframe_recv (input);
        if (zframe_streq (frame, "END")) {
            zframe_destroy (&frame);
            break;
        }
        assert (zframe_more (frame));
        zframe_destroy (&frame);
    }
    assert (frame_nbr == 10);
    frame = zframe_recv_nowait (input);
    assert (frame == NULL);

    zctx_destroy (&ctx);
    //  @end
    printf ("OK\n");
    return 0;
}
