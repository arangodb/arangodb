/*
 *  Copyright 2001-2004 David Abrahams.
 *  Copyright 2005 Rene Rivera.
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 */

/*
 * filesys.c - OS independent file system manipulation support
 *
 * External routines:
 *  file_build1()        - construct a path string based on PATHNAME information
 *  file_dirscan()       - scan a directory for files
 *  file_done()          - module cleanup called on shutdown
 *  file_info()          - return cached information about a path
 *  file_is_file()       - return whether a path identifies an existing file
 *  file_query()         - get cached information about a path, query the OS if
 *                         needed
 *  file_remove_atexit() - schedule a path to be removed on program exit
 *  file_time()          - get a file timestamp
 *
 * External routines - utilities for OS specific module implementations:
 *  file_query_posix_()  - query information about a path using POSIX stat()
 *
 * Internal routines:
 *  file_dirscan_impl()  - no-profiling worker for file_dirscan()
 */


#include "jam.h"
#include "filesys.h"

#include "lists.h"
#include "object.h"
#include "pathsys.h"
#include "strings.h"
#include "output.h"

#include <assert.h>
#include <sys/stat.h>


/* Internal OS specific implementation details - have names ending with an
 * underscore and are expected to be implemented in an OS specific fileXXX.c
 * module.
 */
void file_dirscan_( file_info_t * const dir, scanback func, void * closure );
int file_collect_dir_content_( file_info_t * const dir );
void file_query_( file_info_t * const );

void file_archivescan_( file_archive_info_t * const archive, archive_scanback func,
                        void * closure );
int file_collect_archive_content_( file_archive_info_t * const archive );
void file_archive_query_( file_archive_info_t * const );

static void file_archivescan_impl( OBJECT * path, archive_scanback func,
                                   void * closure );
static void file_dirscan_impl( OBJECT * dir, scanback func, void * closure );
static void free_file_archive_info( void * xarchive, void * data );
static void free_file_info( void * xfile, void * data );

static void remove_files_atexit( void );


static struct hash * filecache_hash;
static struct hash * archivecache_hash;


/*
 * file_archive_info() - return cached information about an archive
 *
 * Returns a default initialized structure containing only queried file's info
 * in case this is the first time this file system entity has been
 * referenced.
 */

file_archive_info_t * file_archive_info( OBJECT * const path, int * found )
{
    OBJECT * const path_key = path_as_key( path );
    file_archive_info_t * archive;

    if ( !archivecache_hash )
        archivecache_hash = hashinit( sizeof( file_archive_info_t ),
            "file_archive_info" );

    archive = (file_archive_info_t *)hash_insert( archivecache_hash, path_key,
            found );

    if ( !*found )
    {
        archive->name = path_key;
        archive->file = 0;
        archive->members = FL0;
    }
    else
        object_free( path_key );

    return archive;
}


/*
 * file_archive_query() - get cached information about a archive file path
 *
 * Returns 0 in case querying the OS about the given path fails, e.g. because
 * the path does not reference an existing file system object.
 */

file_archive_info_t * file_archive_query( OBJECT * const path )
{
    int found;
    file_archive_info_t * const archive = file_archive_info( path, &found );
    file_info_t * file = file_query( path );

    if ( !( file && file->is_file ) )
    {
        return 0;
    }

    archive->file = file;


    return archive;
}



/*
 * file_archivescan() - scan an archive for members
 */

void file_archivescan( OBJECT * path, archive_scanback func, void * closure )
{
    PROFILE_ENTER( FILE_ARCHIVESCAN );
    file_archivescan_impl( path, func, closure );
    PROFILE_EXIT( FILE_ARCHIVESCAN );
}


/*
 * file_build1() - construct a path string based on PATHNAME information
 */

void file_build1( PATHNAME * const f, string * file )
{
    if ( DEBUG_SEARCH )
    {
        out_printf( "build file: " );
        if ( f->f_root.len )
            out_printf( "root = '%.*s' ", f->f_root.len, f->f_root.ptr );
        if ( f->f_dir.len )
            out_printf( "dir = '%.*s' ", f->f_dir.len, f->f_dir.ptr );
        if ( f->f_base.len )
            out_printf( "base = '%.*s' ", f->f_base.len, f->f_base.ptr );
        out_printf( "\n" );
    }

    /* Start with the grist. If the current grist is not surrounded by <>'s, add
     * them.
     */
    if ( f->f_grist.len )
    {
        if ( f->f_grist.ptr[ 0 ] != '<' )
            string_push_back( file, '<' );
        string_append_range(
            file, f->f_grist.ptr, f->f_grist.ptr + f->f_grist.len );
        if ( file->value[ file->size - 1 ] != '>' )
            string_push_back( file, '>' );
    }
}


/*
 * file_dirscan() - scan a directory for files
 */

void file_dirscan( OBJECT * dir, scanback func, void * closure )
{
    PROFILE_ENTER( FILE_DIRSCAN );
    file_dirscan_impl( dir, func, closure );
    PROFILE_EXIT( FILE_DIRSCAN );
}


/*
 * file_done() - module cleanup called on shutdown
 */

void file_done()
{
    remove_files_atexit();
    if ( filecache_hash )
    {
        hashenumerate( filecache_hash, free_file_info, (void *)0 );
        hashdone( filecache_hash );
    }

    if ( archivecache_hash )
    {
        hashenumerate( archivecache_hash, free_file_archive_info, (void *)0 );
        hashdone( archivecache_hash );
    }
}


/*
 * file_info() - return cached information about a path
 *
 * Returns a default initialized structure containing only the path's normalized
 * name in case this is the first time this file system entity has been
 * referenced.
 */

file_info_t * file_info( OBJECT * const path, int * found )
{
    OBJECT * const path_key = path_as_key( path );
    file_info_t * finfo;

    if ( !filecache_hash )
        filecache_hash = hashinit( sizeof( file_info_t ), "file_info" );

    finfo = (file_info_t *)hash_insert( filecache_hash, path_key, found );
    if ( !*found )
    {
        finfo->name = path_key;
        finfo->files = L0;
    }
    else
        object_free( path_key );

    return finfo;
}


/*
 * file_is_file() - return whether a path identifies an existing file
 */

int file_is_file( OBJECT * const path )
{
    file_info_t const * const ff = file_query( path );
    return ff ? ff->is_file : -1;
}


/*
 * file_time() - get a file timestamp
 */

int file_time( OBJECT * const path, timestamp * const time )
{
    file_info_t const * const ff = file_query( path );
    if ( !ff ) return -1;
    timestamp_copy( time, &ff->time );
    return 0;
}


/*
 * file_query() - get cached information about a path, query the OS if needed
 *
 * Returns 0 in case querying the OS about the given path fails, e.g. because
 * the path does not reference an existing file system object.
 */

file_info_t * file_query( OBJECT * const path )
{
    /* FIXME: Add tracking for disappearing files (i.e. those that can not be
     * detected by stat() even though they had been detected successfully
     * before) and see how they should be handled in the rest of Boost Jam code.
     * Possibly allow Jamfiles to specify some files as 'volatile' which would
     * make Boost Jam avoid caching information about those files and instead
     * ask the OS about them every time.
     */
    int found;
    file_info_t * const ff = file_info( path, &found );
    if ( !found )
    {
        file_query_( ff );
        if ( ff->exists )
        {
            /* Set the path's timestamp to 1 in case it is 0 or undetected to avoid
             * confusion with non-existing paths.
             */
            if ( timestamp_empty( &ff->time ) )
                timestamp_init( &ff->time, 1, 0 );
        }
    }
    if ( !ff->exists )
    {
        return 0;
    }
    return ff;
}

#ifndef OS_NT

/*
 * file_query_posix_() - query information about a path using POSIX stat()
 *
 * Fallback file_query_() implementation for OS specific modules.
 *
 * Note that the Windows POSIX stat() function implementation suffers from
 * several issues:
 *   * Does not support file timestamps with resolution finer than 1 second,
 *     meaning it can not be used to detect file timestamp changes of less than
 *     1 second. One possible consequence is that some fast-paced touch commands
 *     (such as those done by Boost Build's internal testing system if it does
 *     not do some extra waiting) will not be detected correctly by the build
 *     system.
 *   * Returns file modification times automatically adjusted for daylight
 *     savings time even though daylight savings time should have nothing to do
 *     with internal time representation.
 */

void file_query_posix_( file_info_t * const info )
{
    struct stat statbuf;
    char const * const pathstr = object_str( info->name );
    char const * const pathspec = *pathstr ? pathstr : ".";

    if ( stat( pathspec, &statbuf ) < 0 )
    {
        info->is_file = 0;
        info->is_dir = 0;
        info->exists = 0;
        timestamp_clear( &info->time );
    }
    else
    {
        info->is_file = statbuf.st_mode & S_IFREG ? 1 : 0;
        info->is_dir = statbuf.st_mode & S_IFDIR ? 1 : 0;
        info->exists = 1;
#if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809
#if defined(OS_MACOSX)
        timestamp_init( &info->time, statbuf.st_mtimespec.tv_sec, statbuf.st_mtimespec.tv_nsec );
#else
        timestamp_init( &info->time, statbuf.st_mtim.tv_sec, statbuf.st_mtim.tv_nsec );
#endif
#else
        timestamp_init( &info->time, statbuf.st_mtime, 0 );
#endif
    }
}

/*
 * file_supported_fmt_resolution() - file modification timestamp resolution
 *
 * Returns the minimum file modification timestamp resolution supported by this
 * Boost Jam implementation. File modification timestamp changes of less than
 * the returned value might not be recognized.
 *
 * Does not take into consideration any OS or file system related restrictions.
 *
 * Return value 0 indicates that any value supported by the OS is also supported
 * here.
 */

void file_supported_fmt_resolution( timestamp * const t )
{
#if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809
    timestamp_init( t, 0, 1 );
#else
    /* The current implementation does not support file modification timestamp
     * resolution of less than one second.
     */
    timestamp_init( t, 1, 0 );
#endif
}

#endif


/*
 * file_remove_atexit() - schedule a path to be removed on program exit
 */

static LIST * files_to_remove = L0;

void file_remove_atexit( OBJECT * const path )
{
    files_to_remove = list_push_back( files_to_remove, object_copy( path ) );
}


/*
 * file_archivescan_impl() - no-profiling worker for file_archivescan()
 */

static void file_archivescan_impl( OBJECT * path, archive_scanback func, void * closure )
{
    file_archive_info_t * const archive = file_archive_query( path );
    if ( !archive || !archive->file->is_file )
        return;

    /* Lazy collect the archive content information. */
    if ( filelist_empty( archive->members ) )
    {
        if ( DEBUG_BINDSCAN )
            printf( "scan archive %s\n", object_str( archive->file->name ) );
        if ( file_collect_archive_content_( archive ) < 0 )
            return;
    }

    /* OS specific part of the file_archivescan operation. */
    file_archivescan_( archive, func, closure );

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
                (*func)( closure, member, symbols, 1, &member_file->time );
                object_free( member );
            }
        }
    }
}


/*
 * file_dirscan_impl() - no-profiling worker for file_dirscan()
 */

static void file_dirscan_impl( OBJECT * dir, scanback func, void * closure )
{
    file_info_t * const d = file_query( dir );
    if ( !d || !d->is_dir )
        return;

    /* Lazy collect the directory content information. */
    if ( list_empty( d->files ) )
    {
        if ( DEBUG_BINDSCAN )
            out_printf( "scan directory %s\n", object_str( d->name ) );
        if ( file_collect_dir_content_( d ) < 0 )
            return;
    }

    /* OS specific part of the file_dirscan operation. */
    file_dirscan_( d, func, closure );

    /* Report the collected directory content. */
    {
        LISTITER iter = list_begin( d->files );
        LISTITER const end = list_end( d->files );
        for ( ; iter != end; iter = list_next( iter ) )
        {
            OBJECT * const path = list_item( iter );
            file_info_t const * const ffq = file_query( path );
            /* Using a file name read from a file_info_t structure allows OS
             * specific implementations to store some kind of a normalized file
             * name there. Using such a normalized file name then allows us to
             * correctly recognize different file paths actually identifying the
             * same file. For instance, an implementation may:
             *  - convert all file names internally to lower case on a case
             *    insensitive file system
             *  - convert the NTFS paths to their long path variants as that
             *    file system each file system entity may have a long and a
             *    short path variant thus allowing for many different path
             *    strings identifying the same file.
             */
            (*func)( closure, ffq->name, 1 /* stat()'ed */, &ffq->time );
        }
    }
}


static void free_file_archive_info( void * xarchive, void * data )
{
    file_archive_info_t * const archive = (file_archive_info_t *)xarchive;

    if ( archive ) filelist_free( archive->members );
}


static void free_file_info( void * xfile, void * data )
{
    file_info_t * const file = (file_info_t *)xfile;
    object_free( file->name );
    list_free( file->files );
}


static void remove_files_atexit( void )
{
    LISTITER iter = list_begin( files_to_remove );
    LISTITER const end = list_end( files_to_remove );
    for ( ; iter != end; iter = list_next( iter ) )
        remove( object_str( list_item( iter ) ) );
    list_free( files_to_remove );
    files_to_remove = L0;
}


/*
 * FILELIST linked-list implementation
 */

FILELIST * filelist_new( OBJECT * path )
{
    FILELIST * list = (FILELIST *)BJAM_MALLOC( sizeof( FILELIST ) );

    memset( list, 0, sizeof( *list ) );
    list->size = 0;
    list->head = 0;
    list->tail = 0;

    return filelist_push_back( list, path );
}

FILELIST * filelist_push_back( FILELIST * list, OBJECT * path )
{
    FILEITEM * item;
    file_info_t * file;

    /* Lazy initialization
     */
    if ( filelist_empty( list ) )
    {
        list = filelist_new( path );
        return list;
    }


    item = (FILEITEM *)BJAM_MALLOC( sizeof( FILEITEM ) );
    memset( item, 0, sizeof( *item ) );
    item->value = (file_info_t *)BJAM_MALLOC( sizeof( file_info_t ) );

    file = item->value;
    memset( file, 0, sizeof( *file ) );

    file->name = path;
    file->files = L0;

    if ( list->tail )
    {
        list->tail->next = item;
    }
    else
    {
        list->head = item;
    }
    list->tail = item;
    list->size++;

    return list;
}

FILELIST * filelist_push_front( FILELIST * list, OBJECT * path )
{
    FILEITEM * item;
    file_info_t * file;

    /* Lazy initialization
     */
    if ( filelist_empty( list ) )
    {
        list = filelist_new( path );
        return list;
    }


    item = (FILEITEM *)BJAM_MALLOC( sizeof( FILEITEM ) );
    memset( item, 0, sizeof( *item ) );
    item->value = (file_info_t *)BJAM_MALLOC( sizeof( file_info_t ) );

    file = item->value;
    memset( file, 0, sizeof( *file ) );

    file->name = path;
    file->files = L0;

    if ( list->head )
    {
        item->next = list->head;
    }
    else
    {
        list->tail = item;
    }
    list->head = item;
    list->size++;

    return list;
}


FILELIST * filelist_pop_front( FILELIST * list )
{
    FILEITEM * item;

    if ( filelist_empty( list ) ) return list;

    item = list->head;

    if ( item )
    {
        if ( item->value ) free_file_info( item->value, 0 );

        list->head = item->next;
        list->size--;
        if ( !list->size ) list->tail = list->head;

#ifdef BJAM_NO_MEM_CACHE
        BJAM_FREE( item );
#endif
    }

    return list;
}

int filelist_length( FILELIST * list )
{
    int result = 0;
    if ( !filelist_empty( list ) ) result = list->size;

    return result;
}

void filelist_free( FILELIST * list )
{
    FILELISTITER iter;

    if ( filelist_empty( list ) ) return;

    while ( filelist_length( list ) ) filelist_pop_front( list );

#ifdef BJAM_NO_MEM_CACHE
    BJAM_FREE( list );
#endif
}

int filelist_empty( FILELIST * list )
{
    return ( list == FL0 );
}


FILELISTITER filelist_begin( FILELIST * list )
{
    if ( filelist_empty( list )
         || list->head == 0 ) return (FILELISTITER)0;

    return &list->head->value;
}


FILELISTITER filelist_end( FILELIST * list )
{
    return (FILELISTITER)0;
}


FILELISTITER filelist_next( FILELISTITER iter )
{
    if ( iter )
    {
        /*  Given FILEITEM.value is defined as first member of FILEITEM structure
         *  and FILELISTITER = &FILEITEM.value,
         *  FILEITEM = *(FILEITEM **)FILELISTITER
         */
        FILEITEM * item = (FILEITEM *)iter;
        iter = ( item->next ? &item->next->value : (FILELISTITER)0 );
    }

    return iter;
}


file_info_t * filelist_item( FILELISTITER it )
{
    file_info_t * result = (file_info_t *)0;

    if ( it )
    {
        result = (file_info_t *)*it;
    }

    return result;
}


file_info_t * filelist_front(  FILELIST * list )
{
    if ( filelist_empty( list )
         || list->head == 0 ) return (file_info_t *)0;

    return list->head->value;
}


file_info_t * filelist_back(  FILELIST * list )
{
    if ( filelist_empty( list )
         || list->tail == 0 ) return (file_info_t *)0;

    return list->tail->value;
}
