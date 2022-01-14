// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators.hpp>
#include <boost/histogram/serialization.hpp>
#include <boost/histogram/weight.hpp>
#include <cassert>
#include "throw_exception.hpp"
#include "utility_serialization.hpp"

using namespace boost::histogram;

int main(int argc, char** argv) {
  (void)argc;
  assert(argc == 2);

  // mean v0
  {
    const auto filename = join(argv[1], "accumulators_serialization_test_mean_v0.xml");
    accumulators::mean<> a;
    load_xml(filename, a);
    BOOST_TEST_EQ(a.count(), 3);
    BOOST_TEST_EQ(a.value(), 2);
    BOOST_TEST_EQ(a.variance(), 0.5);
  }

  // mean
  {
    const auto filename = join(argv[1], "accumulators_serialization_test_mean.xml");
    accumulators::mean<> a;
    a(1);
    a(weight(0.5), 2);
    a(3);
    print_xml(filename, a);

    accumulators::mean<> b;
    BOOST_TEST_NOT(a == b);
    load_xml(filename, b);
    BOOST_TEST(a == b);
  }

  // sum
  {
    const auto filename = join(argv[1], "accumulators_serialization_test_sum.xml");
    accumulators::sum<> a;
    a += 1e100;
    a += 1;
    print_xml(filename, a);

    accumulators::sum<> b;
    BOOST_TEST_NOT(a == b);
    load_xml(filename, b);
    BOOST_TEST(a == b);
  }

  // weighted_mean
  {
    const auto filename =
        join(argv[1], "accumulators_serialization_test_weighted_mean.xml");
    accumulators::weighted_mean<> a;
    a(1);
    a(weight(0.5), 2);
    a(3);
    print_xml(filename, a);

    accumulators::weighted_mean<> b;
    BOOST_TEST_NOT(a == b);
    load_xml(filename, b);
    BOOST_TEST(a == b);
  }

  // weighted_sum
  {
    const auto filename =
        join(argv[1], "accumulators_serialization_test_weighted_sum.xml");
    accumulators::weighted_sum<> a;
    a += weight(1);
    a += weight(10);
    print_xml(filename, a);

    accumulators::weighted_sum<> b;
    BOOST_TEST_NOT(a == b);
    load_xml(filename, b);
    BOOST_TEST(a == b);
  }

  return boost::report_errors();
}
