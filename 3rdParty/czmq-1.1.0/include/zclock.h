/*  =========================================================================
    zclock - millisecond clocks and delays

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

#ifndef __ZCLOCK_H_INCLUDED__
#define __ZCLOCK_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Sleep for a number of milliseconds
void
    zclock_sleep (int msecs);

//  Return current system clock as milliseconds
int64_t
    zclock_time (void);

//  Print formatted string to stdout, prefixed by date/time and
//  terminated with a newline.
void
    zclock_log (const char *format, ...);

//  Self test of this class
int
    zclock_test (Bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
