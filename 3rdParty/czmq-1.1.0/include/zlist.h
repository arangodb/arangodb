/*  =========================================================================
    zlist - generic type-free list container

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

#ifndef __ZFL_LIST_H_INCLUDED__
#define __ZFL_LIST_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
typedef struct _zlist zlist_t;

//  @interface
//  Create a new list container
zlist_t *
    zlist_new (void);

//  Destroy a list container
void
    zlist_destroy (zlist_t **self_p);

//  Return first item in the list, or null
void *
    zlist_first (zlist_t *self);

//  Return next item in the list, or null
void *
    zlist_next (zlist_t *self);

//  Append an item to the end of the list
void
    zlist_append (zlist_t *self, void *item);

//  Push an item to the start of the list
void
    zlist_push (zlist_t *self, void *item);

//  Pop the item off the start of the list, if any
void *
    zlist_pop (zlist_t *self);

//  Remove the specified item from the list if present
void
    zlist_remove (zlist_t *self, void *item);

//  Copy the entire list, return the copy
zlist_t *
    zlist_copy (zlist_t *self);

//  Return number of items in the list
size_t
    zlist_size (zlist_t *self);

//  Self test of this class
void
    zlist_test (int verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
