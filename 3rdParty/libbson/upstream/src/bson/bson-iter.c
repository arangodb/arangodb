/*
 * Copyright 2013-2014 MongoDB, Inc.
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


#include "bson-iter.h"


#define ITER_TYPE(i) ((bson_type_t) *((i)->raw + (i)->type))


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_init --
 *
 *       Initializes @iter to be used to iterate @bson.
 *
 * Returns:
 *       true if bson_iter_t was initialized. otherwise false.
 *
 * Side effects:
 *       @iter is initialized.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_init (bson_iter_t  *iter, /* OUT */
                const bson_t *bson) /* IN */
{
   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (bson, false);

   if (BSON_UNLIKELY (bson->len < 5)) {
      return false;
   }

   iter->raw = bson_get_data (bson);
   iter->len = bson->len;
   iter->off = 0;
   iter->type = 0;
   iter->key = 0;
   iter->d1 = 0;
   iter->d2 = 0;
   iter->d3 = 0;
   iter->d4 = 0;
   iter->next_off = 4;
   iter->err_off = 0;

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_recurse --
 *
 *       Creates a new sub-iter looking at the document or array that @iter
 *       is currently pointing at.
 *
 * Returns:
 *       true if successful and @child was initialized.
 *
 * Side effects:
 *       @child is initialized.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_recurse (const bson_iter_t *iter,  /* IN */
                   bson_iter_t       *child) /* OUT */
{
   const uint8_t *data = NULL;
   uint32_t len = 0;

   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (child, false);

   if (ITER_TYPE (iter) == BSON_TYPE_DOCUMENT) {
      bson_iter_document (iter, &len, &data);
   } else if (ITER_TYPE (iter) == BSON_TYPE_ARRAY) {
      bson_iter_array (iter, &len, &data);
   } else {
      return false;
   }

   child->raw = data;
   child->len = len;
   child->off = 0;
   child->type = 0;
   child->key = 0;
   child->d1 = 0;
   child->d2 = 0;
   child->d3 = 0;
   child->d4 = 0;
   child->next_off = 4;
   child->err_off = 0;

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_init_find --
 *
 *       Initializes a #bson_iter_t and moves the iter to the first field
 *       matching @key.
 *
 * Returns:
 *       true if the field named @key was found; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_init_find (bson_iter_t  *iter, /* INOUT */
                     const bson_t *bson, /* IN */
                     const char   *key)  /* IN */
{
   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (bson, false);
   bson_return_val_if_fail (key, false);

   return bson_iter_init (iter, bson) && bson_iter_find (iter, key);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_init_find_case --
 *
 *       A case-insensitive version of bson_iter_init_find().
 *
 * Returns:
 *       true if the field was found and @iter is observing that field.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_init_find_case (bson_iter_t  *iter, /* INOUT */
                          const bson_t *bson, /* IN */
                          const char   *key)  /* IN */
{
   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (bson, false);
   bson_return_val_if_fail (key, false);

   return bson_iter_init (iter, bson) && bson_iter_find_case (iter, key);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_iter_find_with_len --
 *
 *       Internal helper for finding an exact key.
 *
 * Returns:
 *       true if the field @key was found.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_iter_find_with_len (bson_iter_t *iter,   /* INOUT */
                          const char  *key,    /* IN */
                          int          keylen) /* IN */
{
   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (key, false);

   if (keylen < 0) {
      keylen = (int)strlen (key);
   }

   while (bson_iter_next (iter)) {
      if (!strncmp (key, bson_iter_key (iter), keylen)) {
         return true;
      }
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_find --
 *
 *       Searches through @iter starting from the current position for a key
 *       matching @key. This is a case-sensitive search meaning "KEY" and
 *       "key" would NOT match.
 *
 * Returns:
 *       true if @key is found.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_find (bson_iter_t *iter, /* INOUT */
                const char  *key)  /* IN */
{
   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (key, false);

   return _bson_iter_find_with_len (iter, key, -1);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_find_case --
 *
 *       Searches through @iter starting from the current position for a key
 *       matching @key. This is a case-insensitive search meaning "KEY" and
 *       "key" would match.
 *
 * Returns:
 *       true if @key is found.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_find_case (bson_iter_t *iter, /* INOUT */
                     const char  *key)  /* IN */
{
   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (key, false);

   while (bson_iter_next (iter)) {
#ifdef BSON_OS_WIN32
      if (!_stricmp(key, bson_iter_key (iter))) {
#else
      if (!strcasecmp (key, bson_iter_key (iter))) {
#endif
         return true;
      }
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_find_descendant --
 *
 *       Locates a descendant using the "parent.child.key" notation. This
 *       operates similar to bson_iter_find() except that it can recurse
 *       into children documents using the dot notation.
 *
 * Returns:
 *       true if the descendant was found and @descendant was initialized.
 *
 * Side effects:
 *       @descendant may be initialized.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_find_descendant (bson_iter_t *iter,       /* INOUT */
                           const char  *dotkey,     /* IN */
                           bson_iter_t *descendant) /* OUT */
{
   bson_iter_t tmp;
   const char *dot;
   size_t sublen;

   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (dotkey, false);
   bson_return_val_if_fail (descendant, false);

   if ((dot = strchr (dotkey, '.'))) {
      sublen = dot - dotkey;
   } else {
      sublen = strlen (dotkey);
   }

   if (_bson_iter_find_with_len (iter, dotkey, (int)sublen)) {
      if (!dot) {
         *descendant = *iter;
         return true;
      }

      if (BSON_ITER_HOLDS_DOCUMENT (iter) || BSON_ITER_HOLDS_ARRAY (iter)) {
         if (bson_iter_recurse (iter, &tmp)) {
            return bson_iter_find_descendant (&tmp, dot + 1, descendant);
         }
      }
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_key --
 *
 *       Retrieves the key of the current field. The resulting key is valid
 *       while @iter is valid.
 *
 * Returns:
 *       A string that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_key (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, NULL);

   return bson_iter_key_unsafe (iter);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_type --
 *
 *       Retrieves the type of the current field.  It may be useful to check
 *       the type using the BSON_ITER_HOLDS_*() macros.
 *
 * Returns:
 *       A bson_type_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_type_t
bson_iter_type (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, BSON_TYPE_EOD);
   bson_return_val_if_fail (iter->raw, BSON_TYPE_EOD);
   bson_return_val_if_fail (iter->len, BSON_TYPE_EOD);

   return bson_iter_type_unsafe (iter);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_next --
 *
 *       Advances @iter to the next field of the underlying BSON document.
 *       If all fields have been exhausted, then %false is returned.
 *
 *       It is a programming error to use @iter after this function has
 *       returned false.
 *
 * Returns:
 *       true if the iter was advanced to the next record.
 *       otherwise false and @iter should be considered invalid.
 *
 * Side effects:
 *       @iter may be invalidated.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_next (bson_iter_t *iter) /* INOUT */
{
   const uint8_t *data;
   uint32_t o;
   unsigned int len;

   bson_return_val_if_fail (iter, false);

   if (!iter->raw) {
      return false;
   }

   data = iter->raw;
   len = iter->len;

   iter->off = iter->next_off;
   iter->type = iter->off;
   iter->key = iter->off + 1;
   iter->d1 = 0;
   iter->d2 = 0;
   iter->d3 = 0;
   iter->d4 = 0;

   for (o = iter->off + 1; o < len; o++) {
      if (!data [o]) {
         iter->d1 = ++o;
         goto fill_data_fields;
      }
   }

   goto mark_invalid;

fill_data_fields:

   switch (ITER_TYPE (iter)) {
   case BSON_TYPE_DATE_TIME:
   case BSON_TYPE_DOUBLE:
   case BSON_TYPE_INT64:
   case BSON_TYPE_TIMESTAMP:
      iter->next_off = o + 8;
      break;
   case BSON_TYPE_CODE:
   case BSON_TYPE_SYMBOL:
   case BSON_TYPE_UTF8:
      {
         uint32_t l;

         if ((o + 4) >= len) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         memcpy (&l, iter->raw + iter->d1, 4);
         l = BSON_UINT32_FROM_LE (l);

         if (l > (len - (o + 4))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->next_off = o + 4 + l;

         /*
          * Make sure the string length includes the NUL byte.
          */
         if (BSON_UNLIKELY ((l == 0) || (iter->next_off >= len))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         /*
          * Make sure the last byte is a NUL byte.
          */
         if (BSON_UNLIKELY ((iter->raw + iter->d2)[l - 1] != '\0')) {
            iter->err_off = o + 4 + l - 1;
            goto mark_invalid;
         }
      }
      break;
   case BSON_TYPE_BINARY:
      {
         bson_subtype_t subtype;
         uint32_t l;

         if (o >= (len - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         iter->d3 = o + 5;

         memcpy (&l, iter->raw + iter->d1, 4);
         l = BSON_UINT32_FROM_LE (l);

         if (l >= (len - o)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         subtype = *(iter->raw + iter->d2);

         if (subtype == BSON_SUBTYPE_BINARY_DEPRECATED) {
            if (l < 4) {
               iter->err_off = o;
               goto mark_invalid;
            }
         }

         iter->next_off = o + 5 + l;
      }
      break;
   case BSON_TYPE_ARRAY:
   case BSON_TYPE_DOCUMENT:
      {
         uint32_t l;

         if (o >= (len - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         memcpy (&l, iter->raw + iter->d1, 4);
         l = BSON_UINT32_FROM_LE (l);

         if ((l > len) || (l > (len - o))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->next_off = o + l;
      }
      break;
   case BSON_TYPE_OID:
      iter->next_off = o + 12;
      break;
   case BSON_TYPE_BOOL:
      iter->next_off = o + 1;
      break;
   case BSON_TYPE_REGEX:
      {
         bool eor = false;
         bool eoo = false;

         for (; o < len; o++) {
            if (!data [o]) {
               iter->d2 = ++o;
               eor = true;
               break;
            }
         }

         if (!eor) {
            iter->err_off = iter->next_off;
            goto mark_invalid;
         }

         for (; o < len; o++) {
            if (!data [o]) {
               eoo = true;
               break;
            }
         }

         if (!eoo) {
            iter->err_off = iter->next_off;
            goto mark_invalid;
         }

         iter->next_off = o + 1;
      }
      break;
   case BSON_TYPE_DBPOINTER:
      {
         uint32_t l;

         if (o >= (len - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         memcpy (&l, iter->raw + iter->d1, 4);
         l = BSON_UINT32_FROM_LE (l);

         if ((l > len) || (l > (len - o))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d3 = o + 4 + l;
         iter->next_off = o + 4 + l + 12;
      }
      break;
   case BSON_TYPE_CODEWSCOPE:
      {
         uint32_t l;
         uint32_t doclen;

         if ((len < 19) || (o >= (len - 14))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         iter->d3 = o + 8;

         memcpy (&l, iter->raw + iter->d1, 4);
         l = BSON_UINT32_FROM_LE (l);

         if ((l < 14) || (l >= (len - o))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->next_off = o + l;

         if (iter->next_off >= len) {
            iter->err_off = o;
            goto mark_invalid;
         }

         memcpy (&l, iter->raw + iter->d2, 4);
         l = BSON_UINT32_FROM_LE (l);

         if (l >= (len - o - 4 - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         if ((o + 4 + 4 + l + 4) >= iter->next_off) {
            iter->err_off = o + 4;
            goto mark_invalid;
         }

         iter->d4 = o + 4 + 4 + l;
         memcpy (&doclen, iter->raw + iter->d4, 4);
         doclen = BSON_UINT32_FROM_LE (doclen);

         if ((o + 4 + 4 + l + doclen) != iter->next_off) {
            iter->err_off = o + 4 + 4 + l;
            goto mark_invalid;
         }
      }
      break;
   case BSON_TYPE_INT32:
      iter->next_off = o + 4;
      break;
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_MINKEY:
   case BSON_TYPE_NULL:
   case BSON_TYPE_UNDEFINED:
      iter->d1 = -1;
      iter->next_off = o;
      break;
   case BSON_TYPE_EOD:
   default:
      iter->err_off = o;
      goto mark_invalid;
   }

   /*
    * Check to see if any of the field locations would overflow the
    * current BSON buffer. If so, set the error location to the offset
    * of where the field starts.
    */
   if (iter->next_off >= len) {
      iter->err_off = o;
      goto mark_invalid;
   }

   iter->err_off = 0;

   return true;

mark_invalid:
   iter->raw = NULL;
   iter->len = 0;
   iter->next_off = 0;

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_binary --
 *
 *       Retrieves the BSON_TYPE_BINARY field. The subtype is stored in
 *       @subtype.  The length of @binary in bytes is stored in @binary_len.
 *
 *       @binary should not be modified or freed and is only valid while
 *       @iter is on the current field.
 *
 * Parameters:
 *       @iter: A bson_iter_t
 *       @subtype: A location for the binary subtype.
 *       @binary_len: A location for the length of @binary.
 *       @binary: A location for a pointer to the binary data.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_binary (const bson_iter_t  *iter,        /* IN */
                  bson_subtype_t     *subtype,     /* OUT */
                  uint32_t           *binary_len,  /* OUT */
                  const uint8_t     **binary)      /* OUT */
{
   bson_subtype_t backup;

   bson_return_if_fail (iter);
   bson_return_if_fail (!binary || binary_len);

   if (ITER_TYPE (iter) == BSON_TYPE_BINARY) {
      if (!subtype) {
         subtype = &backup;
      }

      *subtype = (bson_subtype_t) *(iter->raw + iter->d2);

      if (binary) {
         memcpy (binary_len, (iter->raw + iter->d1), 4);
         *binary_len = BSON_UINT32_FROM_LE (*binary_len);
         *binary = iter->raw + iter->d3;

         if (*subtype == BSON_SUBTYPE_BINARY_DEPRECATED) {
            *binary_len -= sizeof (int32_t);
            *binary += sizeof (int32_t);
         }
      }

      return;
   }

   if (binary) {
      *binary = NULL;
   }

   if (binary_len) {
      *binary_len = 0;
   }

   if (subtype) {
      *subtype = BSON_SUBTYPE_BINARY;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_bool --
 *
 *       Retrieves the current field of type BSON_TYPE_BOOL.
 *
 * Returns:
 *       true or false, dependent on bson document.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_bool (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   if (ITER_TYPE (iter) == BSON_TYPE_BOOL) {
      return bson_iter_bool_unsafe (iter);
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_as_bool --
 *
 *       If @iter is on a boolean field, returns the boolean. If it is on a
 *       non-boolean field such as int32, int64, or double, it will convert
 *       the value to a boolean.
 *
 *       Zero is false, and non-zero is true.
 *
 * Returns:
 *       true or false, dependent on field type.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_as_bool (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   switch ((int)ITER_TYPE (iter)) {
   case BSON_TYPE_BOOL:
      return bson_iter_bool (iter);
   case BSON_TYPE_DOUBLE:
      return !(bson_iter_double (iter) == 0.0);
   case BSON_TYPE_INT64:
      return !(bson_iter_int64 (iter) == 0);
   case BSON_TYPE_INT32:
      return !(bson_iter_int32 (iter) == 0);
   case BSON_TYPE_UTF8:
      return true;
   case BSON_TYPE_NULL:
   case BSON_TYPE_UNDEFINED:
      return false;
   default:
      return true;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_double --
 *
 *       Retrieves the current field of type BSON_TYPE_DOUBLE.
 *
 * Returns:
 *       A double.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

double
bson_iter_double (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   if (ITER_TYPE (iter) == BSON_TYPE_DOUBLE) {
      return bson_iter_double_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_int32 --
 *
 *       Retrieves the value of the field of type BSON_TYPE_INT32.
 *
 * Returns:
 *       A 32-bit signed integer.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int32_t
bson_iter_int32 (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   if (ITER_TYPE (iter) == BSON_TYPE_INT32) {
      return bson_iter_int32_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_int64 --
 *
 *       Retrieves a 64-bit signed integer for the current BSON_TYPE_INT64
 *       field.
 *
 * Returns:
 *       A 64-bit signed integer.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_iter_int64 (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   if (ITER_TYPE (iter) == BSON_TYPE_INT64) {
      return bson_iter_int64_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_as_int64 --
 *
 *       If @iter is not an int64 field, it will try to convert the value to
 *       an int64. Such field types include:
 *
 *        - bool
 *        - double
 *        - int32
 *
 * Returns:
 *       An int64_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_iter_as_int64 (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   switch ((int)ITER_TYPE (iter)) {
   case BSON_TYPE_BOOL:
      return (int64_t)bson_iter_bool (iter);
   case BSON_TYPE_DOUBLE:
      return (int64_t)bson_iter_double (iter);
   case BSON_TYPE_INT64:
      return bson_iter_int64 (iter);
   case BSON_TYPE_INT32:
      return (int64_t)bson_iter_int32 (iter);
   default:
      return 0;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_oid --
 *
 *       Retrieves the current field of type %BSON_TYPE_OID. The result is
 *       valid while @iter is valid.
 *
 * Returns:
 *       A bson_oid_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const bson_oid_t *
bson_iter_oid (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, NULL);

   if (ITER_TYPE (iter) == BSON_TYPE_OID) {
      return bson_iter_oid_unsafe (iter);
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_regex --
 *
 *       Fetches the current field from the iter which should be of type
 *       BSON_TYPE_REGEX.
 *
 * Returns:
 *       Regex from @iter. This should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_regex (const bson_iter_t *iter,    /* IN */
                 const char       **options) /* IN */
{
   const char *ret = NULL;
   const char *ret_options = NULL;

   bson_return_val_if_fail (iter, NULL);

   if (ITER_TYPE (iter) == BSON_TYPE_REGEX) {
      ret = (const char *)(iter->raw + iter->d1);
      ret_options = (const char *)(iter->raw + iter->d2);
   }

   if (options) {
      *options = ret_options;
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_utf8 --
 *
 *       Retrieves the current field of type %BSON_TYPE_UTF8 as a UTF-8
 *       encoded string.
 *
 * Parameters:
 *       @iter: A bson_iter_t.
 *       @length: A location for the length of the string.
 *
 * Returns:
 *       A string that should not be modified or freed.
 *
 * Side effects:
 *       @length will be set to the result strings length if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_utf8 (const bson_iter_t *iter,   /* IN */
                uint32_t          *length) /* OUT */
{
   bson_return_val_if_fail (iter, NULL);

   if (ITER_TYPE (iter) == BSON_TYPE_UTF8) {
      if (length) {
         *length = bson_iter_utf8_len_unsafe (iter);
      }

      return (const char *)(iter->raw + iter->d2);
   }

   if (length) {
      *length = 0;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_dup_utf8 --
 *
 *       Copies the current UTF-8 element into a newly allocated string. The
 *       string should be freed using bson_free() when the caller is
 *       finished with it.
 *
 * Returns:
 *       A newly allocated char* that should be freed with bson_free().
 *
 * Side effects:
 *       @length will be set to the result strings length if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_iter_dup_utf8 (const bson_iter_t *iter,   /* IN */
                    uint32_t          *length) /* OUT */
{
   uint32_t local_length = 0;
   const char *str;
   char *ret = NULL;

   bson_return_val_if_fail (iter, NULL);

   if ((str = bson_iter_utf8 (iter, &local_length))) {
      ret = bson_malloc0 (local_length + 1);
      memcpy (ret, str, local_length);
      ret[local_length] = '\0';
   }

   if (length) {
      *length = local_length;
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_code --
 *
 *       Retrieves the current field of type %BSON_TYPE_CODE. The length of
 *       the resulting string is stored in @length.
 *
 * Parameters:
 *       @iter: A bson_iter_t.
 *       @length: A location for the code length.
 *
 * Returns:
 *       A NUL-terminated string containing the code which should not be
 *       modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_code (const bson_iter_t *iter,   /* IN */
                uint32_t          *length) /* OUT */
{
   bson_return_val_if_fail (iter, NULL);

   if (ITER_TYPE (iter) == BSON_TYPE_CODE) {
      if (length) {
         *length = bson_iter_utf8_len_unsafe (iter);
      }

      return (const char *)(iter->raw + iter->d2);
   }

   if (length) {
      *length = 0;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_codewscope --
 *
 *       Similar to bson_iter_code() but with a scope associated encoded as
 *       a BSON document. @scope should not be modified or freed. It is
 *       valid while @iter is valid.
 *
 * Parameters:
 *       @iter: A #bson_iter_t.
 *       @length: A location for the length of resulting string.
 *       @scope_len: A location for the length of @scope.
 *       @scope: A location for the scope encoded as BSON.
 *
 * Returns:
 *       A NUL-terminated string that should not be modified or freed.
 *
 * Side effects:
 *       @length is set to the resulting string length in bytes.
 *       @scope_len is set to the length of @scope in bytes.
 *       @scope is set to the scope documents buffer which can be
 *       turned into a bson document with bson_init_static().
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_codewscope (const bson_iter_t  *iter,      /* IN */
                      uint32_t           *length,    /* OUT */
                      uint32_t           *scope_len, /* OUT */
                      const uint8_t     **scope)     /* OUT */
{
   uint32_t len;

   bson_return_val_if_fail (iter, NULL);

   if (ITER_TYPE (iter) == BSON_TYPE_CODEWSCOPE) {
      if (length) {
         memcpy (&len, iter->raw + iter->d2, 4);
         *length = BSON_UINT32_FROM_LE (len) - 1;
      }

      memcpy (&len, iter->raw + iter->d4, 4);
      *scope_len = BSON_UINT32_FROM_LE (len);
      *scope = iter->raw + iter->d4;
      return (const char *)(iter->raw + iter->d3);
   }

   if (length) {
      *length = 0;
   }

   if (scope_len) {
      *scope_len = 0;
   }

   if (scope) {
      *scope = NULL;
   }

   return NULL;
}


/**
 * bson_iter_dbpointer:
 *
 *
 *
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_dbpointer --
 *
 *       Retrieves a BSON_TYPE_DBPOINTER field. @collection_len will be set
 *       to the length of the collection name. The collection name will be
 *       placed into @collection. The oid will be placed into @oid.
 *
 *       @collection and @oid should not be modified.
 *
 * Parameters:
 *       @iter: A #bson_iter_t.
 *       @collection_len: A location for the length of @collection.
 *       @collection: A location for the collection name.
 *       @oid: A location for the oid.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @collection_len is set to the length of @collection in bytes
 *       excluding the null byte.
 *       @collection is set to the collection name, including a terminating
 *       null byte.
 *       @oid is initialized with the oid.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_dbpointer (const bson_iter_t  *iter,           /* IN */
                     uint32_t           *collection_len, /* OUT */
                     const char        **collection,     /* OUT */
                     const bson_oid_t  **oid)            /* OUT */
{
   bson_return_if_fail (iter);

   if (collection) {
      *collection = NULL;
   }

   if (oid) {
      *oid = NULL;
   }

   if (ITER_TYPE (iter) == BSON_TYPE_DBPOINTER) {
      if (collection_len) {
         memcpy (collection_len, (iter->raw + iter->d1), 4);
         *collection_len = BSON_UINT32_FROM_LE (*collection_len);

         if ((*collection_len) > 0) {
            (*collection_len)--;
         }
      }

      if (collection) {
         *collection = (const char *)(iter->raw + iter->d2);
      }

      if (oid) {
         *oid = (const bson_oid_t *)(iter->raw + iter->d3);
      }
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_symbol --
 *
 *       Retrieves the symbol of the current field of type BSON_TYPE_SYMBOL.
 *
 * Parameters:
 *       @iter: A bson_iter_t.
 *       @length: A location for the length of the symbol.
 *
 * Returns:
 *       A string containing the symbol as UTF-8. The value should not be
 *       modified or freed.
 *
 * Side effects:
 *       @length is set to the resulting strings length in bytes,
 *       excluding the null byte.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_symbol (const bson_iter_t *iter,   /* IN */
                  uint32_t          *length) /* OUT */
{
   const char *ret = NULL;
   uint32_t ret_length = 0;

   bson_return_val_if_fail (iter, NULL);

   if (ITER_TYPE (iter) == BSON_TYPE_SYMBOL) {
      ret = (const char *)(iter->raw + iter->d2);
      ret_length = bson_iter_utf8_len_unsafe (iter);
   }

   if (length) {
      *length = ret_length;
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_date_time --
 *
 *       Fetches the number of milliseconds elapsed since the UNIX epoch.
 *       This value can be negative as times before 1970 are valid.
 *
 * Returns:
 *       A signed 64-bit integer containing the number of milliseconds.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_iter_date_time (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   if (ITER_TYPE (iter) == BSON_TYPE_DATE_TIME) {
      return bson_iter_int64_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_time_t --
 *
 *       Retrieves the current field of type BSON_TYPE_DATE_TIME as a
 *       time_t.
 *
 * Returns:
 *       A #time_t of the number of seconds since UNIX epoch in UTC.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

time_t
bson_iter_time_t (const bson_iter_t *iter) /* IN */
{
   bson_return_val_if_fail (iter, 0);

   if (ITER_TYPE (iter) == BSON_TYPE_DATE_TIME) {
      return bson_iter_time_t_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_timestamp --
 *
 *       Fetches the current field if it is a BSON_TYPE_TIMESTAMP.
 *
 * Parameters:
 *       @iter: A #bson_iter_t.
 *       @timestamp: a location for the timestamp.
 *       @increment: A location for the increment.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @timestamp is initialized.
 *       @increment is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_timestamp (const bson_iter_t *iter,      /* IN */
                     uint32_t          *timestamp, /* OUT */
                     uint32_t          *increment) /* OUT */
{
   uint64_t encoded;
   uint32_t ret_timestamp = 0;
   uint32_t ret_increment = 0;

   bson_return_if_fail (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_TIMESTAMP) {
      memcpy (&encoded, iter->raw + iter->d1, 8);
      encoded = BSON_UINT64_FROM_LE (encoded);
      ret_timestamp = (encoded >> 32) & 0xFFFFFFFF;
      ret_increment = encoded & 0xFFFFFFFF;
   }

   if (timestamp) {
      *timestamp = ret_timestamp;
   }

   if (increment) {
      *increment = ret_increment;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_timeval --
 *
 *       Retrieves the current field of type BSON_TYPE_DATE_TIME and stores
 *       it into the struct timeval provided. tv->tv_sec is set to the
 *       number of seconds since the UNIX epoch in UTC.
 *
 *       Since BSON_TYPE_DATE_TIME does not support fractions of a second,
 *       tv->tv_usec will always be set to zero.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @tv is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_timeval (const bson_iter_t *iter,  /* IN */
                   struct timeval    *tv)    /* OUT */
{
   bson_return_if_fail (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_DATE_TIME) {
      bson_iter_timeval_unsafe (iter, tv);
      return;
   }

   memset (tv, 0, sizeof *tv);
}


/**
 * bson_iter_document:
 * @iter: a bson_iter_t.
 * @document_len: A location for the document length.
 * @document: A location for a pointer to the document buffer.
 *
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_document --
 *
 *       Retrieves the data to the document BSON structure and stores the
 *       length of the document buffer in @document_len and the document
 *       buffer in @document.
 *
 *       If you would like to iterate over the child contents, you might
 *       consider creating a bson_t on the stack such as the following. It
 *       allows you to call functions taking a const bson_t* only.
 *
 *          bson_t b;
 *          uint32_t len;
 *          const uint8_t *data;
 *
 *          bson_iter_document(iter, &len, &data);
 *
 *          if (bson_init_static (&b, data, len)) {
 *             ...
 *          }
 *
 *       There is no need to cleanup the bson_t structure as no data can be
 *       modified in the process of its use (as it is static/const).
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @document_len is initialized.
 *       @document is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_document (const bson_iter_t  *iter,         /* IN */
                    uint32_t           *document_len, /* OUT */
                    const uint8_t     **document)     /* OUT */
{
   bson_return_if_fail (iter);
   bson_return_if_fail (document_len);
   bson_return_if_fail (document);

   *document = NULL;
   *document_len = 0;

   if (ITER_TYPE (iter) == BSON_TYPE_DOCUMENT) {
      memcpy (document_len, (iter->raw + iter->d1), 4);
      *document_len = BSON_UINT32_FROM_LE (*document_len);
      *document = (iter->raw + iter->d1);
   }
}


/**
 * bson_iter_array:
 * @iter: a #bson_iter_t.
 * @array_len: A location for the array length.
 * @array: A location for a pointer to the array buffer.
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_array --
 *
 *       Retrieves the data to the array BSON structure and stores the
 *       length of the array buffer in @array_len and the array buffer in
 *       @array.
 *
 *       If you would like to iterate over the child contents, you might
 *       consider creating a bson_t on the stack such as the following. It
 *       allows you to call functions taking a const bson_t* only.
 *
 *          bson_t b;
 *          uint32_t len;
 *          const uint8_t *data;
 *
 *          bson_iter_array (iter, &len, &data);
 *
 *          if (bson_init_static (&b, data, len)) {
 *             ...
 *          }
 *
 *       There is no need to cleanup the #bson_t structure as no data can be
 *       modified in the process of its use.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @array_len is initialized.
 *       @array is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_array (const bson_iter_t  *iter,      /* IN */
                 uint32_t           *array_len, /* OUT */
                 const uint8_t     **array)     /* OUT */
{
   bson_return_if_fail (iter);
   bson_return_if_fail (array_len);
   bson_return_if_fail (array);

   *array = NULL;
   *array_len = 0;

   if (ITER_TYPE (iter) == BSON_TYPE_ARRAY) {
      memcpy (array_len, (iter->raw + iter->d1), 4);
      *array_len = BSON_UINT32_FROM_LE (*array_len);
      *array = (iter->raw + iter->d1);
   }
}


#define VISIT_FIELD(name) visitor->visit_##name && visitor->visit_##name
#define VISIT_AFTER VISIT_FIELD (after)
#define VISIT_BEFORE VISIT_FIELD (before)
#define VISIT_CORRUPT if (visitor->visit_corrupt) visitor->visit_corrupt
#define VISIT_DOUBLE VISIT_FIELD (double)
#define VISIT_UTF8 VISIT_FIELD (utf8)
#define VISIT_DOCUMENT VISIT_FIELD (document)
#define VISIT_ARRAY VISIT_FIELD (array)
#define VISIT_BINARY VISIT_FIELD (binary)
#define VISIT_UNDEFINED VISIT_FIELD (undefined)
#define VISIT_OID VISIT_FIELD (oid)
#define VISIT_BOOL VISIT_FIELD (bool)
#define VISIT_DATE_TIME VISIT_FIELD (date_time)
#define VISIT_NULL VISIT_FIELD (null)
#define VISIT_REGEX VISIT_FIELD (regex)
#define VISIT_DBPOINTER VISIT_FIELD (dbpointer)
#define VISIT_CODE VISIT_FIELD (code)
#define VISIT_SYMBOL VISIT_FIELD (symbol)
#define VISIT_CODEWSCOPE VISIT_FIELD (codewscope)
#define VISIT_INT32 VISIT_FIELD (int32)
#define VISIT_TIMESTAMP VISIT_FIELD (timestamp)
#define VISIT_INT64 VISIT_FIELD (int64)
#define VISIT_MAXKEY VISIT_FIELD (maxkey)
#define VISIT_MINKEY VISIT_FIELD (minkey)


/**
 * bson_iter_visit_all:
 * @iter: A #bson_iter_t.
 * @visitor: A #bson_visitor_t containing the visitors.
 * @data: User data for @visitor data parameters.
 *
 *
 * Returns: true if the visitor was pre-maturely ended; otherwise false.
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_visit_all --
 *
 *       Visits all fields forward from the current position of @iter. For
 *       each field found a function in @visitor will be called. Typically
 *       you will use this immediately after initializing a bson_iter_t.
 *
 *          bson_iter_init (&iter, b);
 *          bson_iter_visit_all (&iter, my_visitor, NULL);
 *
 *       @iter will no longer be valid after this function has executed and
 *       will need to be reinitialized if intending to reuse.
 *
 * Returns:
 *       true if successfully visited all fields or callback requested
 *       early termination, otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_visit_all (bson_iter_t          *iter,    /* INOUT */
                     const bson_visitor_t *visitor, /* IN */
                     void                 *data)    /* IN */
{
   const char *key;

   bson_return_val_if_fail (iter, false);
   bson_return_val_if_fail (visitor, false);

   while (bson_iter_next (iter)) {
      key = bson_iter_key_unsafe (iter);

      if (*key && !bson_utf8_validate (key, strlen (key), false)) {
         iter->err_off = iter->off;
         return true;
      }

      if (VISIT_BEFORE (iter, key, data)) {
         return true;
      }

      switch (bson_iter_type (iter)) {
      case BSON_TYPE_DOUBLE:

         if (VISIT_DOUBLE (iter, key, bson_iter_double (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_UTF8:
         {
            uint32_t utf8_len;
            const char *utf8;

            utf8 = bson_iter_utf8 (iter, &utf8_len);

            if (!bson_utf8_validate (utf8, utf8_len, true)) {
               iter->err_off = iter->off;
               return true;
            }

            if (VISIT_UTF8 (iter, key, utf8_len, utf8, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_DOCUMENT:
         {
            const uint8_t *docbuf = NULL;
            uint32_t doclen = 0;
            bson_t b;

            bson_iter_document (iter, &doclen, &docbuf);

            if (bson_init_static (&b, docbuf, doclen) &&
                VISIT_DOCUMENT (iter, key, &b, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_ARRAY:
         {
            const uint8_t *docbuf = NULL;
            uint32_t doclen = 0;
            bson_t b;

            bson_iter_array (iter, &doclen, &docbuf);

            if (bson_init_static (&b, docbuf, doclen)
                && VISIT_ARRAY (iter, key, &b, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_BINARY:
         {
            const uint8_t *binary = NULL;
            bson_subtype_t subtype = BSON_SUBTYPE_BINARY;
            uint32_t binary_len = 0;

            bson_iter_binary (iter, &subtype, &binary_len, &binary);

            if (VISIT_BINARY (iter, key, subtype, binary_len, binary, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_UNDEFINED:

         if (VISIT_UNDEFINED (iter, key, data)) {
            return true;
         }

         break;
      case BSON_TYPE_OID:

         if (VISIT_OID (iter, key, bson_iter_oid (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_BOOL:

         if (VISIT_BOOL (iter, key, bson_iter_bool (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_DATE_TIME:

         if (VISIT_DATE_TIME (iter, key, bson_iter_date_time (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_NULL:

         if (VISIT_NULL (iter, key, data)) {
            return true;
         }

         break;
      case BSON_TYPE_REGEX:
         {
            const char *regex = NULL;
            const char *options = NULL;
            regex = bson_iter_regex (iter, &options);

            if (VISIT_REGEX (iter, key, regex, options, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_DBPOINTER:
         {
            uint32_t collection_len = 0;
            const char *collection = NULL;
            const bson_oid_t *oid = NULL;

            bson_iter_dbpointer (iter, &collection_len, &collection, &oid);

            if (VISIT_DBPOINTER (iter, key, collection_len, collection, oid,
                                 data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_CODE:
         {
            uint32_t code_len;
            const char *code;

            code = bson_iter_code (iter, &code_len);

            if (VISIT_CODE (iter, key, code_len, code, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_SYMBOL:
         {
            uint32_t symbol_len;
            const char *symbol;

            symbol = bson_iter_symbol (iter, &symbol_len);

            if (VISIT_SYMBOL (iter, key, symbol_len, symbol, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_CODEWSCOPE:
         {
            uint32_t length = 0;
            const char *code;
            const uint8_t *docbuf = NULL;
            uint32_t doclen = 0;
            bson_t b;

            code = bson_iter_codewscope (iter, &length, &doclen, &docbuf);

            if (bson_init_static (&b, docbuf, doclen) &&
                VISIT_CODEWSCOPE (iter, key, length, code, &b, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_INT32:

         if (VISIT_INT32 (iter, key, bson_iter_int32 (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_TIMESTAMP:
         {
            uint32_t timestamp;
            uint32_t increment;
            bson_iter_timestamp (iter, &timestamp, &increment);

            if (VISIT_TIMESTAMP (iter, key, timestamp, increment, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_INT64:

         if (VISIT_INT64 (iter, key, bson_iter_int64 (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_MAXKEY:

         if (VISIT_MAXKEY (iter, bson_iter_key_unsafe (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_MINKEY:

         if (VISIT_MINKEY (iter, bson_iter_key_unsafe (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_EOD:
      default:
         break;
      }

      if (VISIT_AFTER (iter, bson_iter_key_unsafe (iter), data)) {
         return true;
      }
   }

   if (iter->err_off) {
      VISIT_CORRUPT (iter, data);
   }

#undef VISIT_FIELD

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_bool --
 *
 *       Overwrites the current BSON_TYPE_BOOLEAN field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_bool (bson_iter_t *iter,  /* IN */
                          bool         value) /* IN */
{
   bson_return_if_fail (iter);
   bson_return_if_fail (value == 1 || value == 0);

   if (ITER_TYPE (iter) == BSON_TYPE_BOOL) {
      memcpy ((void *)(iter->raw + iter->d1), &value, 1);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_int32 --
 *
 *       Overwrites the current BSON_TYPE_INT32 field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_int32 (bson_iter_t *iter,  /* IN */
                           int32_t      value) /* IN */
{
   bson_return_if_fail (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_INT32) {
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
      value = BSON_UINT32_TO_LE (value);
#endif
      memcpy ((void *)(iter->raw + iter->d1), &value, 4);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_int64 --
 *
 *       Overwrites the current BSON_TYPE_INT64 field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_int64 (bson_iter_t *iter,   /* IN */
                           int64_t      value)  /* IN */
{
   bson_return_if_fail (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_INT64) {
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
      value = BSON_UINT64_TO_LE (value);
#endif
      memcpy ((void *)(iter->raw + iter->d1), &value, 8);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_double --
 *
 *       Overwrites the current BSON_TYPE_DOUBLE field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_double (bson_iter_t *iter,  /* IN */
                            double       value) /* IN */
{
   bson_return_if_fail (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_DOUBLE) {
      value = BSON_DOUBLE_TO_LE (value);
      memcpy ((void *)(iter->raw + iter->d1), &value, 8);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_value --
 *
 *       Retrieves a bson_value_t containing the boxed value of the current
 *       element. The result of this function valid until the state of
 *       iter has been changed (through the use of bson_iter_next()).
 *
 * Returns:
 *       A bson_value_t that should not be modified or freed. If you need
 *       to hold on to the value, use bson_value_copy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const bson_value_t *
bson_iter_value (bson_iter_t *iter) /* IN */
{
   bson_value_t *value;

   bson_return_val_if_fail (iter, NULL);

   value = &iter->value;
   value->value_type = ITER_TYPE (iter);

   switch (value->value_type) {
   case BSON_TYPE_DOUBLE:
      value->value.v_double = bson_iter_double (iter);
      break;
   case BSON_TYPE_UTF8:
      value->value.v_utf8.str =
         (char *)bson_iter_utf8 (iter, &value->value.v_utf8.len);
      break;
   case BSON_TYPE_DOCUMENT:
      bson_iter_document (iter,
                          &value->value.v_doc.data_len,
                          (const uint8_t **)&value->value.v_doc.data);
      break;
   case BSON_TYPE_ARRAY:
      bson_iter_array (iter,
                       &value->value.v_doc.data_len,
                       (const uint8_t **)&value->value.v_doc.data);
      break;
   case BSON_TYPE_BINARY:
      bson_iter_binary (iter,
                        &value->value.v_binary.subtype,
                        &value->value.v_binary.data_len,
                        (const uint8_t **)&value->value.v_binary.data);
      break;
   case BSON_TYPE_OID:
      bson_oid_copy (bson_iter_oid (iter), &value->value.v_oid);
      break;
   case BSON_TYPE_BOOL:
      value->value.v_bool = bson_iter_bool (iter);
      break;
   case BSON_TYPE_DATE_TIME:
      value->value.v_datetime = bson_iter_date_time (iter);
      break;
   case BSON_TYPE_REGEX:
      value->value.v_regex.regex = (char *)bson_iter_regex (
            iter,
            (const char **)&value->value.v_regex.options);
      break;
   case BSON_TYPE_DBPOINTER: {
      const bson_oid_t *oid;

      bson_iter_dbpointer (iter,
                           &value->value.v_dbpointer.collection_len,
                           (const char **)&value->value.v_dbpointer.collection,
                           &oid);
      bson_oid_copy (oid, &value->value.v_dbpointer.oid);
      break;
   }
   case BSON_TYPE_CODE:
      value->value.v_code.code =
         (char *)bson_iter_code (
            iter,
            &value->value.v_code.code_len);
      break;
   case BSON_TYPE_SYMBOL:
      value->value.v_symbol.symbol =
         (char *)bson_iter_symbol (
            iter,
            &value->value.v_symbol.len);
      break;
   case BSON_TYPE_CODEWSCOPE:
      value->value.v_codewscope.code =
         (char *)bson_iter_codewscope (
            iter,
            &value->value.v_codewscope.code_len,
            &value->value.v_codewscope.scope_len,
            (const uint8_t **)&value->value.v_codewscope.scope_data);
      break;
   case BSON_TYPE_INT32:
      value->value.v_int32 = bson_iter_int32 (iter);
      break;
   case BSON_TYPE_TIMESTAMP:
      bson_iter_timestamp (iter,
                           &value->value.v_timestamp.timestamp,
                           &value->value.v_timestamp.increment);
      break;
   case BSON_TYPE_INT64:
      value->value.v_int64 = bson_iter_int64 (iter);
      break;
   case BSON_TYPE_NULL:
   case BSON_TYPE_UNDEFINED:
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_MINKEY:
      break;
   case BSON_TYPE_EOD:
   default:
      return NULL;
   }

   return value;
}
