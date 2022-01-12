// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <boost/histogram/accumulators/weighted_mean.hpp>
#include <boost/histogram/weight.hpp>
#include <sstream>
#include "is_close.hpp"
#include "throw_exception.hpp"
#include "utility_str.hpp"

using namespace boost::histogram;
using namespace std::literals;

int main() {
  using m_t = accumulators::weighted_mean<double>;
  using detail::square;

  // basic interface, string conversion
  {
    // see https://en.wikipedia.org/wiki/Weighted_arithmetic_mean#Reliability_weights

    m_t a;
    BOOST_TEST_EQ(a.sum_of_weights(), 0);
    BOOST_TEST_EQ(a, m_t{});

    a(weight(0.5), 1);
    a(weight(1.0), 2);
    a(weight(0.5), 3);

    BOOST_TEST_EQ(a.sum_of_weights(), 1 + 2 * 0.5);
    BOOST_TEST_EQ(a.sum_of_weights_squared(), 1 + 2 * 0.5 * 0.5);
    // https://en.wikipedia.org/wiki/Effective_sample_size#weighted_samples
    BOOST_TEST_EQ(a.count(), square(a.sum_of_weights()) / a.sum_of_weights_squared());
    BOOST_TEST_EQ(a.value(), (0.5 * 1 + 1.0 * 2 + 0.5 * 3) / a.sum_of_weights());
    const auto m = a.value();
    BOOST_TEST_IS_CLOSE(
        a.variance(),
        (0.5 * square(1 - m) + square(2 - m) + 0.5 * square(3 - m)) /
            (a.sum_of_weights() - a.sum_of_weights_squared() / a.sum_of_weights()),
        1e-3);

    BOOST_TEST_EQ(str(a), "weighted_mean(2, 2, 0.8)"s);
    BOOST_TEST_EQ(str(a, 25, false), " weighted_mean(2, 2, 0.8)"s);
    BOOST_TEST_EQ(str(a, 25, true), "weighted_mean(2, 2, 0.8) "s);
  }

  // addition of zero element
  {
    BOOST_TEST_EQ(m_t() += m_t(), m_t());
    BOOST_TEST_EQ(m_t(1, 2, 3, 4) += m_t(), m_t(1, 2, 3, 4));
    BOOST_TEST_EQ(m_t() += m_t(1, 2, 3, 4), m_t(1, 2, 3, 4));
  }

  // addition
  {
    m_t a, b, c;

    a(weight(4), 2);
    a(weight(3), 3);
    BOOST_TEST_EQ(a.sum_of_weights(), 4 + 3);
    BOOST_TEST_EQ(a.sum_of_weights_squared(), 4 * 4 + 3 * 3);
    BOOST_TEST_EQ(a.value(), (4 * 2 + 3 * 3) / 7.);
    BOOST_TEST_IS_CLOSE(a.variance(), 0.5, 1e-3);

    b(weight(2), 4);
    b(weight(1), 6);
    BOOST_TEST_EQ(b.sum_of_weights(), 3);
    BOOST_TEST_EQ(b.sum_of_weights_squared(), 1 + 2 * 2);
    BOOST_TEST_EQ(b.value(), (2 * 4 + 1 * 6) / (2. + 1.));
    BOOST_TEST_IS_CLOSE(b.variance(), 2, 1e-3);

    c(weight(4), 2);
    c(weight(3), 3);
    c(weight(2), 4);
    c(weight(1), 6);

    auto d = a;
    d += b;
    BOOST_TEST_EQ(c.sum_of_weights(), d.sum_of_weights());
    BOOST_TEST_EQ(c.sum_of_weights_squared(), d.sum_of_weights_squared());
    BOOST_TEST_EQ(c.value(), d.value());
    BOOST_TEST_IS_CLOSE(c.variance(), d.variance(), 1e-3);
  }

  // using weights * 2 compared to adding weighted samples twice must
  // - give same for sum_of_weights and mean
  // - give twice sum_of_weights_squared
  // - give half effective count
  // - variance is complicated, but larger
  {
    m_t a, b;

    for (int i = 0; i < 2; ++i) {
      a(weight(0.5), 1);
      a(weight(1.0), 2);
      a(weight(0.5), 3);
    }

    b(weight(1), 1);
    b(weight(2), 2);
    b(weight(1), 3);

    BOOST_TEST_EQ(a.sum_of_weights(), b.sum_of_weights());
    BOOST_TEST_EQ(2 * a.sum_of_weights_squared(), b.sum_of_weights_squared());
    BOOST_TEST_EQ(a.count(), 2 * b.count());
    BOOST_TEST_EQ(a.value(), b.value());
    BOOST_TEST_LT(a.variance(), b.variance());
  }

  return boost::report_errors();
}
