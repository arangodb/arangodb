/*  =========================================================================
    zstr - sending and receiving strings

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

#ifndef __ZSTR_H_INCLUDED__
#define __ZSTR_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Receive a string off a socket, caller must free it
char *
    zstr_recv (void *socket);

//  Receive a string off a socket if socket had input waiting
char *
    zstr_recv_nowait (void *socket);

//  Send a string to a socket in 0MQ string format
int
    zstr_send (void *socket, const char *string);

//  Send a string to a socket in 0MQ string format, with MORE flag
int
    zstr_sendm (void *socket, const char *string);

//  Send a formatted string to a socket
int
    zstr_sendf (void *socket, const char *format, ...);

//  Self test of this class
int
    zstr_test (Bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
