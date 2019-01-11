/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*  This file is ALSO:
 *  Copyright 2001-2004 David Abrahams.
 *  Copyright 2005 Rene Rivera.
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 */

/*
 * fileunix.c - manipulate file names and scan directories on UNIX/AmigaOS
 *
 * External routines:
 *  file_archscan()                 - scan an archive for files
 *  file_mkdir()                    - create a directory
 *  file_supported_fmt_resolution() - file modification timestamp resolution
 *
 * External routines called only via routines in filesys.c:
 *  file_collect_dir_content_() - collects directory content information
 *  file_dirscan_()             - OS specific file_dirscan() implementation
 *  file_query_()               - query information about a path from the OS
 *  file_collect_archive_content_() - collects information about archive members
 *  file_archivescan_()         - OS specific file_archivescan() implementation
 */

#include "jam.h"
#ifdef USE_FILEUNIX
#include "filesys.h"

#include "object.h"
#include "pathsys.h"
#include "strings.h"
#include "output.h"

#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>  /* needed for mkdir() */

#if defined( sun ) || defined( __sun ) || defined( linux )
# include <unistd.h>  /* needed for read and close prototype */
#endif

#if defined( OS_SEQUENT ) || \
    defined( OS_DGUX ) || \
    defined( OS_SCO ) || \
    defined( OS_ISC )
# define PORTAR 1
#endif

#if defined( OS_RHAPSODY ) || defined( OS_MACOSX ) || defined( OS_NEXT )
# include <sys/dir.h>
# include <unistd.h>  /* need unistd for rhapsody's proper lseek */
# define STRUCT_DIRENT struct direct
#else
# include <dirent.h>
# define STRUCT_DIRENT struct dirent
#endif

#ifdef OS_COHERENT
# include <arcoff.h>
# define HAVE_AR
#endif

#if defined( OS_MVS ) || defined( OS_INTERIX )
#define ARMAG  "!<arch>\n"
#define SARMAG  8
#define ARFMAG  "`\n"
#define HAVE_AR

struct ar_hdr  /* archive file member header - printable ascii */
{
    char ar_name[ 16 ];  /* file member name - `/' terminated */
    char ar_date[ 12 ];  /* file member date - decimal */
    char ar_uid[ 6 ];    /* file member user id - decimal */
    char ar_gid[ 6 ];    /* file member group id - decimal */
    char ar_mode[ 8 ];   /* file member mode - octal */
    char ar_size[ 10 ];  /* file member size - decimal */
    char ar_fmag[ 2 ];   /* ARFMAG - string to end header */
};
#endif

#if defined( OS_QNX ) || \
    defined( OS_BEOS ) || \
    defined( OS_HAIKU ) || \
    defined( OS_MPEIX )
# define NO_AR
# define HAVE_AR
#endif

#ifndef HAVE_AR
# ifdef OS_AIX
/* Define these for AIX to get the definitions for both small and big archive
 * file format variants.
 */
#  define __AR_SMALL__
#  define __AR_BIG__
# endif
# include <ar.h>
#endif


/*
 * file_collect_dir_content_() - collects directory content information
 */

int file_collect_dir_content_( file_info_t * const d )
{
    LIST * files = L0;
    PATHNAME f;
    int n;
    STRUCT_DIRENT ** namelist;
    STRUCT_DIRENT * dirent;
    string path[ 1 ];
    char const * dirstr;

    assert( d );
    assert( d->is_dir );
    assert( list_empty( d->files ) );

    dirstr = object_str( d->name );

    memset( (char *)&f, '\0', sizeof( f ) );
    f.f_dir.ptr = dirstr;
    f.f_dir.len = strlen( dirstr );

    if ( !*dirstr ) dirstr = ".";

    if ( -1 == ( n = scandir( dirstr, &namelist, NULL, alphasort ) ) )
        return -1;

    string_new( path );
    while ( n-- )
    {
        OBJECT * name;
        dirent = namelist[ n ];
        f.f_base.ptr = dirent->d_name
        #ifdef old_sinix
            - 2  /* Broken structure definition on sinix. */
        #endif
            ;
        f.f_base.len = strlen( f.f_base.ptr );

        string_truncate( path, 0 );
        path_build( &f, path );
        name = object_new( path->value );
        /* Immediately stat the file to preserve invariants. */
        if ( file_query( name ) )
            files = list_push_back( files, name );
        else
            object_free( name );
        free( dirent );
    }
    string_free( path );

    free( namelist );

    d->files = files;
    return 0;
}


/*
 * file_dirscan_() - OS specific file_dirscan() implementation
 */

void file_dirscan_( file_info_t * const d, scanback func, void * closure )
{
    assert( d );
    assert( d->is_dir );

    /* Special case / : enter it */
    if ( !strcmp( object_str( d->name ), "/" ) )
        (*func)( closure, d->name, 1 /* stat()'ed */, &d->time );
}


/*
 * file_mkdir() - create a directory
 */

int file_mkdir( char const * const path )
{
    /* Explicit cast to remove const modifiers and avoid related compiler
     * warnings displayed when using the intel compiler.
     */
    return mkdir( (char *)path, 0777 );
}


/*
 * file_query_() - query information about a path from the OS
 */

void file_query_( file_info_t * const info )
{
    file_query_posix_( info );
}


int file_collect_archive_content_( file_archive_info_t * const archive );

/*
 * file_archscan() - scan an archive for files
 */
void file_archscan( char const * arch, scanback func, void * closure )
{
    OBJECT * path = object_new( arch );
    file_archive_info_t * archive = file_archive_query( path );

    object_free( path );

    if ( filelist_empty( archive->members ) )
    {
        if ( file_collect_archive_content_( archive ) < 0 )
            return;
    }

    /* Report the collected archive content. */
    {
        FILELISTITER iter = filelist_begin( archive->members );
        FILELISTITER const end = filelist_end( archive->members );
        char buf[ MAXJPATH ];

        for ( ; iter != end ; iter = filelist_next( iter ) )
        {
            file_info_t * member_file = filelist_item( iter );
            LIST * symbols = member_file->files;

            /* Construct member path: 'archive-path(member-name)'
             */
            sprintf( buf, "%s(%s)",
                object_str( archive->file->name ),
                object_str( member_file->name ) );
            {
                OBJECT * const member = object_new( buf );
                (*func)( closure, member, 1 /* time valid */, &member_file->time );
                object_free( member );
            }
        }
    }
}


/*
 *  file_archivescan_()         - OS specific file_archivescan() implementation
 */

void file_archivescan_( file_archive_info_t * const archive, archive_scanback func,
                        void * closure )
{
}


/*
 *  file_collect_archive_content_() - collects information about archive members
 */

#ifndef AIAMAG  /* God-fearing UNIX */

#define SARFMAG  2
#define SARHDR  sizeof( struct ar_hdr )

int file_collect_archive_content_( file_archive_info_t * const archive )
{
#ifndef NO_AR
    struct ar_hdr ar_hdr;
    char * string_table = 0;
    char buf[ MAXJPATH ];
    long offset;
    int fd;
    const char * path = object_str( archive->file->name );

    if ( ! filelist_empty( archive->members ) ) filelist_free( archive->members );

    if ( ( fd = open( path, O_RDONLY, 0 ) ) < 0 )
        return -1;

    if ( read( fd, buf, SARMAG ) != SARMAG ||
        strncmp( ARMAG, buf, SARMAG ) )
    {
        close( fd );
        return -1;
    }

    offset = SARMAG;

    if ( DEBUG_BINDSCAN )
        out_printf( "scan archive %s\n", path );

    while ( ( read( fd, &ar_hdr, SARHDR ) == SARHDR ) &&
        !( memcmp( ar_hdr.ar_fmag, ARFMAG, SARFMAG )
#ifdef ARFZMAG
            /* OSF also has a compressed format */
            && memcmp( ar_hdr.ar_fmag, ARFZMAG, SARFMAG )
#endif
        ) )
    {
        char   lar_name_[ 257 ];
        char * lar_name = lar_name_ + 1;
        long   lar_date;
        long   lar_size;
        long   lar_offset;
        char * c;
        char * src;
        char * dest;

        strncpy( lar_name, ar_hdr.ar_name, sizeof( ar_hdr.ar_name ) );

        sscanf( ar_hdr.ar_date, "%ld", &lar_date );
        sscanf( ar_hdr.ar_size, "%ld", &lar_size );

        if ( ar_hdr.ar_name[ 0 ] == '/' )
        {
            if ( ar_hdr.ar_name[ 1 ] == '/' )
            {
                /* This is the "string table" entry of the symbol table, holding
                 * filename strings longer than 15 characters, i.e. those that
                 * do not fit into ar_name.
                 */
                string_table = (char *)BJAM_MALLOC_ATOMIC( lar_size );
                lseek( fd, offset + SARHDR, 0 );
                if ( read( fd, string_table, lar_size ) != lar_size )
                    out_printf("error reading string table\n");
            }
            else if ( string_table && ar_hdr.ar_name[ 1 ] != ' ' )
            {
                /* Long filenames are recognized by "/nnnn" where nnnn is the
                 * offset of the string in the string table represented in ASCII
                 * decimals.
                 */
                dest = lar_name;
                lar_offset = atoi( lar_name + 1 );
                src = &string_table[ lar_offset ];
                while ( *src != '/' )
                    *dest++ = *src++;
                *dest = '/';
            }
        }

        c = lar_name - 1;
        while ( ( *++c != ' ' ) && ( *c != '/' ) );
        *c = '\0';

        if ( DEBUG_BINDSCAN )
            out_printf( "archive name %s found\n", lar_name );

        sprintf( buf, "%s", lar_name );

        if ( strcmp( buf, "") != 0 )
        {
            file_info_t * member = 0;

            archive->members = filelist_push_back( archive->members, object_new( buf ) );
            member = filelist_back( archive->members );
            member->is_file = 1;
            member->is_dir = 0;
            member->exists = 0;
            timestamp_init( &member->time, (time_t)lar_date, 0 );
        }

        offset += SARHDR + ( ( lar_size + 1 ) & ~1 );
        lseek( fd, offset, 0 );
    }

    if ( string_table )
        BJAM_FREE( string_table );

    close( fd );
#endif  /* NO_AR */

    return 0;
}

#else  /* AIAMAG - RS6000 AIX */

static void collect_archive_content_small( int fd, file_archive_info_t * const archive )
{
    struct fl_hdr fl_hdr;

    struct {
        struct ar_hdr hdr;
        char pad[ 256 ];
    } ar_hdr ;

    char buf[ MAXJPATH ];
    long offset;
    const char * path = object_str( archive->file->name );

    if ( read( fd, (char *)&fl_hdr, FL_HSZ ) != FL_HSZ )
        return;

    sscanf( fl_hdr.fl_fstmoff, "%ld", &offset );

    if ( DEBUG_BINDSCAN )
        out_printf( "scan archive %s\n", path );

    while ( offset > 0 && lseek( fd, offset, 0 ) >= 0 &&
        read( fd, &ar_hdr, sizeof( ar_hdr ) ) >= (int)sizeof( ar_hdr.hdr ) )
    {
        long lar_date;
        int lar_namlen;

        sscanf( ar_hdr.hdr.ar_namlen, "%d" , &lar_namlen );
        sscanf( ar_hdr.hdr.ar_date  , "%ld", &lar_date   );
        sscanf( ar_hdr.hdr.ar_nxtmem, "%ld", &offset     );

        if ( !lar_namlen )
            continue;

        ar_hdr.hdr._ar_name.ar_name[ lar_namlen ] = '\0';

        sprintf( buf, "%s", ar_hdr.hdr._ar_name.ar_name );

        if ( strcmp( buf, "") != 0 )
        {
            file_info_t * member = 0;

            archive->members = filelist_push_back( archive->members, object_new( buf ) );
            member = filelist_back( archive->members );
            member->is_file = 1;
            member->is_dir = 0;
            member->exists = 0;
            timestamp_init( &member->time, (time_t)lar_date, 0 );
        }
    }
}

/* Check for OS versions supporting the big variant. */
#ifdef AR_HSZ_BIG

static void collect_archive_content_big( int fd, file_archive_info_t * const archive )
{
    struct fl_hdr_big fl_hdr;

    struct {
        struct ar_hdr_big hdr;
        char pad[ 256 ];
    } ar_hdr ;

    char buf[ MAXJPATH ];
    long long offset;
    const char * path = object_str( archive->file->name );

    if ( read( fd, (char *)&fl_hdr, FL_HSZ_BIG ) != FL_HSZ_BIG )
        return;

    sscanf( fl_hdr.fl_fstmoff, "%lld", &offset );

    if ( DEBUG_BINDSCAN )
        out_printf( "scan archive %s\n", path );

    while ( offset > 0 && lseek( fd, offset, 0 ) >= 0 &&
        read( fd, &ar_hdr, sizeof( ar_hdr ) ) >= sizeof( ar_hdr.hdr ) )
    {
        long lar_date;
        int lar_namlen;

        sscanf( ar_hdr.hdr.ar_namlen, "%d"  , &lar_namlen );
        sscanf( ar_hdr.hdr.ar_date  , "%ld" , &lar_date   );
        sscanf( ar_hdr.hdr.ar_nxtmem, "%lld", &offset     );

        if ( !lar_namlen )
            continue;

        ar_hdr.hdr._ar_name.ar_name[ lar_namlen ] = '\0';

        sprintf( buf, "%s", ar_hdr.hdr._ar_name.ar_name );

        if ( strcmp( buf, "") != 0 )
        {
            file_info_t * member = 0;

            archive->members = filelist_push_back( archive->members, object_new( buf ) );
            member = filelist_back( archive->members );
            member->is_file = 1;
            member->is_dir = 0;
            member->exists = 0;
            timestamp_init( &member->time, (time_t)lar_date, 0 );
        }
    }
}

#endif  /* AR_HSZ_BIG */

int file_collect_archive_content_( file_archive_info_t * const archive )
{
    int fd;
    char fl_magic[ SAIAMAG ];
    const char * path = object_str( archive->file->name );

    if ( ! filelist_empty( archive->members ) ) filelist_free( archive->members );

    if ( ( fd = open( path, O_RDONLY, 0 ) ) < 0 )
        return -1;

    if ( read( fd, fl_magic, SAIAMAG ) != SAIAMAG ||
        lseek( fd, 0, SEEK_SET ) == -1 )
    {
        close( fd );
        return -1;
    }

    if ( !strncmp( AIAMAG, fl_magic, SAIAMAG ) )
    {
        /* read small variant */
        collect_archive_content_small( fd, archive );
    }
#ifdef AR_HSZ_BIG
    else if ( !strncmp( AIAMAGBIG, fl_magic, SAIAMAG ) )
    {
        /* read big variant */
        collect_archive_content_big( fd, archive );
    }
#endif

    close( fd );

    return 0;
}

#endif  /* AIAMAG - RS6000 AIX */

#endif  /* USE_FILEUNIX */
