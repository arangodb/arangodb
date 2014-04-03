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


#include <assert.h>
#include <bson.h>
#include <fcntl.h>
#include <time.h>

#include "bson-tests.h"
#include "TestSuite.h"


static void
test_bson_string_new (void)
{
   bson_string_t *str;
   char *s;

   str = bson_string_new(NULL);
   s = bson_string_free(str, false);
   assert(s);
   assert(!strcmp(s, ""));
   bson_free(s);

   str = bson_string_new("");
   s = bson_string_free(str, false);
   assert(s);
   assert(!*s);
   assert(0 == strcmp(s, ""));
   bson_free(s);

   str = bson_string_new("abcdef");
   s = bson_string_free(str, false);
   assert(s);
   assert(!strcmp(s, "abcdef"));
   bson_free(s);

   str = bson_string_new("");
   s = bson_string_free(str, true);
   assert(!s);
}


static void
test_bson_string_append (void)
{
   bson_string_t *str;
   char *s;

   str = bson_string_new(NULL);
   bson_string_append(str, "christian was here");
   bson_string_append(str, "\n");
   s = bson_string_free(str, false);
   assert(s);
   assert(!strcmp(s, "christian was here\n"));
   bson_free(s);

   str = bson_string_new(">>>");
   bson_string_append(str, "^^^");
   bson_string_append(str, "<<<");
   s = bson_string_free(str, false);
   assert(s);
   assert(!strcmp(s, ">>>^^^<<<"));
   bson_free(s);
}


static void
test_bson_string_append_c (void)
{
   bson_string_t *str;
   char *s;

   str = bson_string_new(NULL);
   bson_string_append_c(str, 'c');
   bson_string_append_c(str, 'h');
   bson_string_append_c(str, 'r');
   bson_string_append_c(str, 'i');
   bson_string_append_c(str, 's');
   s = bson_string_free(str, false);
   assert(s);
   assert(!strcmp(s, "chris"));
   bson_free(s);
}


static void
test_bson_string_append_printf (void)
{
   bson_string_t *str;

   str = bson_string_new("abcd ");
   bson_string_append_printf(str, "%d %d %d", 1, 2, 3);
   BSON_ASSERT(!strcmp(str->str, "abcd 1 2 3"));
   bson_string_truncate(str, 2);
   BSON_ASSERT(!strcmp(str->str, "ab"));
   bson_string_free(str, true);
}


static void
test_bson_string_append_unichar (void)
{
   static const char test1[] = {0xe2, 0x82, 0xac, 0};
   bson_string_t *str;
   char *s;

   str = bson_string_new(NULL);
   bson_string_append_unichar(str, 0x20AC);
   s = bson_string_free(str, false);
   assert(s);
   assert(!strcmp(s, test1));
   bson_free(s);
}


static void
test_bson_strdup_printf (void)
{
   char *s;

   s = bson_strdup_printf("%s:%u", "localhost", 27017);
   assert(!strcmp(s, "localhost:27017"));
   bson_free(s);
}


static void
test_bson_strdup (void)
{
   char *s;

   s = bson_strdup("localhost:27017");
   assert(!strcmp(s, "localhost:27017"));
   bson_free(s);
}


static void
test_bson_strndup (void)
{
   char *s;

   s = bson_strndup("asdf", 2);
   assert(!strcmp(s, "as"));
   bson_free(s);
}


void
test_string_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/string/new", test_bson_string_new);
   TestSuite_Add (suite, "/bson/string/append", test_bson_string_append);
   TestSuite_Add (suite, "/bson/string/append_c", test_bson_string_append_c);
   TestSuite_Add (suite, "/bson/string/append_printf", test_bson_string_append_printf);
   TestSuite_Add (suite, "/bson/string/append_unichar", test_bson_string_append_unichar);
   TestSuite_Add (suite, "/bson/string/strdup", test_bson_strdup);
   TestSuite_Add (suite, "/bson/string/strdup_printf", test_bson_strdup_printf);
   TestSuite_Add (suite, "/bson/string/strndup", test_bson_strndup);
}
