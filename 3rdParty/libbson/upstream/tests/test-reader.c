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
#include <fcntl.h>

#include "bson-tests.h"
#include "TestSuite.h"


#ifndef BINARY_DIR
# define BINARY_DIR "tests/binary"
#endif


static void
test_reader_from_data (void)
{
   bson_reader_t *reader;
   uint8_t *buffer;
   const bson_t *b;
   uint32_t i;
   bool eof = false;

   buffer = bson_malloc0(4095);
   for (i = 0; i < 4095; i += 5) {
      buffer[i] = 5;
   }

   reader = bson_reader_new_from_data(buffer, 4095);

   for (i = 0; (b = bson_reader_read(reader, &eof)); i++) {
      const uint8_t *buf = bson_get_data(b);

      /* do nothing */
      assert(b->len == 5);
      assert(buf[0] == 5);
      assert(buf[1] == 0);
      assert(buf[2] == 0);
      assert(buf[3] == 0);
      assert(buf[4] == 0);
   }

   assert(i == (4095/5));

   assert_cmpint(eof, ==, true);

   bson_free(buffer);

   bson_reader_destroy(reader);
}


static void
test_reader_from_data_overflow (void)
{
   bson_reader_t *reader;
   uint8_t *buffer;
   const bson_t *b;
   uint32_t i;
   bool eof = false;

   buffer = bson_malloc0(4096);
   for (i = 0; i < 4095; i += 5) {
      buffer[i] = 5;
   }

   buffer[4095] = 5;

   reader = bson_reader_new_from_data(buffer, 4096);

   for (i = 0; (b = bson_reader_read(reader, &eof)); i++) {
      const uint8_t *buf = bson_get_data(b);
      assert(b->len == 5);
      assert(buf[0] == 5);
      assert(buf[1] == 0);
      assert(buf[2] == 0);
      assert(buf[3] == 0);
      assert(buf[4] == 0);
      eof = false;
   }

   assert(i == (4095/5));

   assert_cmpint(eof, ==, false);

   bson_free(buffer);

   bson_reader_destroy(reader);
}

static ssize_t
test_reader_from_handle_read(void * handle, void * buf, size_t len)
{
   return bson_read(*(int *)handle, buf, len);
}

static void
test_reader_from_handle_destroy(void * handle)
{
   bson_close(*(int *)handle);
}

static void
test_reader_from_handle (void)
{
   bson_reader_t *reader;
   const bson_t *b;
   bson_iter_t iter;
   uint32_t i;
   bool eof;
   int fd;

   fd  = bson_open(BINARY_DIR"/stream.bson", O_RDONLY);
   assert(-1 != fd);

   reader = bson_reader_new_from_handle ((void *)&fd,
                                         &test_reader_from_handle_read,
                                         &test_reader_from_handle_destroy);

   for (i = 0; i < 1000; i++) {
      eof = false;
      b = bson_reader_read(reader, &eof);
      assert(b);
      assert(bson_iter_init(&iter, b));
      assert(!bson_iter_next(&iter));
   }

   assert_cmpint(eof, ==, false);
   b = bson_reader_read(reader, &eof);
   assert(!b);
   assert_cmpint(eof, ==, true);
   bson_reader_destroy(reader);
}


static void
test_reader_tell (void)
{
   bson_reader_t *reader;
   const bson_t *b;
   uint32_t i;
   bson_iter_t iter;
   bool eof;
   int fd;

   fd  = bson_open(BINARY_DIR"/stream.bson", O_RDONLY);
   assert(-1 != fd);

   reader = bson_reader_new_from_handle ((void *)&fd,
                                         &test_reader_from_handle_read,
                                         &test_reader_from_handle_destroy);

   for (i = 0; i < 1000; i++) {
      if (i) {
         assert_cmpint(5 * i, ==, bson_reader_tell(reader));
      } else {
         assert_cmpint(0, ==, bson_reader_tell(reader));
      }
      eof = false;
      b = bson_reader_read(reader, &eof);
      assert(b);
      assert(bson_iter_init(&iter, b));
      assert(!bson_iter_next(&iter));
   }

   assert_cmpint(5000, ==, bson_reader_tell(reader));
   assert_cmpint(eof, ==, false);
   b = bson_reader_read(reader, &eof);
   assert(!b);
   assert_cmpint(eof, ==, true);
   bson_reader_destroy(reader);
}


static void
test_reader_from_handle_corrupt (void)
{
   bson_reader_t *reader;
   const bson_t *b;
   uint32_t i;
   bson_iter_t iter;
   bool eof;
   int fd;

   fd  = bson_open(BINARY_DIR"/stream_corrupt.bson", O_RDONLY);
   assert(-1 != fd);

   reader = bson_reader_new_from_handle ((void *)&fd,
                                         &test_reader_from_handle_read,
                                         &test_reader_from_handle_destroy);

   for (i = 0; i < 1000; i++) {
      b = bson_reader_read(reader, &eof);
      assert(b);
      assert(bson_iter_init(&iter, b));
      assert(!bson_iter_next(&iter));
   }

   b = bson_reader_read(reader, &eof);
   assert(!b);
   bson_reader_destroy(reader);
}


static void
test_reader_grow_buffer (void)
{
   bson_reader_t *reader;
   const bson_t *b;
   bool eof = false;
   int fd;

   fd  = bson_open(BINARY_DIR"/readergrow.bson", O_RDONLY);
   assert(-1 != fd);

   reader = bson_reader_new_from_handle ((void *)&fd,
                                         &test_reader_from_handle_read,
                                         &test_reader_from_handle_destroy);

   b = bson_reader_read(reader, &eof);
   assert(b);
   assert(!eof);

   b = bson_reader_read(reader, &eof);
   assert(!b);
   assert(eof);

   bson_reader_destroy(reader);
}


void
test_reader_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/reader/new_from_data", test_reader_from_data);
   TestSuite_Add (suite, "/bson/reader/new_from_data_overflow",
                  test_reader_from_data_overflow);
   TestSuite_Add (suite, "/bson/reader/new_from_handle", test_reader_from_handle);
   TestSuite_Add (suite, "/bson/reader/tell", test_reader_tell);
   TestSuite_Add (suite, "/bson/reader/new_from_handle_corrupt",
                  test_reader_from_handle_corrupt);
   TestSuite_Add (suite, "/bson/reader/grow_buffer", test_reader_grow_buffer);
}
