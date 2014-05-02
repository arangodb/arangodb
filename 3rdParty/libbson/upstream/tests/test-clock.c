#include <bson.h>
#include <assert.h>

#include "TestSuite.h"
#include "bson-tests.h"


static void
test_get_monotonic_time (void)
{
   int64_t t;
   int64_t t2;

   t = bson_get_monotonic_time();
   t2 = bson_get_monotonic_time();
   assert(t);
   assert(t2);
   assert_cmpint(t, <=, t2);
}


void
test_clock_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/clock/get_monotonic_time", test_get_monotonic_time);
}
