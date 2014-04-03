#include <bcon.h>

#include "bson-tests.h"
#include "TestSuite.h"

static void
test_utf8 (void)
{
   const char *val;

   bson_t *bcon = BCON_NEW ("hello", "world");

   assert (BCON_EXTRACT (bcon, "hello", BCONE_UTF8 (val)));

   assert (strcmp (val, "world") == 0);

   bson_destroy (bcon);
}

static void
test_double (void)
{
   double val;

   bson_t *bcon = BCON_NEW ("foo", BCON_DOUBLE (1.1));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_DOUBLE (val)));

   assert (val == 1.1);

   bson_destroy (bcon);
}

static void
test_binary (void)
{
   bson_subtype_t subtype;
   uint32_t len;
   const uint8_t *binary;

   bson_t *bcon = BCON_NEW (
      "foo", BCON_BIN (
         BSON_SUBTYPE_BINARY,
         (uint8_t *)"deadbeef",
         8
         )
      );

   assert (BCON_EXTRACT (bcon, "foo", BCONE_BIN (subtype, binary, len)));

   assert (subtype == BSON_SUBTYPE_BINARY);
   assert (len == 8);
   assert (memcmp (binary, "deadbeef", 8) == 0);

   bson_destroy (bcon);
}


static void
test_undefined (void)
{
   bson_t *bcon = BCON_NEW ("foo", BCON_UNDEFINED);

   assert (BCON_EXTRACT (bcon, "foo", BCONE_UNDEFINED));

   bson_destroy (bcon);
}


static void
test_oid (void)
{
   bson_oid_t oid;
   bson_t *bcon;
   const bson_oid_t *ooid;

   bson_oid_init (&oid, NULL);

   bcon = BCON_NEW ("foo", BCON_OID (&oid));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_OID (ooid)));

   assert (bson_oid_equal (&oid, ooid));

   bson_destroy (bcon);
}

static void
test_bool (void)
{
   bool b;

   bson_t *bcon = BCON_NEW ("foo", BCON_BOOL (true));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_BOOL (b)));

   assert (b == true);

   bson_destroy (bcon);
}

static void
test_date_time (void)
{
   int64_t out;

   bson_t *bcon = BCON_NEW ("foo", BCON_DATE_TIME (10000));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_DATE_TIME (out)));

   assert (out == 10000);

   bson_destroy (bcon);
}


static void
test_null (void)
{
   bson_t *bcon = BCON_NEW ("foo", BCON_NULL);

   assert (BCON_EXTRACT (bcon, "foo", BCONE_NULL));

   bson_destroy (bcon);
}


static void
test_regex (void)
{
   const char *regex;
   const char *flags;

   bson_t *bcon = BCON_NEW ("foo", BCON_REGEX ("^foo|bar$", "i"));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_REGEX (regex, flags)));

   assert (strcmp (regex, "^foo|bar$") == 0);
   assert (strcmp (flags, "i") == 0);

   bson_destroy (bcon);
}


static void
test_dbpointer (void)
{
   const char *collection;
   bson_oid_t oid;
   const bson_oid_t *ooid;
   bson_t *bcon;

   bson_oid_init (&oid, NULL);

   bcon = BCON_NEW ("foo", BCON_DBPOINTER ("collection", &oid));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_DBPOINTER (collection, ooid)));

   assert (strcmp (collection, "collection") == 0);
   assert (bson_oid_equal (ooid, &oid));

   bson_destroy (bcon);
}


static void
test_code (void)
{
   const char *val;

   bson_t *bcon = BCON_NEW ("foo", BCON_CODE ("var a = {};"));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_CODE (val)));

   assert (strcmp (val, "var a = {};") == 0);

   bson_destroy (bcon);
}


static void
test_symbol (void)
{
   const char *val;

   bson_t *bcon = BCON_NEW ("foo", BCON_SYMBOL ("symbol"));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_SYMBOL (val)));

   assert (strcmp (val, "symbol") == 0);

   bson_destroy (bcon);
}


static void
test_codewscope (void)
{
   const char *code;
   bson_t oscope;

   bson_t *scope = BCON_NEW ("b", BCON_INT32 (10));
   bson_t *bcon = BCON_NEW ("foo", BCON_CODEWSCOPE ("var a = b;", scope));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_CODEWSCOPE (code, oscope)));

   assert (strcmp (code, "var a = b;") == 0);
   bson_eq_bson (&oscope, scope);

   bson_destroy (&oscope);
   bson_destroy (scope);
   bson_destroy (bcon);
}


static void
test_int32 (void)
{
   int32_t i32;

   bson_t *bcon = BCON_NEW ("foo", BCON_INT32 (10));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_INT32 (i32)));

   assert (i32 == 10);

   bson_destroy (bcon);
}


static void
test_timestamp (void)
{
   int32_t timestamp;
   int32_t increment;

   bson_t *bcon = BCON_NEW ("foo", BCON_TIMESTAMP (100, 1000));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_TIMESTAMP (timestamp, increment)));

   assert (timestamp == 100);
   assert (increment == 1000);

   bson_destroy (bcon);
}


static void
test_int64 (void)
{
   int64_t i64;

   bson_t *bcon = BCON_NEW ("foo", BCON_INT64 (10));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_INT64 (i64)));

   assert (i64 == 10);

   bson_destroy (bcon);
}


static void
test_maxkey (void)
{
   bson_t *bcon = BCON_NEW ("foo", BCON_MAXKEY);

   assert (BCON_EXTRACT (bcon, "foo", BCONE_MAXKEY));

   bson_destroy (bcon);
}


static void
test_minkey (void)
{
   bson_t *bcon = BCON_NEW ("foo", BCON_MINKEY);

   assert (BCON_EXTRACT (bcon, "foo", BCONE_MINKEY));

   bson_destroy (bcon);
}


static void
test_bson_document (void)
{
   bson_t ochild;
   bson_t *child = BCON_NEW ("bar", "baz");
   bson_t *bcon = BCON_NEW ("foo", BCON_DOCUMENT (child));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_DOCUMENT (ochild)));

   bson_eq_bson (&ochild, child);

   bson_destroy (&ochild);
   bson_destroy (child);
   bson_destroy (bcon);
}


static void
test_bson_array (void)
{
   bson_t ochild;
   bson_t *child = BCON_NEW ("0", "baz");
   bson_t *bcon = BCON_NEW ("foo", BCON_ARRAY (child));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_ARRAY (ochild)));

   bson_eq_bson (&ochild, child);

   bson_destroy (&ochild);
   bson_destroy (child);
   bson_destroy (bcon);
}


static void
test_inline_array (void)
{
   int32_t a, b;

   bson_t *bcon = BCON_NEW (
      "foo", "[",
         BCON_INT32 (1), BCON_INT32 (2),
      "]"
   );

   assert (BCON_EXTRACT (bcon,
      "foo", "[",
         BCONE_INT32 (a), BCONE_INT32 (b),
      "]"
   ));

   assert (a == 1);
   assert (b == 2);

   bson_destroy (bcon);
}

static void
test_inline_doc (void)
{
   int32_t a, b;

   bson_t *bcon = BCON_NEW (
      "foo", "{",
         "b", BCON_INT32 (2),
         "a", BCON_INT32 (1),
      "}"
   );

   assert (BCON_EXTRACT (bcon,
      "foo", "{",
         "a", BCONE_INT32 (a),
         "b", BCONE_INT32 (b),
      "}"
   ));

   assert (a == 1);
   assert (b == 2);

   bson_destroy (bcon);
}


static void
test_extract_ctx_helper (bson_t *bson,
                         ...)
{
   va_list ap;
   bcon_extract_ctx_t ctx;
   int i;
   int n;

   bcon_extract_ctx_init (&ctx);

   va_start (ap, bson);

   n = va_arg (ap, int);

   for (i = 0; i < n; i++) {
      assert (bcon_extract_ctx_va (bson, &ctx, &ap));
   }

   va_end (ap);
}

static void
test_extract_ctx (void)
{
   int32_t a, b, c;

   bson_t *bson = BCON_NEW (
      "a", BCON_INT32 (1),
      "b", BCON_INT32 (2),
      "c", BCON_INT32 (3)
      );

   test_extract_ctx_helper (bson, 3,
                            "a", BCONE_INT32 (a), NULL,
                            "b", BCONE_INT32 (b), NULL,
                            "c", BCONE_INT32 (c), NULL
                            );

   assert (a == 1);
   assert (b == 2);
   assert (c == 3);

   bson_destroy (bson);
}


static void
test_nested (void)
{
   const char *utf8;
   int i32;

   bson_t *bcon = BCON_NEW (
      "hello", "world",
      "foo", "{",
      "bar", BCON_INT32 (10),
      "}"
      );

   assert (BCON_EXTRACT (bcon,
                         "hello", BCONE_UTF8 (utf8),
                         "foo", "{",
                         "bar", BCONE_INT32 (i32),
                         "}"
                         ));

   assert (strcmp ("world", utf8) == 0);
   assert (i32 == 10);

   bson_destroy (bcon);
}


static void
test_skip (void)
{
   bson_t *bcon = BCON_NEW (
      "hello", "world",
      "foo", "{",
      "bar", BCON_INT32 (10),
      "}"
      );

   assert (BCON_EXTRACT (bcon,
                         "hello", BCONE_SKIP (BSON_TYPE_UTF8),
                         "foo", "{",
                         "bar", BCONE_SKIP (BSON_TYPE_INT32),
                         "}"
                         ));

   assert (!BCON_EXTRACT (bcon,
                          "hello", BCONE_SKIP (BSON_TYPE_UTF8),
                          "foo", "{",
                          "bar", BCONE_SKIP (BSON_TYPE_INT64),
                          "}"
                          ));

   bson_destroy (bcon);
}


static void
test_iter (void)
{
   bson_iter_t iter;
   bson_t *other;

   bson_t *bcon = BCON_NEW ("foo", BCON_INT32 (10));

   assert (BCON_EXTRACT (bcon, "foo", BCONE_ITER (iter)));

   assert (bson_iter_type (&iter) == BSON_TYPE_INT32);
   assert (bson_iter_int32 (&iter) == 10);

   other = BCON_NEW ("foo", BCON_ITER (&iter));

   bson_eq_bson (other, bcon);

   bson_destroy (bcon);
   bson_destroy (other);
}


void
test_bcon_extract_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/bcon/extract/test_utf8", test_utf8);
   TestSuite_Add (suite, "/bson/bcon/extract/test_double", test_double);
   TestSuite_Add (suite, "/bson/bcon/extract/test_binary", test_binary);
   TestSuite_Add (suite, "/bson/bcon/extract/test_undefined", test_undefined);
   TestSuite_Add (suite, "/bson/bcon/extract/test_oid", test_oid);
   TestSuite_Add (suite, "/bson/bcon/extract/test_bool", test_bool);
   TestSuite_Add (suite, "/bson/bcon/extract/test_date_time", test_date_time);
   TestSuite_Add (suite, "/bson/bcon/extract/test_null", test_null);
   TestSuite_Add (suite, "/bson/bcon/extract/test_regex", test_regex);
   TestSuite_Add (suite, "/bson/bcon/extract/test_dbpointer", test_dbpointer);
   TestSuite_Add (suite, "/bson/bcon/extract/test_code", test_code);
   TestSuite_Add (suite, "/bson/bcon/extract/test_symbol", test_symbol);
   TestSuite_Add (suite, "/bson/bcon/extract/test_codewscope", test_codewscope);
   TestSuite_Add (suite, "/bson/bcon/extract/test_int32", test_int32);
   TestSuite_Add (suite, "/bson/bcon/extract/test_timestamp", test_timestamp);
   TestSuite_Add (suite, "/bson/bcon/extract/test_int64", test_int64);
   TestSuite_Add (suite, "/bson/bcon/extract/test_maxkey", test_maxkey);
   TestSuite_Add (suite, "/bson/bcon/extract/test_minkey", test_minkey);
   TestSuite_Add (suite, "/bson/bcon/extract/test_bson_document", test_bson_document);
   TestSuite_Add (suite, "/bson/bcon/extract/test_bson_array", test_bson_array);
   TestSuite_Add (suite, "/bson/bcon/extract/test_inline_array", test_inline_array);
   TestSuite_Add (suite, "/bson/bcon/extract/test_inline_doc", test_inline_doc);
   TestSuite_Add (suite, "/bson/bcon/extract/test_extract_ctx", test_extract_ctx);
   TestSuite_Add (suite, "/bson/bcon/extract/test_nested", test_nested);
   TestSuite_Add (suite, "/bson/bcon/extract/test_skip", test_skip);
   TestSuite_Add (suite, "/bson/bcon/extract/test_iter", test_iter);
}
