#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "link test"
#include <boost/test/unit_test.hpp>

struct TestSetup {
  TestSetup() {
    BOOST_TEST_MESSAGE("test message");
  }
};

BOOST_FIXTURE_TEST_SUITE(Test, TestSetup)

BOOST_AUTO_TEST_CASE (test_Test1) {
}

BOOST_AUTO_TEST_SUITE_END()
