/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*  This file is ALSO:
 *  Copyright 2001-2004 David Abrahams.
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 */

/*
 * filesys.h - OS specific file routines
 */

#ifndef FILESYS_DWA20011025_H
#define FILESYS_DWA20011025_H

#include "hash.h"
#include "lists.h"
#include "object.h"
#include "pathsys.h"
#include "timestamp.h"


typedef struct file_info_t
{
    OBJECT * name;
    char is_file;
    char is_dir;
    char exists;
    timestamp time;
    LIST * files;
} file_info_t;

typedef struct file_item FILEITEM;
struct file_item
{
    file_info_t * value;  /* expected to be equvalent with &FILEITEM */
    FILEITEM * next;
};

typedef struct file_list
{
    FILEITEM * head;
    FILEITEM * tail;
    int size;
} FILELIST;

typedef file_info_t * * FILELISTITER;  /*  also &FILEITEM equivalent */


typedef struct file_archive_info_t
{
    file_info_t * file;
    FILELIST * members;
} file_archive_info_t;


typedef void (*archive_scanback)( void * closure, OBJECT * path, LIST * symbols,
    int found, timestamp const * const );
typedef void (*scanback)( void * closure, OBJECT * path, int found,
    timestamp const * const );


void file_archscan( char const * arch, scanback func, void * closure );
void file_archivescan( OBJECT * path, archive_scanback func, void * closure );
void file_build1( PATHNAME * const f, string * file ) ;
void file_dirscan( OBJECT * dir, scanback func, void * closure );
file_info_t * file_info( OBJECT * const path, int * found );
int file_is_file( OBJECT * const path );
int file_mkdir( char const * const path );
file_info_t * file_query( OBJECT * const path );
void file_remove_atexit( OBJECT * const path );
void file_supported_fmt_resolution( timestamp * const );
int file_time( OBJECT * const path, timestamp * const );


/*  Archive/library file support */
file_archive_info_t * file_archive_info( OBJECT * const path, int * found );
file_archive_info_t * file_archive_query( OBJECT * const path );

/* FILELIST linked-list */
FILELIST * filelist_new( OBJECT * path );
FILELIST * filelist_push_back( FILELIST * list, OBJECT * path );
FILELIST * filelist_push_front( FILELIST * list, OBJECT * path );
FILELIST * filelist_pop_front( FILELIST * list );
int filelist_length( FILELIST * list );
void filelist_free( FILELIST * list );

FILELISTITER filelist_begin( FILELIST * list );
FILELISTITER filelist_end( FILELIST * list );
FILELISTITER filelist_next( FILELISTITER it );
file_info_t * filelist_item( FILELISTITER it );
file_info_t * filelist_front(  FILELIST * list );
file_info_t * filelist_back(  FILELIST * list );

#define FL0 ((FILELIST *)0)


/* Internal utility worker functions. */
void file_query_posix_( file_info_t * const );

void file_done();

#endif
