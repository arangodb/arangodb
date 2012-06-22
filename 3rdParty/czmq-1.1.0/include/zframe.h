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

#ifndef __ZFRAME_H_INCLUDED__
#define __ZFRAME_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _zframe_t zframe_t;

//  @interface
#define ZFRAME_MORE     1
#define ZFRAME_REUSE    2

//  Create a new frame with optional size, and optional data
zframe_t *
    zframe_new (const void *data, size_t size);

//  Destroy a frame
void
    zframe_destroy (zframe_t **self_p);

//  Receive frame from socket, returns zframe_t object or NULL if the recv
//  was interrupted. Does a blocking recv, if you want to not block then use
//  zframe_recv_nowait().
zframe_t *
    zframe_recv (void *socket);

//  Receive a new frame off the socket. Returns newly allocated frame, or
//  NULL if there was no input waiting, or if the read was interrupted.
zframe_t *
    zframe_recv_nowait (void *socket);

//  Send a frame to a socket, destroy frame after sending
void
    zframe_send (zframe_t **self_p, void *socket, int flags);

//  Return number of bytes in frame data
size_t
    zframe_size (zframe_t *self);

//  Return address of frame data
byte *
    zframe_data (zframe_t *self);

//  Create a new frame that duplicates an existing frame
zframe_t *
    zframe_dup (zframe_t *self);

//  Return frame data encoded as printable hex string
char *
    zframe_strhex (zframe_t *self);

//  Return frame data copied into freshly allocated string
char *
    zframe_strdup (zframe_t *self);

//  Return TRUE if frame body is equal to string, excluding terminator
Bool
    zframe_streq (zframe_t *self, char *string);

//  Return frame 'more' property
int
    zframe_more (zframe_t *self);

//  Return TRUE if two frames have identical size and data
//  If either frame is NULL, equality is always false.
Bool
    zframe_eq (zframe_t *self, zframe_t *other);

//  Print contents of frame to stderr
void
    zframe_print (zframe_t *self, char *prefix);

//  Set new contents for frame
void
    zframe_reset (zframe_t *self, const void *data, size_t size);

//  Self test of this class
int
    zframe_test (Bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
