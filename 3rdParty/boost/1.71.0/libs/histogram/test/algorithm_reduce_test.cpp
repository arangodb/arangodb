// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/algorithm/reduce.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <boost/histogram/axis/category.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/axis/variable.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/unsafe_access.hpp>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;
using namespace boost::histogram::algorithm;

template <typename Tag>
void run_tests() {
  // reduce:
  // - does not work with arguments not convertible to double
  // - does not work with category axis, which is not ordered

  using regular = axis::regular<double, axis::transform::id, axis::null_type>;
  {
    auto h = make_s(Tag(), std::vector<int>(), regular(4, 1, 5), regular(3, -1, 2));

    // not allowed: invalid axis index
    BOOST_TEST_THROWS((void)reduce(h, slice(10, 2, 3)), std::invalid_argument);
    // not allowed: repeated indices
    BOOST_TEST_THROWS((void)reduce(h, slice(1, 0, 2), slice(1, 1, 3)),
                      std::invalid_argument);
    BOOST_TEST_THROWS((void)reduce(h, rebin(0, 2), rebin(0, 2)), std::invalid_argument);
    BOOST_TEST_THROWS((void)reduce(h, shrink(1, 0, 2), shrink(1, 0, 2)),
                      std::invalid_argument);
    // not allowed: slice with begin >= end
    BOOST_TEST_THROWS((void)reduce(h, slice(0, 1, 1)), std::invalid_argument);
    BOOST_TEST_THROWS((void)reduce(h, slice(0, 2, 1)), std::invalid_argument);
    // not allowed: shrink with lower == upper
    BOOST_TEST_THROWS((void)reduce(h, shrink(0, 0, 0)), std::invalid_argument);
    // not allowed: shrink axis to zero size
    BOOST_TEST_THROWS((void)reduce(h, shrink(0, 10, 11)), std::invalid_argument);
    // not allowed: rebin with zero merge
    BOOST_TEST_THROWS((void)reduce(h, rebin(0, 0)), std::invalid_argument);

    /*
      matrix layout:
      x ->
    y 1 0 1 0
    | 1 1 0 0
    v 0 2 1 3
    */
    h.at(0, 0) = 1;
    h.at(0, 1) = 1;
    h.at(1, 1) = 1;
    h.at(1, 2) = 2;
    h.at(2, 0) = 1;
    h.at(2, 2) = 1;
    h.at(3, 2) = 3;

    // should do nothing, index order does not matter
    auto hr = reduce(h, shrink(1, -1, 2), rebin(0, 1));
    BOOST_TEST_EQ(hr.rank(), 2);
    BOOST_TEST_EQ(sum(hr), 10);
    BOOST_TEST_EQ(hr.axis(0).size(), h.axis(0).size());
    BOOST_TEST_EQ(hr.axis(1).size(), h.axis(1).size());
    BOOST_TEST_EQ(hr.axis(0).bin(0).lower(), 1);
    BOOST_TEST_EQ(hr.axis(0).bin(3).upper(), 5);
    BOOST_TEST_EQ(hr.axis(1).bin(0).lower(), -1);
    BOOST_TEST_EQ(hr.axis(1).bin(2).upper(), 2);
    BOOST_TEST_EQ(hr, h);
    hr = reduce(h, slice(1, 0, 4), slice(0, 0, 4));
    BOOST_TEST_EQ(hr, h);

    hr = reduce(h, shrink(0, 2, 4));
    BOOST_TEST_EQ(hr.rank(), 2);
    BOOST_TEST_EQ(sum(hr), 10);
    BOOST_TEST_EQ(hr.axis(0).size(), 2);
    BOOST_TEST_EQ(hr.axis(1).size(), h.axis(1).size());
    BOOST_TEST_EQ(hr.axis(0).bin(0).lower(), 2);
    BOOST_TEST_EQ(hr.axis(0).bin(1).upper(), 4);
    BOOST_TEST_EQ(hr.axis(1).bin(0).lower(), -1);
    BOOST_TEST_EQ(hr.axis(1).bin(2).upper(), 2);
    BOOST_TEST_EQ(hr.at(-1, 0), 1); // underflow
    BOOST_TEST_EQ(hr.at(0, 0), 0);
    BOOST_TEST_EQ(hr.at(1, 0), 1);
    BOOST_TEST_EQ(hr.at(2, 0), 0); // overflow
    BOOST_TEST_EQ(hr.at(-1, 1), 1);
    BOOST_TEST_EQ(hr.at(0, 1), 1);
    BOOST_TEST_EQ(hr.at(1, 1), 0);
    BOOST_TEST_EQ(hr.at(2, 1), 0);
    BOOST_TEST_EQ(hr.at(-1, 2), 0);
    BOOST_TEST_EQ(hr.at(0, 2), 2);
    BOOST_TEST_EQ(hr.at(1, 2), 1);
    BOOST_TEST_EQ(hr.at(2, 2), 3);

    /*
      matrix layout:
      x ->
    y 1 0 1 0
    | 1 1 0 0
    v 0 2 1 3
    */

    hr = reduce(h, shrink_and_rebin(0, 2, 5, 2), rebin(1, 3));
    BOOST_TEST_EQ(hr.rank(), 2);
    BOOST_TEST_EQ(sum(hr), 10);
    BOOST_TEST_EQ(hr.axis(0).size(), 1);
    BOOST_TEST_EQ(hr.axis(1).size(), 1);
    BOOST_TEST_EQ(hr.axis(0).bin(0).lower(), 2);
    BOOST_TEST_EQ(hr.axis(0).bin(0).upper(), 4);
    BOOST_TEST_EQ(hr.axis(1).bin(0).lower(), -1);
    BOOST_TEST_EQ(hr.axis(1).bin(0).upper(), 2);
    BOOST_TEST_EQ(hr.at(-1, 0), 2); // underflow
    BOOST_TEST_EQ(hr.at(0, 0), 5);
    BOOST_TEST_EQ(hr.at(1, 0), 3); // overflow

    // test overload that accepts iterable and test option fusion
    std::vector<reduce_option> opts{{shrink(0, 2, 5), rebin(0, 2), rebin(1, 3)}};
    auto hr2 = reduce(h, opts);
    BOOST_TEST_EQ(hr2, hr);
    opts = {rebin(0, 2), slice(0, 1, 4), rebin(1, 3)};
    auto hr3 = reduce(h, opts);
    BOOST_TEST_EQ(hr3, hr);
  }

  // mixed axis types
  {
    axis::regular<> r(5, 0.0, 1.0);
    axis::variable<> v{{1., 2., 3.}};
    axis::category<int> c{{1, 2, 3}};
    auto h = make(Tag(), r, v, c);
    auto hr = algorithm::reduce(h, shrink(0, 0.2, 0.7));
    BOOST_TEST_EQ(hr.axis(0).size(), 3);
    BOOST_TEST_EQ(hr.axis(0).bin(0).lower(), 0.2);
    BOOST_TEST_EQ(hr.axis(0).bin(2).upper(), 0.8);
    BOOST_TEST_EQ(hr.axis(1).size(), 2);
    BOOST_TEST_EQ(hr.axis(1).bin(0).lower(), 1);
    BOOST_TEST_EQ(hr.axis(1).bin(1).upper(), 3);
    BOOST_TEST_THROWS((void)algorithm::reduce(h, rebin(2, 2)), std::invalid_argument);
  }

  // reduce on integer axis, rebin must fail
  {
    auto h = make(Tag(), axis::integer<>(1, 4));
    BOOST_TEST_THROWS((void)reduce(h, rebin(2)), std::invalid_argument);
    auto hr = reduce(h, shrink(2, 3));
    BOOST_TEST_EQ(hr.axis().size(), 1);
    BOOST_TEST_EQ(hr.axis().bin(0), 2);
    BOOST_TEST_EQ(hr.axis().bin(1), 3);
  }

  // reduce on circular axis, shrink must fail, also rebin with remainder
  {
    auto h = make(Tag(), axis::circular<>(4, 1, 4));
    BOOST_TEST_THROWS((void)reduce(h, shrink(0, 2)), std::invalid_argument);
    BOOST_TEST_THROWS((void)reduce(h, rebin(3)), std::invalid_argument);
    auto hr = reduce(h, rebin(2));
    BOOST_TEST_EQ(hr.axis().size(), 2);
    BOOST_TEST_EQ(hr.axis().bin(0).lower(), 1);
    BOOST_TEST_EQ(hr.axis().bin(1).upper(), 4);
  }

  // reduce on variable axis
  {
    auto h = make(Tag(), axis::variable<>({0, 1, 2, 3, 4, 5, 6}));
    auto hr = reduce(h, shrink_and_rebin(1, 5, 2));
    BOOST_TEST_EQ(hr.axis().size(), 2);
    BOOST_TEST_EQ(hr.axis().value(0), 1);
    BOOST_TEST_EQ(hr.axis().value(1), 3);
    BOOST_TEST_EQ(hr.axis().value(2), 5);
  }

  // reduce on axis with inverted range
  {
    auto h = make(Tag(), regular(4, 2, -2));
    auto hr = reduce(h, shrink(1, -1));
    BOOST_TEST_EQ(hr.axis().size(), 2);
    BOOST_TEST_EQ(hr.axis().bin(0).lower(), 1);
    BOOST_TEST_EQ(hr.axis().bin(1).upper(), -1);
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
