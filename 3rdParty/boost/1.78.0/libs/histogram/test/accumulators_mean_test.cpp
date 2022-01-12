// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/mean.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <boost/histogram/weight.hpp>
#include <sstream>
#include "is_close.hpp"
#include "throw_exception.hpp"
#include "utility_str.hpp"

using namespace boost::histogram;
using namespace std::literals;

int main() {
  using m_t = accumulators::mean<double>;

  // basic interface, string conversion
  {
    m_t a;
    BOOST_TEST_EQ(a.count(), 0);
    BOOST_TEST_EQ(a, m_t{});

    a(4);
    a(7);
    a(13);
    a(16);

    BOOST_TEST_EQ(a.count(), 4);
    BOOST_TEST_EQ(a.value(), 10);
    BOOST_TEST_EQ(a.variance(), 30);

    BOOST_TEST_EQ(str(a), "mean(4, 10, 30)"s);
    BOOST_TEST_EQ(str(a, 20, false), "     mean(4, 10, 30)"s);
    BOOST_TEST_EQ(str(a, 20, true), "mean(4, 10, 30)     "s);
  }

  // small variation on large number
  {
    m_t a;
    a(1e8 + 4);
    a(1e8 + 7);
    a(1e8 + 13);
    a(1e8 + 16);

    BOOST_TEST_EQ(a.count(), 4);
    BOOST_TEST_EQ(a.value(), 1e8 + 10);
    BOOST_TEST_EQ(a.variance(), 30);
  }

  // addition of zero element
  {
    BOOST_TEST_EQ(m_t() += m_t(), m_t());
    BOOST_TEST_EQ(m_t(1, 2, 3) += m_t(), m_t(1, 2, 3));
    BOOST_TEST_EQ(m_t() += m_t(1, 2, 3), m_t(1, 2, 3));
  }

  // addition
  {
    m_t a, b, c;

    a(1);
    a(2);
    a(3);
    BOOST_TEST_EQ(a.count(), 3);
    BOOST_TEST_EQ(a.value(), 2);
    BOOST_TEST_EQ(a.variance(), 1);

    b(4);
    b(6);
    BOOST_TEST_EQ(b.count(), 2);
    BOOST_TEST_EQ(b.value(), 5);
    BOOST_TEST_EQ(b.variance(), 2);

    c(1);
    c(2);
    c(3);
    c(4);
    c(6);

    auto d = a;
    d += b;
    BOOST_TEST_EQ(c.count(), d.count());
    BOOST_TEST_EQ(c.value(), d.value());
    BOOST_TEST_IS_CLOSE(c.variance(), d.variance(), 1e-3);
  }

  // using weight=2 must be same as adding all samples twice
  {
    m_t a, b;

    for (int i = 0; i < 2; ++i) {
      a(4);
      a(7);
      a(13);
      a(16);
    }

    b(weight(2), 4);
    b(weight(2), 7);
    b(weight(2), 13);
    b(weight(2), 16);

    BOOST_TEST_EQ(a.count(), b.count());
    BOOST_TEST_EQ(a.value(), b.value());
    BOOST_TEST_IS_CLOSE(a.variance(), b.variance(), 1e-3);
  }

  return boost::report_errors();
}
