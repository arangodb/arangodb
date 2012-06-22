/*  =========================================================================
    zhash - generic type-free hash container

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

/*
@header
    Expandable hash table container
@discuss
    Note that it's relatively slow (~50k insertions/deletes per second), so
    don't do inserts/updates on the critical path for message I/O.  It can
    do ~2.5M lookups per second for 16-char keys.  Timed on a 1.6GHz CPU.
@end
*/

#include "../include/czmq_prelude.h"
#include "../include/zhash.h"

//  Hash table performance parameters

#define INITIAL_SIZE    255     //  Initial size in items
#define LOAD_FACTOR     75      //  Percent loading before splitting
#define GROWTH_FACTOR   200     //  Increase in % after splitting


//  Hash item, used internally only

typedef struct _item_t item_t;
struct _item_t {
    void
        *value;                 //  Opaque item value
    item_t
        *next;                  //  Next item in the hash slot
    qbyte
        index;                  //  Index of item in table
    char
        *key;                   //  Item's original key
    zhash_free_fn
        *free_fn;               //  Value free function if any
};

//  Hash table structure

struct _zhash {
    size_t
        size;                   //  Current size of hash table
    size_t
        limit;                  //  Current hash table limit
    item_t
        **items;                //  Array of items
    uint
        cached_index;           //  Avoids duplicate hash calculations
};


//  --------------------------------------------------------------------------
//  Local helper function
//  Compute hash for key string

static uint
s_item_hash (char *key, size_t limit)
{
    uint
        key_hash = 0;

    //  Torek hashing function
    while (*key) {
        key_hash *= 33;
        key_hash += *key;
        key++;
    }
    key_hash %= limit;
    return key_hash;
}


//  --------------------------------------------------------------------------
//  Local helper function
//  Lookup item in hash table, returns item or NULL

static item_t *
s_item_lookup (zhash_t *self, char *key)
{
    //  Look in bucket list for item by key
    self->cached_index = s_item_hash (key, self->limit);
    item_t *item = self->items [self->cached_index];
    while (item) {
        if (streq (item->key, key))
            break;
        item = item->next;
    }
    return item;
}


//  --------------------------------------------------------------------------
//  Local helper function
//  Insert new item into hash table, returns item
//  If item already existed, returns NULL

static item_t *
s_item_insert (zhash_t *self, char *key, void *value)
{
    //  Check that item does not already exist in hash table
    //  Leaves self->cached_index with calculated hash item
    item_t *item = s_item_lookup (self, key);
    if (item == NULL) {
        item = (item_t *) zmalloc (sizeof (item_t));
        item->value = value;
        item->key = strdup (key);
        item->index = self->cached_index;
        //  Insert into start of bucket list
        item->next = self->items [self->cached_index];
        self->items [self->cached_index] = item;
        self->size++;
    }
    else
        item = NULL;            //  Signal duplicate insertion
    return item;
}


//  --------------------------------------------------------------------------
//  Local helper function
//  Destroy item in hash table, item must exist in table

static void
s_item_destroy (zhash_t *self, item_t *item, Bool hard)
{
    //  Find previous item since it's a singly-linked list
    item_t *cur_item = self->items [item->index];
    item_t **prev_item = &(self->items [item->index]);
    while (cur_item) {
        if (cur_item == item)
            break;
        prev_item = &(cur_item->next);
        cur_item = cur_item->next;
    }
    assert (cur_item);
    *prev_item = item->next;
    self->size--;
    if (hard) {
        if (item->free_fn)
            (item->free_fn) (item->value);
        free (item->key);
        free (item);
    }
}


//  --------------------------------------------------------------------------
//  Hash table constructor

zhash_t *
zhash_new (void)
{
    zhash_t *self = (zhash_t *) zmalloc (sizeof (zhash_t));
    self->limit = INITIAL_SIZE;
    self->items = (item_t **) zmalloc (sizeof (item_t *) * self->limit);
    return self;
}


//  --------------------------------------------------------------------------
//  Hash table destructor

void
zhash_destroy (zhash_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zhash_t *self = *self_p;
        uint index;
        for (index = 0; index < self->limit; index++) {
            //  Destroy all items in this hash bucket
            item_t *cur_item = self->items [index];
            while (cur_item) {
                item_t *next_item = cur_item->next;
                s_item_destroy (self, cur_item, TRUE);
                cur_item = next_item;
            }
        }
        if (self->items)
            free (self->items);

        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Insert item into hash table with specified key and item
//  If key is already present returns -1 and leaves existing item unchanged
//  Returns 0 on success.

int
zhash_insert (zhash_t *self, char *key, void *value)
{
    assert (self);
    assert (key);

    //  If we're exceeding the load factor of the hash table,
    //  resize it according to the growth factor
    if (self->size >= self->limit * LOAD_FACTOR / 100) {
        item_t
            *cur_item,
            *next_item;
        item_t
            **new_items;
        size_t
            new_limit;
        qbyte
            index,
            new_index;

        //  Create new hash table
        new_limit = self->limit * GROWTH_FACTOR / 100;
        new_items = (item_t **) zmalloc (sizeof (item_t *) * new_limit);

        //  Move all items to the new hash table, rehashing to
        //  take into account new hash table limit
        for (index = 0; index != self->limit; index++) {
            cur_item = self->items [index];
            while (cur_item) {
                next_item = cur_item->next;
                new_index = s_item_hash (cur_item->key, new_limit);
                cur_item->index = new_index;
                cur_item->next = new_items [new_index];
                new_items [new_index] = cur_item;
                cur_item = next_item;
            }
        }
        //  Destroy old hash table
        free (self->items);
        self->items = new_items;
        self->limit = new_limit;
    }
    return s_item_insert (self, key, value)? 0: -1;
}


//  --------------------------------------------------------------------------
//  Update item into hash table with specified key and item.
//  If key is already present, destroys old item and inserts new one.
//  Use free_fn method to ensure deallocator is properly called on item.

void
zhash_update (zhash_t *self, char *key, void *value)
{
    assert (self);
    assert (key);
    item_t *item = s_item_lookup (self, key);
    if (item) {
        if (item->free_fn)
            (item->free_fn) (item->value);
        item->value = value;
    }
    else
        zhash_insert (self, key, value);
}


//  --------------------------------------------------------------------------
//  Remove an item specified by key from the hash table. If there was no such
//  item, this function does nothing.

void
zhash_delete (zhash_t *self, char *key)
{
    assert (self);
    assert (key);

    item_t *item = s_item_lookup (self, key);
    if (item)
        s_item_destroy (self, item, TRUE);
}


//  --------------------------------------------------------------------------
//  Look for item in hash table and return its item, or NULL

void *
zhash_lookup (zhash_t *self, char *key)
{
    assert (self);
    assert (key);

    item_t *item = s_item_lookup (self, key);
    if (item)
        return item->value;
    else
        return NULL;
}


//  --------------------------------------------------------------------------
//  Reindexes an item from an old key to a new key. If there was no such
//  item, does nothing. If the new key already exists, deletes old item.

int
zhash_rename (zhash_t *self, char *old_key, char *new_key)
{
    item_t *item = s_item_lookup (self, old_key);
    if (item) {
        s_item_destroy (self, item, FALSE);
        item_t *new_item = s_item_lookup (self, new_key);
        if (new_item == NULL) {
            free (item->key);
            item->key = strdup (new_key);
            item->index = self->cached_index;
            item->next = self->items [self->cached_index];
            self->items [self->cached_index] = item;
            self->size++;
            return 0;
        }
        else
            return -1;
    }
    else
        return -1;
}


//  --------------------------------------------------------------------------
//  Set a free function for the specified hash table item. When the item is
//  destroyed, the free function, if any, is called on that item.
//  Use this when hash items are dynamically allocated, to ensure that
//  you don't have memory leaks. You can pass 'free' or NULL as a free_fn.
//  Returns the item, or NULL if there is no such item.

void *
zhash_freefn (zhash_t *self, char *key, zhash_free_fn *free_fn)
{
    assert (self);
    assert (key);

    item_t *item = s_item_lookup (self, key);
    if (item) {
        item->free_fn = free_fn;
        return item->value;
    }
    else
        return NULL;
}


//  --------------------------------------------------------------------------
//  Return size of hash table

size_t
zhash_size (zhash_t *self)
{
    assert (self);
    return self->size;
}


//  --------------------------------------------------------------------------
//  Apply function to each item in the hash table. Items are iterated in no
//  defined order.  Stops if callback function returns non-zero and returns
//  final return code from callback function (zero = success).

int
zhash_foreach (zhash_t *self, zhash_foreach_fn *callback, void *argument)
{
    assert (self);
    int rc = 0;
    uint index;
    for (index = 0; index != self->limit; index++) {
        item_t *item = self->items [index];
        while (item) {
            //  Invoke callback, passing item properties and argument
            item_t *next = item->next;
            rc = callback (item->key, item->value, argument);
            if (rc)
                break;          //  End if non-zero return code
            item = next;
        }
    }
    return rc;
}


//  --------------------------------------------------------------------------
//  Runs selftest of class
//  TODO: add unit test for free_fn, foreach

void
zhash_test (int verbose)
{
    printf (" * zhash: ");

    //  @selftest
    zhash_t *hash = zhash_new ();
    assert (hash);
    assert (zhash_size (hash) == 0);

    //  Insert some items
    int rc;
    rc = zhash_insert (hash, "DEADBEEF", (void *) 0xDEADBEEF);
    assert (rc == 0);
    rc = zhash_insert (hash, "ABADCAFE", (void *) 0xABADCAFE);
    assert (rc == 0);
    rc = zhash_insert (hash, "C0DEDBAD", (void *) 0xC0DEDBAD);
    assert (rc == 0);
    rc = zhash_insert (hash, "DEADF00D", (void *) 0xDEADF00D);
    assert (rc == 0);
    assert (zhash_size (hash) == 4);

    //  Look for existing items
    void *item;
    item = zhash_lookup (hash, "DEADBEEF");
    assert (item == (void *) 0xDEADBEEF);
    item = zhash_lookup (hash, "ABADCAFE");
    assert (item == (void *) 0xABADCAFE);
    item = zhash_lookup (hash, "C0DEDBAD");
    assert (item == (void *) 0xC0DEDBAD);
    item = zhash_lookup (hash, "DEADF00D");
    assert (item == (void *) 0xDEADF00D);

    //  Look for non-existent items
    item = zhash_lookup (hash, "0xF0000000");
    assert (item == NULL);

    //  Try to insert duplicate items
    rc = zhash_insert (hash, "DEADBEEF", (void *) 0xF0000000);
    assert (rc == -1);
    item = zhash_lookup (hash, "DEADBEEF");
    assert (item == (void *) 0xDEADBEEF);

    //  Rename an item
    rc = zhash_rename (hash, "DEADBEEF", "LIVEBEEF");
    assert (rc == 0);
    rc = zhash_rename (hash, "WHATBEEF", "LIVEBEEF");
    assert (rc == -1);

    //  Delete a item
    zhash_delete (hash, "LIVEBEEF");
    item = zhash_lookup (hash, "LIVEBEEF");
    assert (item == NULL);
    assert (zhash_size (hash) == 3);

    //  Check that the queue is robust against random usage
    struct {
        char name [100];
        Bool exists;
    } testset [200];
    memset (testset, 0, sizeof (testset));
    int testmax = 200, testnbr, iteration;

    srandom ((unsigned) time (NULL));
    for (iteration = 0; iteration < 25000; iteration++) {
        testnbr = randof (testmax);
        if (testset [testnbr].exists) {
            item = zhash_lookup (hash, testset [testnbr].name);
            assert (item);
            zhash_delete (hash, testset [testnbr].name);
            testset [testnbr].exists = FALSE;
        }
        else {
            sprintf (testset [testnbr].name, "%x-%x", rand (), rand ());
            if (zhash_insert (hash, testset [testnbr].name, "") == 0)
                testset [testnbr].exists = TRUE;
        }
    }
    //  Test 10K lookups
    for (iteration = 0; iteration < 10000; iteration++)
        item = zhash_lookup (hash, "DEADBEEFABADCAFE");

    //  Destructor should be safe to call twice
    zhash_destroy (&hash);
    zhash_destroy (&hash);
    assert (hash == NULL);
    //  @end

    printf ("OK\n");
}
