/*  =========================================================================
    zfile - helper functions for working with files.

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
    The zfile class provides methods to work with files.
@discuss
@end
*/

#include "../include/czmq_prelude.h"

//  Delete file, return 0 if OK, -1 if not possible.

int
zfile_delete (char *filename)
{
    assert (filename);
#if (defined (__WINDOWS__))
    return DeleteFile (filename) ? 0 : -1;
#else
    return unlink (filename);
#endif
}


//  Make directory (maximum one level depending on OS)

int
zfile_mkdir (char *dirname)
{
#if (defined (__WINDOWS__))
    return !CreateDirectory (dirname, NULL);
#else
    return mkdir (dirname, 0755);    //  User RWE Group RE World RE
#endif
}


//  Returns the file mode for the specified file or directory name;
//  returns 0 if the specified file does not exist.

static int
s_file_mode (char *filename)
{
    assert (filename);

#if (defined (__WINDOWS__))
    DWORD dwfa = GetFileAttributes (filename);
    if (dwfa == 0xffffffff)
        return 0;

    int mode = 0;
    if (dwfa & FILE_ATTRIBUTE_DIRECTORY)
        mode |= S_IFDIR;
    else
        mode |= S_IFREG;

    if (!(dwfa & FILE_ATTRIBUTE_HIDDEN))
        mode |= S_IREAD;

    if (!(dwfa & FILE_ATTRIBUTE_READONLY))
        mode |= S_IWRITE;

    return mode;
#else
    struct stat stat_buf;
    if (stat ((char *) filename, &stat_buf) == 0)
        return stat_buf.st_mode;
    else
        return 0;
#endif
}

//  Return 1 if file exists, else zero
int
zfile_exists (char *filename)
{
    assert (filename);
    return s_file_mode (filename) > 0;
}


//  Return size of file, or -1 if not found
ssize_t
zfile_size (char *filename)
{
    struct stat
        stat_buf;

    assert (filename);
    if (stat ((char *) filename, &stat_buf) == 0)
        return stat_buf.st_size;
    else
        return -1;
}

//  --------------------------------------------------------------------------
//  Selftest

int
zfile_test (Bool verbose)
{
    printf (" * zmsg: ");

    //  @selftest
    int rc = zfile_delete ("nosuchfile");
    assert (rc == -1);

    rc = zfile_exists ("nosuchfile");
    assert (rc == FALSE);

    rc = (int) zfile_size ("nosuchfile");
    assert (rc == -1);

    //  @end
    printf ("OK\n");
    return 0;
}
