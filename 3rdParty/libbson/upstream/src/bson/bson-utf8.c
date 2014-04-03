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


#include <string.h>

#include "bson-memory.h"
#include "bson-string.h"
#include "bson-utf8.h"


/*
 *--------------------------------------------------------------------------
 *
 * _bson_utf8_get_sequence --
 *
 *       Determine the sequence length of the first UTF-8 character in
 *       @utf8. The sequence length is stored in @seq_length and the mask
 *       for the first character is stored in @first_mask.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @seq_length is set.
 *       @first_mask is set.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_bson_utf8_get_sequence (const char *utf8,       /* IN */
                         uint8_t    *seq_length, /* OUT */
                         uint8_t    *first_mask) /* OUT */
{
   unsigned char c = *(const unsigned char *)utf8;
   uint8_t m;
   uint8_t n;

   /*
    * See the following[1] for a description of what the given multi-byte
    * sequences will be based on the bits set of the first byte. We also need
    * to mask the first byte based on that.  All subsequent bytes are masked
    * against 0x3F.
    *
    * [1] http://www.joelonsoftware.com/articles/Unicode.html
    */

   if ((c & 0x80) == 0) {
      n = 1;
      m = 0x7F;
   } else if ((c & 0xE0) == 0xC0) {
      n = 2;
      m = 0x1F;
   } else if ((c & 0xF0) == 0xE0) {
      n = 3;
      m = 0x0F;
   } else if ((c & 0xF8) == 0xF0) {
      n = 4;
      m = 0x07;
   } else if ((c & 0xFC) == 0xF8) {
      n = 5;
      m = 0x03;
   } else if ((c & 0xFE) == 0xFC) {
      n = 6;
      m = 0x01;
   } else {
      n = 0;
      m = 0;
   }

   *seq_length = n;
   *first_mask = m;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_validate --
 *
 *       Validates that @utf8 is a valid UTF-8 string.
 *
 *       If @allow_null is true, then \0 is allowed within @utf8_len bytes
 *       of @utf8.  Generally, this is bad practice since the main point of
 *       UTF-8 strings is that they can be used with strlen() and friends.
 *       However, some languages such as Python can send UTF-8 encoded
 *       strings with NUL's in them.
 *
 * Parameters:
 *       @utf8: A UTF-8 encoded string.
 *       @utf8_len: The length of @utf8 in bytes.
 *       @allow_null: If \0 is allowed within @utf8, exclusing trailing \0.
 *
 * Returns:
 *       true if @utf8 is valid UTF-8. otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_utf8_validate (const char *utf8,       /* IN */
                    size_t      utf8_len,   /* IN */
                    bool        allow_null) /* IN */
{
   uint8_t first_mask;
   uint8_t seq_length;
   unsigned i;
   unsigned j;

   bson_return_val_if_fail (utf8, false);

   for (i = 0; i < utf8_len; i += seq_length) {
      _bson_utf8_get_sequence (&utf8[i], &seq_length, &first_mask);

      if (!seq_length) {
         return false;
      }

      for (j = i + 1; j < (i + seq_length); j++) {
         if ((utf8[j] & 0xC0) != 0x80) {
            return false;
         }
      }

      if (!allow_null) {
         for (j = 0; j < seq_length; j++) {
            if (((i + j) > utf8_len) || !utf8[i + j]) {
               return false;
            }
         }
      }
   }

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_escape_for_json --
 *
 *       Allocates a new string matching @utf8 except that special
 *       characters in JSON will be escaped. The resulting string is also
 *       UTF-8 encoded.
 *
 *       Both " and \ characters will be escaped. Additionally, if a NUL
 *       byte is found before @utf8_len bytes, it will be converted to the
 *       two byte UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A UTF-8 encoded string.
 *       @utf8_len: The length of @utf8 in bytes or -1 if NUL terminated.
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_utf8_escape_for_json (const char *utf8,     /* IN */
                           ssize_t     utf8_len) /* IN */
{
   bson_unichar_t c;
   bson_string_t *str;
   const char *end;

   bson_return_val_if_fail (utf8, NULL);

   str = bson_string_new (NULL);

   if (utf8_len < 0) {
      utf8_len = strlen (utf8);
   }

   end = utf8 + utf8_len;

   for (; utf8 < end; utf8 = bson_utf8_next_char (utf8)) {
      c = bson_utf8_get_char (utf8);

      switch (c) {
      case '\\':
      case '"':
      case '/':
         bson_string_append_c (str, '\\');
         bson_string_append_unichar (str, c);
         break;
      case '\b':
         bson_string_append (str, "\\b");
         break;
      case '\f':
         bson_string_append (str, "\\f");
         break;
      case '\n':
         bson_string_append (str, "\\n");
         break;
      case '\r':
         bson_string_append (str, "\\r");
         break;
      case '\t':
         bson_string_append (str, "\\t");
         break;
      default:
         if (c < ' ') {
            bson_string_append_printf (str, "\\u%04u", (unsigned)c);
         } else {
            bson_string_append_unichar (str, c);
         }
         break;
      }
   }

   return bson_string_free (str, false);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_get_char --
 *
 *       Fetches the next UTF-8 character from the UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       A 32-bit bson_unichar_t reprsenting the multi-byte sequence.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_unichar_t
bson_utf8_get_char (const char *utf8) /* IN */
{
   bson_unichar_t c;
   uint8_t mask;
   uint8_t num;
   int i;

   bson_return_val_if_fail (utf8, -1);

   _bson_utf8_get_sequence (utf8, &num, &mask);
   c = (*utf8) & mask;

   for (i = 1; i < num; i++) {
      c = (c << 6) | (utf8[i] & 0x3F);
   }

   return c;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_next_char --
 *
 *       Returns an incremented pointer to the beginning of the next
 *       multi-byte sequence in @utf8.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       An incremented pointer in @utf8.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_utf8_next_char (const char *utf8) /* IN */
{
   uint8_t mask;
   uint8_t num;

   bson_return_val_if_fail (utf8, NULL);

   _bson_utf8_get_sequence (utf8, &num, &mask);

   return utf8 + num;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_from_unichar --
 *
 *       Converts the unichar to a sequence of utf8 bytes and stores those
 *       in @utf8. The number of bytes in the sequence are stored in @len.
 *
 * Parameters:
 *       @unichar: A bson_unichar_t.
 *       @utf8: A location for the multi-byte sequence.
 *       @len: A location for number of bytes stored in @utf8.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @utf8 is set.
 *       @len is set.
 *
 *--------------------------------------------------------------------------
 */

void
bson_utf8_from_unichar (
      bson_unichar_t  unichar,                               /* IN */
      char            utf8[BSON_ENSURE_ARRAY_PARAM_SIZE(6)], /* OUT */
      uint32_t       *len)                                   /* OUT */
{
   bson_return_if_fail (utf8);
   bson_return_if_fail (len);

   if (unichar <= 0x7F) {
      utf8[0] = unichar;
      *len = 1;
   } else if (unichar <= 0x7FF) {
      *len = 2;
      utf8[0] = 0xC0 | ((unichar >> 6) & 0x3F);
      utf8[1] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0xFFFF) {
      *len = 3;
      utf8[0] = 0xE0 | ((unichar >> 12) & 0xF);
      utf8[1] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[2] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0x1FFFFF) {
      *len = 4;
      utf8[0] = 0xF0 | ((unichar >> 18) & 0x7);
      utf8[1] = 0x80 | ((unichar >> 12) & 0x3F);
      utf8[2] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[3] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0x3FFFFFF) {
      *len = 5;
      utf8[0] = 0xF8 | ((unichar >> 24) & 0x3);
      utf8[1] = 0x80 | ((unichar >> 18) & 0x3F);
      utf8[2] = 0x80 | ((unichar >> 12) & 0x3F);
      utf8[3] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[4] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0x7FFFFFFF) {
      *len = 6;
      utf8[0] = 0xFC | ((unichar >> 31) & 0x1);
      utf8[1] = 0x80 | ((unichar >> 25) & 0x3F);
      utf8[2] = 0x80 | ((unichar >> 19) & 0x3F);
      utf8[3] = 0x80 | ((unichar >> 13) & 0x3F);
      utf8[4] = 0x80 | ((unichar >> 7) & 0x3F);
      utf8[5] = 0x80 | ((unichar) & 0x1);
   } else {
      *len = 0;
   }
}
