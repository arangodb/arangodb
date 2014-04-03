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

#include <bson.h>

#include <assert.h>

#include "bson-tests.h"
#include "TestSuite.h"


static void
test_bson_utf8_validate (void)
{
   static const char test1[] = {0xe2, 0x82, 0xac, ' ', 0xe2, 0x82, 0xac, ' ', 0xe2, 0x82, 0xac, 0};

   assert(bson_utf8_validate("asdf", 4, false));
   assert(bson_utf8_validate("asdf", 4, true));
   assert(bson_utf8_validate("asdf", 5, true));
   assert(!bson_utf8_validate("asdf", 5, false));

   assert(bson_utf8_validate(test1, 11, true));
   assert(bson_utf8_validate(test1, 11, false));
   assert(bson_utf8_validate(test1, 12, true));
   assert(!bson_utf8_validate(test1, 12, false));
}


static void
test_bson_utf8_escape_for_json (void)
{
   char *str;

   str = bson_utf8_escape_for_json("my\0key", 6);
   assert(0 == memcmp(str, "my\\u0000key", 7));
   bson_free(str);

   str = bson_utf8_escape_for_json("my\"key", 6);
   assert(0 == memcmp(str, "my\\\"key", 8));
   bson_free(str);

   str = bson_utf8_escape_for_json("my\\key", 6);
   assert(0 == memcmp(str, "my\\\\key", 8));
   bson_free(str);

   str = bson_utf8_escape_for_json("\\\"\\\"", -1);
   assert(0 == memcmp(str, "\\\\\\\"\\\\\\\"", 9));
   bson_free(str);
}


static void
test_bson_utf8_get_char (void)
{
   static const char *test1 = "asdf";
   static const char test2[] = {0xe2, 0x82, 0xac, ' ', 0xe2, 0x82, 0xac, ' ', 0xe2, 0x82, 0xac, 0};
   const char *c;

   c = test1;
   assert(bson_utf8_get_char(c) == 'a');
   c = bson_utf8_next_char(c);
   assert(bson_utf8_get_char(c) == 's');
   c = bson_utf8_next_char(c);
   assert(bson_utf8_get_char(c) == 'd');
   c = bson_utf8_next_char(c);
   assert(bson_utf8_get_char(c) == 'f');
   c = bson_utf8_next_char(c);
   assert(!*c);

   c = test2;
   assert(bson_utf8_get_char(c) == 0x20AC);
   c = bson_utf8_next_char(c);
   assert(c == test2 + 3);
   assert(bson_utf8_get_char(c) == ' ');
   c = bson_utf8_next_char(c);
   assert(bson_utf8_get_char(c) == 0x20AC);
   c = bson_utf8_next_char(c);
   assert(bson_utf8_get_char(c) == ' ');
   c = bson_utf8_next_char(c);
   assert(bson_utf8_get_char(c) == 0x20AC);
   c = bson_utf8_next_char(c);
   assert(!*c);
}


static void
test_bson_utf8_from_unichar (void)
{
   static const char test1[] = {'a'};
   static const char test2[] = {0xc3, 0xbf};
   static const char test3[] = {0xe2, 0x82, 0xac};
   uint32_t len;
   char str[6];

   /*
    * First possible sequence of a certain length.
    */
   bson_utf8_from_unichar(0, str, &len);
   assert(len == 1);
   bson_utf8_from_unichar(0x00000080, str, &len);
   assert(len == 2);
   bson_utf8_from_unichar(0x00000800, str, &len);
   assert(len == 3);
   bson_utf8_from_unichar(0x00010000, str, &len);
   assert(len == 4);
   bson_utf8_from_unichar(0x00200000, str, &len);
   assert(len == 5);
   bson_utf8_from_unichar(0x04000000, str, &len);
   assert(len == 6);

   /*
    * Last possible sequence of a certain length.
    */
   bson_utf8_from_unichar(0x0000007F, str, &len);
   assert(len == 1);
   bson_utf8_from_unichar(0x000007FF, str, &len);
   assert(len == 2);
   bson_utf8_from_unichar(0x0000FFFF, str, &len);
   assert(len == 3);
   bson_utf8_from_unichar(0x001FFFFF, str, &len);
   assert(len == 4);
   bson_utf8_from_unichar(0x03FFFFFF, str, &len);
   assert(len == 5);
   bson_utf8_from_unichar(0x7FFFFFFF, str, &len);
   assert(len == 6);

   /*
    * Other interesting values.
    */
   bson_utf8_from_unichar(0x0000D7FF, str, &len);
   assert(len == 3);
   bson_utf8_from_unichar(0x0000E000, str, &len);
   assert(len == 3);
   bson_utf8_from_unichar(0x0000FFFD, str, &len);
   assert(len == 3);
   bson_utf8_from_unichar(0x0010FFFF, str, &len);
   assert(len == 4);
   bson_utf8_from_unichar(0x00110000, str, &len);
   assert(len == 4);

   bson_utf8_from_unichar('a', str, &len);
   assert(len == 1);
   assert(!memcmp(test1, str, 1));

   bson_utf8_from_unichar(0xFF, str, &len);
   assert(len == 2);
   assert(!memcmp(test2, str, 2));

   bson_utf8_from_unichar(0x20AC, str, &len);
   assert(len == 3);
   assert(!memcmp(test3, str, 3));
}


void
test_utf8_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/utf8/validate", test_bson_utf8_validate);
   TestSuite_Add (suite, "/bson/utf8/escape_for_json", test_bson_utf8_escape_for_json);
   TestSuite_Add (suite, "/bson/utf8/get_char_next_char", test_bson_utf8_get_char);
   TestSuite_Add (suite, "/bson/utf8/from_unichar", test_bson_utf8_from_unichar);
}
