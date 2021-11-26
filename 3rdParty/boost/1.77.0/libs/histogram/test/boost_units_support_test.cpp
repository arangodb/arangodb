// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/regular.hpp>
#include "throw_exception.hpp"
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/indexed.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/units/quantity.hpp>
#include <boost/units/systems/si/length.hpp>
#include <limits>
#include "is_close.hpp"

using namespace boost::histogram;
using namespace boost::histogram::literals;
namespace tr = axis::transform;

int main() {
  using namespace boost::units;
  using Q = quantity<si::length>;

  // axis::regular with quantity
  {
    axis::regular<Q> b(2, 0 * si::meter, 2 * si::meter);
    BOOST_TEST_EQ(b.bin(-1).lower() / si::meter,
                  -std::numeric_limits<double>::infinity());
    BOOST_TEST_IS_CLOSE(b.bin(0).lower() / si::meter, 0.0, 1e-9);
    BOOST_TEST_IS_CLOSE(b.bin(1).lower() / si::meter, 1.0, 1e-9);
    BOOST_TEST_IS_CLOSE(b.bin(2).lower() / si::meter, 2.0, 1e-9);
    BOOST_TEST_EQ(b.bin(2).upper() / si::meter, std::numeric_limits<double>::infinity());

    BOOST_TEST_EQ(b.index(-std::numeric_limits<double>::infinity() * si::meter), -1);
    BOOST_TEST_EQ(b.index(-1 * si::meter), -1); // produces NaN in conversion
    BOOST_TEST_EQ(b.index(0 * si::meter), 0);
    BOOST_TEST_EQ(b.index(0.99 * si::meter), 0);
    BOOST_TEST_EQ(b.index(1 * si::meter), 1);
    BOOST_TEST_EQ(b.index(1.99 * si::meter), 1);
    BOOST_TEST_EQ(b.index(2 * si::meter), 2);
    BOOST_TEST_EQ(b.index(100 * si::meter), 2);
    BOOST_TEST_EQ(b.index(std::numeric_limits<double>::infinity() * si::meter), 2);
  }

  // axis::regular with quantity and transform
  {
    axis::regular<Q, tr::log> b(2, 1 * si::meter, 10 * si::meter);
    BOOST_TEST_EQ(b.value(-1) / si::meter, 0);
    BOOST_TEST_IS_CLOSE(b.value(0) / si::meter, 1.0, 1e-9);
    BOOST_TEST_IS_CLOSE(b.value(1) / si::meter, 3.1623, 1e-3);
    BOOST_TEST_IS_CLOSE(b.value(2) / si::meter, 10.0, 1e-9);
    BOOST_TEST_EQ(b.value(3) / si::meter, std::numeric_limits<double>::infinity());
  }

  // histogram with quantity axis
  {
    auto h = make_histogram(axis::regular<Q>(2, 0 * si::meter, 1 * si::meter),
                            axis::regular<>(2, 0, 1));
    h(0.1 * si::meter, 0.1); // fills bin (0, 0)
    BOOST_TEST_EQ(h.at(0, 0), 1);
    for (auto&& x : indexed(h)) {
      BOOST_TEST_THROWS(x.density(), std::runtime_error); // cannot use density method
      BOOST_TEST_EQ(x.index(0), 2.0 * x.bin(0_c).lower() / si::meter);
      BOOST_TEST_EQ(x.index(1), 2.0 * x.bin(1_c).lower());
    }
  }

  return boost::report_errors();
}
