/*  =========================================================================
    zfile - helper functions for working with files.

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

#ifndef __ZFILE_H_INCLUDED__
#define __ZFILE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Delete file. Does not complain if the file is absent
int
    zfile_delete (char *filename);

//  Make directory (maximum one level depending on OS)
int
    zfile_mkdir (char *dirname);

//  Return 1 if file exists, else zero
int
    zfile_exists (char *filename);

//  Return size of file, or -1 if not found
ssize_t
    zfile_size (char *filename);

//  Self test of this class
int
    zfile_test (Bool verbose);
//  @end

#ifdef __cplusplus
}
#endif


#endif  //  __ZFILE_H_INCLUDED__
