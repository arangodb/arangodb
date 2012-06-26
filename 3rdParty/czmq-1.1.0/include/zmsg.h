/*  =========================================================================
    zmsg - working with multipart messages

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

#ifndef __ZMSG_H_INCLUDED__
#define __ZMSG_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _zmsg_t zmsg_t;

//  @interface
//  Create a new empty message object
zmsg_t *
    zmsg_new (void);

//  Destroy a message object and all frames it contains
void
    zmsg_destroy (zmsg_t **self_p);

//  Read 1 or more frames off the socket, into a new message object
zmsg_t *
    zmsg_recv (void *socket);

//  Send a message to the socket, and then destroy it
void
    zmsg_send (zmsg_t **self_p, void *socket);

//  Return number of frames in message
size_t
    zmsg_size (zmsg_t *self);

//  Return combined size of all frames in message
size_t
    zmsg_content_size (zmsg_t *self);

//  Push frame to front of message, before first frame
void
    zmsg_push (zmsg_t *self, zframe_t *frame);

//  Pop frame off front of message, caller now owns frame
zframe_t *
    zmsg_pop (zmsg_t *self);

//  Add frame to end of message, after last frame
void
    zmsg_add (zmsg_t *self, zframe_t *frame);

//  Push block of memory as new frame to front of message
void
    zmsg_pushmem (zmsg_t *self, const void *src, size_t size);

//  Push block of memory as new frame to end of message
void
    zmsg_addmem (zmsg_t *self, const void *src, size_t size);

//  Push string as new frame to front of message
void
    zmsg_pushstr (zmsg_t *self, const char *format, ...);

//  Push string as new frame to end of message
void
    zmsg_addstr (zmsg_t *self, const char *format, ...);

//  Pop frame off front of message, return as fresh string
char *
    zmsg_popstr (zmsg_t *self);

//  Push frame to front of message, before first frame
//  Pushes an empty frame in front of frame
void
    zmsg_wrap (zmsg_t *self, zframe_t *frame);

//  Pop frame off front of message, caller now owns frame
//  If next frame is empty, pops and destroys that empty frame.
zframe_t *
    zmsg_unwrap (zmsg_t *self);

//  Remove frame from message, at any position, caller owns it
void
    zmsg_remove (zmsg_t *self, zframe_t *frame);

//  Return first frame in message, or null
zframe_t *
    zmsg_first (zmsg_t *self);

//  Return next frame in message, or null
zframe_t *
    zmsg_next (zmsg_t *self);

//  Return last frame in message, or null
zframe_t *
    zmsg_last (zmsg_t *self);

//  Save message to an open file, return 0 if OK, else -1.
int
    zmsg_save (zmsg_t *self, FILE *file);

//  Load/append an open file into message, create new message if
//  null message provided.
zmsg_t *
    zmsg_load (zmsg_t *self, FILE *file);

//  Encode message to a new buffer, return buffer size
size_t
    zmsg_encode (zmsg_t *self, byte **buffer);

//  Decode a buffer into a new message, returns NULL if buffer is not
//  properly formatted.
zmsg_t *
    zmsg_decode (byte *buffer, size_t buffer_size);

//  Create copy of message, as new message object
zmsg_t *
    zmsg_dup (zmsg_t *self);

//  Print message to stderr, for debugging
void
    zmsg_dump (zmsg_t *self);

//  Self test of this class
int
    zmsg_test (Bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
