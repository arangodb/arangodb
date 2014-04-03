#include <bson.h>
#include <bcon.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include "bson-tests.h"
#include "TestSuite.h"

#ifndef BINARY_DIR
# define BINARY_DIR "tests/binary"
#endif

static void
test_bson_as_json (void)
{
   bson_oid_t oid;
   bson_t *b;
   bson_t *b2;
   char *str;
   size_t len;
   int i;

   bson_oid_init_from_string(&oid, "123412341234abcdabcdabcd");

   b = bson_new();
   assert(bson_append_utf8(b, "utf8", -1, "bar", -1));
   assert(bson_append_int32(b, "int32", -1, 1234));
   assert(bson_append_int64(b, "int64", -1, 4321));
   assert(bson_append_double(b, "double", -1, 123.4));
   assert(bson_append_undefined(b, "undefined", -1));
   assert(bson_append_null(b, "null", -1));
   assert(bson_append_oid(b, "oid", -1, &oid));
   assert(bson_append_bool(b, "true", -1, true));
   assert(bson_append_bool(b, "false", -1, false));
   assert(bson_append_time_t(b, "date", -1, time(NULL)));
   assert(bson_append_timestamp(b, "timestamp", -1, (uint32_t)time(NULL), 1234));
   assert(bson_append_regex(b, "regex", -1, "^abcd", "xi"));
   assert(bson_append_dbpointer(b, "dbpointer", -1, "mycollection", &oid));
   assert(bson_append_minkey(b, "minkey", -1));
   assert(bson_append_maxkey(b, "maxkey", -1));
   assert(bson_append_symbol(b, "symbol", -1, "var a = {};", -1));

   b2 = bson_new();
   assert(bson_append_int32(b2, "0", -1, 60));
   assert(bson_append_document(b, "document", -1, b2));
   assert(bson_append_array(b, "array", -1, b2));

   {
      const uint8_t binary[] = { 0, 1, 2, 3, 4 };
      assert(bson_append_binary(b, "binary", -1, BSON_SUBTYPE_BINARY,
                                binary, sizeof binary));
   }

   for (i = 0; i < 1000; i++) {
      str = bson_as_json(b, &len);
      bson_free(str);
   }

   bson_destroy(b);
   bson_destroy(b2);
}


static void
test_bson_as_json_string (void)
{
   size_t len;
   bson_t *b;
   char *str;

   b = bson_new();
   assert(bson_append_utf8(b, "foo", -1, "bar", -1));
   str = bson_as_json(b, &len);
   assert(len == 17);
   assert(!strcmp("{ \"foo\" : \"bar\" }", str));
   bson_free(str);
   bson_destroy(b);
}


static void
test_bson_as_json_int32 (void)
{
   size_t len;
   bson_t *b;
   char *str;

   b = bson_new();
   assert(bson_append_int32(b, "foo", -1, 1234));
   str = bson_as_json(b, &len);
   assert(len == 16);
   assert(!strcmp("{ \"foo\" : 1234 }", str));
   bson_free(str);
   bson_destroy(b);
}


static void
test_bson_as_json_int64 (void)
{
   size_t len;
   bson_t *b;
   char *str;

   b = bson_new();
   assert(bson_append_int64(b, "foo", -1, 341234123412341234ULL));
   str = bson_as_json(b, &len);
   assert(len == 30);
   assert(!strcmp("{ \"foo\" : 341234123412341234 }", str));
   bson_free(str);
   bson_destroy(b);
}


static void
test_bson_as_json_double (void)
{
   size_t len;
   bson_t *b;
   char *str;

   b = bson_new();
   assert(bson_append_double(b, "foo", -1, 123.456));
   str = bson_as_json(b, &len);
   assert(len >= 19);
   assert(!strncmp("{ \"foo\" : 123.456", str, 17));
   assert(!strcmp(" }", str + len - 2));
   bson_free(str);
   bson_destroy(b);
}


static void
test_bson_as_json_utf8 (void)
{
   size_t len;
   bson_t *b;
   char *str;

   b = bson_new();
   assert(bson_append_utf8(b, "€€€€€", -1, "€€€€€", -1));
   str = bson_as_json(b, &len);
   assert(!strcmp(str, "{ \"€€€€€\" : \"€€€€€\" }"));
   bson_free(str);
   bson_destroy(b);
}


static void
test_bson_as_json_stack_overflow (void)
{
   uint8_t *buf;
   bson_t b;
   size_t buflen = 1024 * 1024 * 17;
   char *str;
   int fd;
   ssize_t r;

   buf = bson_malloc0(buflen);

   fd = bson_open(BINARY_DIR"/stackoverflow.bson", O_RDONLY);
   BSON_ASSERT(-1 != fd);

   r = bson_read(fd, buf, buflen);
   BSON_ASSERT(r == 16777220);

   r = bson_init_static(&b, buf, 16777220);
   BSON_ASSERT(r);

   str = bson_as_json(&b, NULL);
   BSON_ASSERT(str);

   r = !!strstr(str, "...");
   BSON_ASSERT(str);

   bson_free(str);
   bson_destroy(&b);
   bson_free(buf);
}


static void
test_bson_corrupt (void)
{
   uint8_t *buf;
   bson_t b;
   size_t buflen = 1024;
   char *str;
   int fd;
   ssize_t r;

   buf = bson_malloc0(buflen);

   fd = bson_open(BINARY_DIR"/test55.bson", O_RDONLY);
   BSON_ASSERT(-1 != fd);

   r = bson_read(fd, buf, buflen);
   BSON_ASSERT(r == 24);

   r = bson_init_static(&b, buf, (uint32_t)r);
   BSON_ASSERT(r);

   str = bson_as_json(&b, NULL);
   BSON_ASSERT(!str);

   bson_destroy(&b);
   bson_free(buf);
}

static void
test_bson_corrupt_utf8 (void)
{
   uint8_t *buf;
   bson_t b;
   size_t buflen = 1024;
   char *str;
   int fd;
   ssize_t r;

   buf = bson_malloc0(buflen);

   fd = bson_open(BINARY_DIR"/test56.bson", O_RDONLY);
   BSON_ASSERT(-1 != fd);

   r = bson_read(fd, buf, buflen);
   BSON_ASSERT(r == 42);

   r = bson_init_static(&b, buf, (uint32_t)r);
   BSON_ASSERT(r);

   str = bson_as_json(&b, NULL);
   BSON_ASSERT(!str);

   bson_destroy(&b);
   bson_free(buf);
}


static void
test_bson_corrupt_binary (void)
{
   uint8_t *buf;
   bson_t b;
   size_t buflen = 1024;
   char *str;
   int fd;
   ssize_t r;

   buf = bson_malloc0(buflen);

   fd = bson_open(BINARY_DIR"/test57.bson", O_RDONLY);
   BSON_ASSERT(-1 != fd);

   r = bson_read(fd, buf, buflen);
   BSON_ASSERT(r == 26);

   r = bson_init_static(&b, buf, (uint32_t)r);
   BSON_ASSERT(r);

   str = bson_as_json(&b, NULL);
   BSON_ASSERT(!str);

   bson_destroy(&b);
   bson_free(buf);
}

static void
_test_bson_json_read_compare (const char *json,
                              int         size,
                              ...)
{
   bson_error_t error = { 0 };
   bson_json_reader_t *reader;
   va_list ap;
   int r;
   bson_t *compare;
   bson_t bson = BSON_INITIALIZER;

   reader = bson_json_data_reader_new ((size == 1), size);
   bson_json_data_reader_ingest(reader, (uint8_t *)json, strlen(json));

   va_start (ap, size);

   while ((r = bson_json_reader_read (reader, &bson, &error))) {
      if (r == -1) {
         fprintf (stderr, "%s\n", error.message);
         abort ();
      }

      compare = va_arg (ap, bson_t *);

      assert (compare);

      bson_eq_bson (&bson, compare);

      bson_destroy (compare);

      bson_reinit (&bson);
   }

   va_end (ap);

   bson_json_reader_destroy (reader);
   bson_destroy (&bson);
}

static void
test_bson_json_read(void)
{
   const char * json = "{ \n\
      \"foo\" : \"bar\", \n\
      \"bar\" : 12341, \n\
      \"baz\" : 123.456, \n\
      \"map\" : { \"a\" : 1 }, \n\
      \"array\" : [ 1, 2, 3, 4 ], \n\
      \"null\" : null, \n\
      \"boolean\" : true, \n\
      \"oid\" : { \n\
        \"$oid\" : \"000000000000000000000000\" \n\
      }, \n\
      \"binary\" : { \n\
        \"$type\" : \"00\", \n\
        \"$binary\" : \"ZGVhZGJlZWY=\" \n\
      }, \n\
      \"regex\" : { \n\
        \"$regex\" : \"foo|bar\", \n\
        \"$options\" : \"ism\" \n\
      }, \n\
      \"date\" : { \n\
        \"$date\" : 10000 \n\
      }, \n\
      \"ref\" : { \n\
        \"$ref\" : \"foo\", \n\
        \"$id\" : \"000000000000000000000000\" \n\
      }, \n\
      \"undefined\" : { \n\
        \"$undefined\" : true \n\
      }, \n\
      \"minkey\" : { \n\
        \"$minKey\" : 1 \n\
      }, \n\
      \"maxkey\" : { \n\
        \"$maxKey\" : 1 \n\
      }, \n\
      \"timestamp\" : { \n\
        \"$timestamp\" : { \n\
           \"t\" : 100, \n\
           \"i\" : 1000 \n\
        } \n\
      } \n\
   } { \"after\": \"b\" } { \"twice\" : true }";

   bson_oid_t oid;
   bson_t * first, *second, *third;

   bson_oid_init_from_string(&oid, "000000000000000000000000");

   first = BCON_NEW(
      "foo", "bar",
      "bar", BCON_INT32(12341),
      "baz", BCON_DOUBLE(123.456),
      "map", "{",
         "a", BCON_INT32(1),
      "}",
      "array", "[", BCON_INT32(1), BCON_INT32(2), BCON_INT32(3), BCON_INT32(4), "]",
      "null", BCON_NULL,
      "boolean", BCON_BOOL(true),
      "oid", BCON_OID(&oid),
      "binary", BCON_BIN(BSON_SUBTYPE_BINARY, (const uint8_t *)"deadbeef", 8),
      "regex", BCON_REGEX("foo|bar", "ism"),
      "date", BCON_DATE_TIME(10000),
      "ref", BCON_DBPOINTER("foo", &oid),
      "undefined", BCON_UNDEFINED,
      "minkey", BCON_MINKEY,
      "maxkey", BCON_MAXKEY,
      "timestamp", BCON_TIMESTAMP(100, 1000)
   );

   second = BCON_NEW("after", "b");
   third = BCON_NEW("twice", BCON_BOOL(true));

   _test_bson_json_read_compare(json, 5, first, second, third, NULL);
}

static void
test_bson_json_error (const char              *json,
                      int                      domain,
                      bson_json_error_code_t   code)
{
   bson_error_t error;
   bson_t * bson;

   bson = bson_new_from_json ((const uint8_t *)json, strlen(json), &error);

   assert (! bson);
   assert (error.domain == domain);
   assert (error.code == code);
}

static void
test_bson_json_read_missing_complex(void)
{
   const char * json = "{ \n\
      \"foo\" : { \n\
         \"$options\" : \"ism\"\n\
      }\n\
   }";

   test_bson_json_error (json, BSON_ERROR_JSON,
                         BSON_JSON_ERROR_READ_INVALID_PARAM);
}

static void
test_bson_json_read_invalid_json(void)
{
   const char * json = "{ \n\
      \"foo\" : { \n\
   }";

   test_bson_json_error (json, BSON_ERROR_JSON,
                         BSON_JSON_ERROR_READ_CORRUPT_JS);
}

static ssize_t
test_bson_json_read_bad_cb_helper(void *_ctx, uint8_t * buf, size_t len)
{
   return -1;
}

static void
test_bson_json_read_bad_cb(void)
{
   bson_error_t error;
   bson_json_reader_t *reader;
   int r;
   bson_t bson = BSON_INITIALIZER;

   reader = bson_json_reader_new (NULL, &test_bson_json_read_bad_cb_helper, NULL, false, 0);

   r = bson_json_reader_read (reader, &bson, &error);

   assert(r == -1);
   assert(error.domain = BSON_ERROR_JSON);
   assert(error.code = BSON_JSON_ERROR_READ_CB_FAILURE);

   bson_json_reader_destroy (reader);
   bson_destroy (&bson);
}

void
test_json_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/as_json/x1000", test_bson_as_json);
   TestSuite_Add (suite, "/bson/as_json/string", test_bson_as_json_string);
   TestSuite_Add (suite, "/bson/as_json/int32", test_bson_as_json_int32);
   TestSuite_Add (suite, "/bson/as_json/int64", test_bson_as_json_int64);
   TestSuite_Add (suite, "/bson/as_json/double", test_bson_as_json_double);
   TestSuite_Add (suite, "/bson/as_json/utf8", test_bson_as_json_utf8);
   TestSuite_Add (suite, "/bson/as_json/stack_overflow", test_bson_as_json_stack_overflow);
   TestSuite_Add (suite, "/bson/as_json/corrupt", test_bson_corrupt);
   TestSuite_Add (suite, "/bson/as_json/corrupt_utf8", test_bson_corrupt_utf8);
   TestSuite_Add (suite, "/bson/as_json/corrupt_binary", test_bson_corrupt_binary);
   TestSuite_Add (suite, "/bson/json/read", test_bson_json_read);
   TestSuite_Add (suite, "/bson/json/read/missing_complex", test_bson_json_read_missing_complex);
   TestSuite_Add (suite, "/bson/json/read/invalid_json", test_bson_json_read_invalid_json);
   TestSuite_Add (suite, "/bson/json/read/bad_cb", test_bson_json_read_bad_cb);
}
