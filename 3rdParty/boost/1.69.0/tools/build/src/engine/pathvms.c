/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/* This file is ALSO:
 * Copyright 2001-2004 David Abrahams.
 * Copyright 2005 Rene Rivera.
 * Copyright 2015 Artur Shepilko.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */


/*
 * pathvms.c - VMS-specific path manipulation support
 *
 * This implementation is based on POSIX-style path manipulation.
 *
 * VMS CTRL directly supports both POSIX- and native VMS-style path expressions,
 * with the POSIX-to-VMS path translation performed internally by the same
 * set of functions. For the most part such processing is transparent, with
 * few differences mainly related to file-versions (in POSIX mode only the recent
 * version is visible).
 *
 * This should allow us to some extent re-use pathunix.c implementation.
 *
 * Thus in jam-files the path references can also remain POSIX/UNIX-like on all
 * levels EXCEPT in actions scope, where the path references must be translated
 * to the native VMS-style. This approach is somewhat similar to jam CYGWIN
 * handling.
 *
 *
 * External routines:
 *  path_register_key()
 *  path_as_key()
 *  path_done()
 *
 * External routines called only via routines in pathsys.c:
 *  path_get_process_id_()
 *  path_get_temp_path_()
 *  path_translate_to_os_()
 */


#include "jam.h"

#ifdef OS_VMS

#include "pathsys.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>  /* needed for getpid() */
#include <unixlib.h>  /* needed for decc$to_vms() */


/*
 * path_get_process_id_()
 */

unsigned long path_get_process_id_( void )
{
    return getpid();
}


/*
 * path_get_temp_path_()
 */

void path_get_temp_path_( string * buffer )
{
    char const * t = getenv( "TMPDIR" );
    string_append( buffer, t ? t : "/tmp" );
}


/*
 * translate_path_posix2vms()
 *
 * POSIX-to-VMS file specification translation:
 *
 * Translation is performed with decc$to_vms() CTRL routine (default decc$features)
 * Some limitations apply:
 *   -- ODS-2 compliant file specs only (no spaces, punctuation chars etc.)
 *
 *   -- wild-cards are not allowed
 *      In general decc$to_vms() can expand the wildcard for existing files,
 *      yet it cannot retain wild-cards in translated spec. Use GLOB for this.
 *
 *   -- rooted path must refer to an existing/defined device or root-dir
 *      (e.g.  /defconcealed/dir/file.ext  or /existingrootdir/dir/file.ext )
 *
 *   -- POSIX dir/no-type-file path ambiguity (e.g. dir/newsubdir vs. dir/newfile
 *      is handled as follows:
 *
 *   1) first try as directory:
 *      -- if translated (may be a dir): means the file-path has no .type/suffix
 *      -- if not translated, then it may be a file (has .type) OR invalid spec
 *   2) then try as file:
 *      -- if translated and also is a dir -- check if such file exists (stat)
 *      -- if not translated, but is a dir -- return as dir
 *
 *   NOTE: on VMS it's possible to have both a file and a dir of the same name
 *   appear in the same directory. In such case _directory_ intent is assumed.
 *
 *   It's preferable to avoid such naming ambiguity in this context, so
 *   append an empty .type to specify a no-type file (eg. "filename.")
 *
 */


static string * m_vmsfilespec = NULL;

/*
 * copy_vmsfilespec() - decc$to_vms action routine for matched filenames
 */

static int copy_vmsfilespec( char * f, int type )
{
  assert ( NULL != m_vmsfilespec && "Must be bound to a valid object" );

  string_copy( m_vmsfilespec, f );

  /* 0:Exit on first match (1:Process all) */
  return 0;
}


static int translate_path_posix2vms( string * path )
{
    int translated = 0;

    string as_dir[ 1 ];
    string as_file[ 1 ];
    int dir_count;
    int file_count;

    unsigned char is_dir;
    unsigned char is_file;
    unsigned char is_ambiguous;

    string_new( as_dir );
    string_new( as_file );


    m_vmsfilespec = as_dir;

    /* MATCH 0:do not allow wildcards, 0:allow directories (2:dir only) */
    dir_count = decc$to_vms( path->value, copy_vmsfilespec, 0, 2 );


    m_vmsfilespec = as_file;

    /* MATCH 0:do not allow wildcards, 0:allow directories (2:dir only) */
    file_count = decc$to_vms( path->value, copy_vmsfilespec, 0, 0 );

    m_vmsfilespec = NULL;


    translated = ( file_count || dir_count );

    if ( file_count && dir_count )
    {
        struct stat statbuf;

        /* use as_file only when exists AND as_dir does not exist
        *  otherwise use as_dir
        */
        if ( stat(as_dir->value, &statbuf ) < 0
             && stat(as_file->value, &statbuf ) > 0
             && ( statbuf.st_mode & S_IFREG ) )
        {
            string_truncate( path, 0 );
            string_append( path, as_file->value );
        }
        else
        {
            string_truncate( path, 0 );
            string_append( path, as_dir->value );
        }
    }
    else if ( file_count )
    {
        string_truncate( path, 0 );
        string_append( path, as_file->value );
    }
    else if ( dir_count  )
    {
        string_truncate( path, 0 );
        string_append( path, as_dir->value );
    }
    else
    {
        /* error: unable to translate path to native format */
        translated = 0;
    }

    string_free( as_dir );
    string_free( as_file );

    return translated;
}


/*
 * path_translate_to_os_()
 */

int path_translate_to_os_( char const * f, string * file )
{
    int translated = 0;

    /* by default, pass on the original path */
    string_copy( file, f );

    translated = translate_path_posix2vms( file );

    return translated;
}


/*
 * path_register_key()
 */

void path_register_key( OBJECT * path )
{
}


/*
 * path_as_key()
 */

OBJECT * path_as_key( OBJECT * path )
{
    return object_copy( path );
}


/*
 * path_done()
 */

void path_done( void )
{
}

#endif

