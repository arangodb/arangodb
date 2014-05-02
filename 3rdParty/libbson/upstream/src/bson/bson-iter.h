/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#if !defined (BSON_INSIDE) && !defined (BSON_COMPILATION)
# error "Only <bson.h> can be included directly."
#endif


#ifndef BSON_ITER_H
#define BSON_ITER_H


#include "bson.h"
#include "bson-endian.h"
#include "bson-macros.h"
#include "bson-types.h"


BSON_BEGIN_DECLS


#define BSON_ITER_HOLDS_DOUBLE(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_DOUBLE)

#define BSON_ITER_HOLDS_UTF8(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_UTF8)

#define BSON_ITER_HOLDS_DOCUMENT(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_DOCUMENT)

#define BSON_ITER_HOLDS_ARRAY(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_ARRAY)

#define BSON_ITER_HOLDS_BINARY(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_BINARY)

#define BSON_ITER_HOLDS_UNDEFINED(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_UNDEFINED)

#define BSON_ITER_HOLDS_OID(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_OID)

#define BSON_ITER_HOLDS_BOOL(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_BOOL)

#define BSON_ITER_HOLDS_DATE_TIME(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_DATE_TIME)

#define BSON_ITER_HOLDS_NULL(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_NULL)

#define BSON_ITER_HOLDS_REGEX(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_REGEX)

#define BSON_ITER_HOLDS_DBPOINTER(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_DBPOINTER)

#define BSON_ITER_HOLDS_CODE(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_CODE)

#define BSON_ITER_HOLDS_SYMBOL(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_SYMBOL)

#define BSON_ITER_HOLDS_CODEWSCOPE(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_CODEWSCOPE)

#define BSON_ITER_HOLDS_INT32(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_INT32)

#define BSON_ITER_HOLDS_TIMESTAMP(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_TIMESTAMP)

#define BSON_ITER_HOLDS_INT64(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_INT64)

#define BSON_ITER_HOLDS_MAXKEY(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_MAXKEY)

#define BSON_ITER_HOLDS_MINKEY(iter) \
   (bson_iter_type ((iter)) == BSON_TYPE_MINKEY)

#define BSON_ITER_IS_KEY(iter, key) \
   (0 == strcmp ((key), bson_iter_key ((iter))))


const bson_value_t *
bson_iter_value (bson_iter_t *iter);


/**
 * bson_iter_utf8_len_unsafe:
 * @iter: a bson_iter_t.
 *
 * Returns the length of a string currently pointed to by @iter. This performs
 * no validation so the is responsible for knowing the BSON is valid. Calling
 * bson_validate() is one way to do this ahead of time.
 */
static BSON_INLINE uint32_t
bson_iter_utf8_len_unsafe (const bson_iter_t *iter)
{
   int32_t val;

   memcpy (&val, iter->raw + iter->d1, 4);
   val = BSON_UINT32_FROM_LE (val);
   return MAX (0, val - 1);
}


void
bson_iter_array (const bson_iter_t   *iter,
                 uint32_t       *array_len,
                 const uint8_t **array);


void
bson_iter_binary (const bson_iter_t   *iter,
                  bson_subtype_t      *subtype,
                  uint32_t       *binary_len,
                  const uint8_t **binary);


const char *
bson_iter_code (const bson_iter_t *iter,
                uint32_t     *length);


/**
 * bson_iter_code_unsafe:
 * @iter: A bson_iter_t.
 * @length: A location for the length of the resulting string.
 *
 * Like bson_iter_code() but performs no integrity checks.
 *
 * Returns: A string that should not be modified or freed.
 */
static BSON_INLINE const char *
bson_iter_code_unsafe (const bson_iter_t *iter,
                       uint32_t     *length)
{
   *length = bson_iter_utf8_len_unsafe (iter);
   return (const char *)(iter->raw + iter->d2);
}


const char *
bson_iter_codewscope (const bson_iter_t   *iter,
                      uint32_t       *length,
                      uint32_t       *scope_len,
                      const uint8_t **scope);


void
bson_iter_dbpointer (const bson_iter_t *iter,
                     uint32_t     *collection_len,
                     const char       **collection,
                     const bson_oid_t **oid);


void
bson_iter_document (const bson_iter_t   *iter,
                    uint32_t       *document_len,
                    const uint8_t **document);


double
bson_iter_double (const bson_iter_t *iter);


/**
 * bson_iter_double_unsafe:
 * @iter: A bson_iter_t.
 *
 * Similar to bson_iter_double() but does not perform an integrity checking.
 *
 * Returns: A double.
 */
static BSON_INLINE double
bson_iter_double_unsafe (const bson_iter_t *iter)
{
   double val;

   memcpy (&val, iter->raw + iter->d1, 8);
   return BSON_DOUBLE_FROM_LE (val);
}


bool
bson_iter_init (bson_iter_t  *iter,
                const bson_t *bson);


bool
bson_iter_init_find (bson_iter_t  *iter,
                     const bson_t *bson,
                     const char   *key);


bool
bson_iter_init_find_case (bson_iter_t  *iter,
                          const bson_t *bson,
                          const char   *key);


int32_t
bson_iter_int32 (const bson_iter_t *iter);


/**
 * bson_iter_int32_unsafe:
 * @iter: A bson_iter_t.
 *
 * Similar to bson_iter_int32() but with no integrity checking.
 *
 * Returns: A 32-bit signed integer.
 */
static BSON_INLINE int32_t
bson_iter_int32_unsafe (const bson_iter_t *iter)
{
   int32_t val;

   memcpy (&val, iter->raw + iter->d1, 4);
   return BSON_UINT32_FROM_LE (val);
}


int64_t
bson_iter_int64 (const bson_iter_t *iter);


int64_t
bson_iter_as_int64 (const bson_iter_t *iter);


/**
 * bson_iter_int64_unsafe:
 * @iter: a bson_iter_t.
 *
 * Similar to bson_iter_int64() but without integrity checking.
 *
 * Returns: A 64-bit signed integer.
 */
static BSON_INLINE int64_t
bson_iter_int64_unsafe (const bson_iter_t *iter)
{
   int64_t val;

   memcpy (&val, iter->raw + iter->d1, 8);
   return BSON_UINT64_FROM_LE (val);
}


bool
bson_iter_find (bson_iter_t *iter,
                const char  *key);


bool
bson_iter_find_case (bson_iter_t *iter,
                     const char  *key);


bool
bson_iter_find_descendant (bson_iter_t *iter,
                           const char  *dotkey,
                           bson_iter_t *descendant);


bool
bson_iter_next (bson_iter_t *iter);


const bson_oid_t *
bson_iter_oid (const bson_iter_t *iter);


/**
 * bson_iter_oid_unsafe:
 * @iter: A #bson_iter_t.
 *
 * Similar to bson_iter_oid() but performs no integrity checks.
 *
 * Returns: A #bson_oid_t that should not be modified or freed.
 */
static BSON_INLINE const bson_oid_t *
bson_iter_oid_unsafe (const bson_iter_t *iter)
{
   return (const bson_oid_t *)(iter->raw + iter->d1);
}


const char *
bson_iter_key (const bson_iter_t *iter);


/**
 * bson_iter_key_unsafe:
 * @iter: A bson_iter_t.
 *
 * Similar to bson_iter_key() but performs no integrity checking.
 *
 * Returns: A string that should not be modified or freed.
 */
static BSON_INLINE const char *
bson_iter_key_unsafe (const bson_iter_t *iter)
{
   return (const char *)(iter->raw + iter->key);
}


const char *
bson_iter_utf8 (const bson_iter_t *iter,
                uint32_t     *length);


/**
 * bson_iter_utf8_unsafe:
 *
 * Similar to bson_iter_utf8() but performs no integrity checking.
 *
 * Returns: A string that should not be modified or freed.
 */
static BSON_INLINE const char *
bson_iter_utf8_unsafe (const bson_iter_t *iter,
                       uint32_t     *length)
{
   *length = bson_iter_utf8_len_unsafe (iter);
   return (const char *)(iter->raw + iter->d2);
}


char *
bson_iter_dup_utf8 (const bson_iter_t *iter,
                    uint32_t     *length);


int64_t
bson_iter_date_time (const bson_iter_t *iter);


time_t
bson_iter_time_t (const bson_iter_t *iter);


/**
 * bson_iter_time_t_unsafe:
 * @iter: A bson_iter_t.
 *
 * Similar to bson_iter_time_t() but performs no integrity checking.
 *
 * Returns: A time_t containing the number of seconds since UNIX epoch
 *          in UTC.
 */
static BSON_INLINE time_t
bson_iter_time_t_unsafe (const bson_iter_t *iter)
{
   return (time_t)(bson_iter_int64_unsafe (iter) / 1000UL);
}


void
bson_iter_timeval (const bson_iter_t *iter,
                   struct timeval    *tv);


/**
 * bson_iter_timeval_unsafe:
 * @iter: A bson_iter_t.
 * @tv: A struct timeval.
 *
 * Similar to bson_iter_timeval() but performs no integrity checking.
 */
static BSON_INLINE void
bson_iter_timeval_unsafe (const bson_iter_t *iter,
                          struct timeval    *tv)
{
#ifdef BSON_OS_WIN32
   tv->tv_sec = (long)bson_iter_int64_unsafe (iter);
#else
   tv->tv_sec = (suseconds_t)bson_iter_int64_unsafe (iter);
#endif
   tv->tv_usec = 0;
}


void
bson_iter_timestamp (const bson_iter_t *iter,
                     uint32_t     *timestamp,
                     uint32_t     *increment);


bool
bson_iter_bool (const bson_iter_t *iter);


/**
 * bson_iter_bool_unsafe:
 * @iter: A bson_iter_t.
 *
 * Similar to bson_iter_bool() but performs no integrity checking.
 *
 * Returns: true or false.
 */
static BSON_INLINE bool
bson_iter_bool_unsafe (const bson_iter_t *iter)
{
   char val;

   memcpy (&val, iter->raw + iter->d1, 1);
   return !!val;
}


bool
bson_iter_as_bool (const bson_iter_t *iter);


const char *
bson_iter_regex (const bson_iter_t *iter,
                 const char       **options);


const char *
bson_iter_symbol (const bson_iter_t *iter,
                  uint32_t     *length);


bson_type_t
bson_iter_type (const bson_iter_t *iter);


/**
 * bson_iter_type_unsafe:
 * @iter: A bson_iter_t.
 *
 * Similar to bson_iter_type() but performs no integrity checking.
 *
 * Returns: A bson_type_t.
 */
static BSON_INLINE bson_type_t
bson_iter_type_unsafe (const bson_iter_t *iter)
{
   return (bson_type_t) (iter->raw + iter->type) [0];
}


bool
bson_iter_recurse (const bson_iter_t *iter,
                   bson_iter_t       *child);


void
bson_iter_overwrite_int32 (bson_iter_t *iter,
                           int32_t value);


void
bson_iter_overwrite_int64 (bson_iter_t *iter,
                           int64_t value);


void
bson_iter_overwrite_double (bson_iter_t *iter,
                            double       value);


void
bson_iter_overwrite_bool (bson_iter_t *iter,
                          bool  value);


bool
bson_iter_visit_all (bson_iter_t          *iter,
                     const bson_visitor_t *visitor,
                     void                 *data);


BSON_END_DECLS


#endif /* BSON_ITER_H */
