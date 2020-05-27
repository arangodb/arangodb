// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/mean.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <boost/histogram/accumulators/sum.hpp>
#include <boost/histogram/accumulators/thread_safe.hpp>
#include <boost/histogram/accumulators/weighted_mean.hpp>
#include <boost/histogram/accumulators/weighted_sum.hpp>
#include "throw_exception.hpp"
#include <sstream>
#include "is_close.hpp"

using namespace boost::histogram;
using namespace std::literals;

template <class T>
auto str(const T& t) {
  std::ostringstream os;
  os << t;
  return os.str();
}

int main() {
  {
    using w_t = accumulators::weighted_sum<double>;
    w_t w;
    BOOST_TEST_EQ(str(w), "weighted_sum(0, 0)"s);

    BOOST_TEST_EQ(w, w_t(0));
    BOOST_TEST_NE(w, w_t(1));
    w = w_t(1);
    BOOST_TEST_EQ(w.value(), 1);
    BOOST_TEST_EQ(w.variance(), 1);
    BOOST_TEST_EQ(w, 1);
    BOOST_TEST_NE(w, 2);

    w += 2;
    BOOST_TEST_EQ(w.value(), 3);
    BOOST_TEST_EQ(w.variance(), 5);
    BOOST_TEST_EQ(w, w_t(3, 5));
    BOOST_TEST_NE(w, w_t(3));

    w += w_t(1, 2);
    BOOST_TEST_EQ(w.value(), 4);
    BOOST_TEST_EQ(w.variance(), 7);

    // consistency: a weighted counter increased by weight 1 multiplied
    // by 2 must be the same as a weighted counter increased by weight 2
    w_t u(0);
    ++u;
    u *= 2;
    BOOST_TEST_EQ(u, w_t(2, 4));

    w_t v(0);
    v += 2;
    BOOST_TEST_EQ(u, v);

    // conversion to RealType
    w_t y(1, 2);
    BOOST_TEST_NE(y, 1);
    BOOST_TEST_EQ(static_cast<double>(y), 1);
  }

  {
    using m_t = accumulators::mean<double>;
    m_t a;
    BOOST_TEST_EQ(a.count(), 0);

    a(4);
    a(7);
    a(13);
    a(16);

    BOOST_TEST_EQ(a.count(), 4);
    BOOST_TEST_EQ(a.value(), 10);
    BOOST_TEST_EQ(a.variance(), 30);

    BOOST_TEST_EQ(str(a), "mean(4, 10, 30)"s);

    m_t b;
    b(1e8 + 4);
    b(1e8 + 7);
    b(1e8 + 13);
    b(1e8 + 16);

    BOOST_TEST_EQ(b.count(), 4);
    BOOST_TEST_EQ(b.value(), 1e8 + 10);
    BOOST_TEST_EQ(b.variance(), 30);

    auto c = a;
    c += a; // same as feeding all samples twice

    BOOST_TEST_EQ(c.count(), 8);
    BOOST_TEST_EQ(c.value(), 10);
    BOOST_TEST_IS_CLOSE(c.variance(), 25.714, 1e-3);
  }

  {
    using m_t = accumulators::weighted_mean<double>;
    m_t a;
    BOOST_TEST_EQ(a.sum_of_weights(), 0);

    a(0.5, 1);
    a(1.0, 2);
    a(0.5, 3);

    BOOST_TEST_EQ(a.sum_of_weights(), 2);
    BOOST_TEST_EQ(a.value(), 2);
    BOOST_TEST_IS_CLOSE(a.variance(), 0.8, 1e-3);

    BOOST_TEST_EQ(str(a), "weighted_mean(2, 2, 0.8)"s);

    auto b = a;
    b += a; // same as feeding all samples twice

    BOOST_TEST_EQ(b.sum_of_weights(), 4);
    BOOST_TEST_EQ(b.value(), 2);
    BOOST_TEST_IS_CLOSE(b.variance(), 0.615, 1e-3);
  }

  {
    double bad_sum = 0;
    bad_sum += 1;
    bad_sum += 1e100;
    bad_sum += 1;
    bad_sum += -1e100;
    BOOST_TEST_EQ(bad_sum, 0); // instead of 2

    accumulators::sum<double> sum;
    ++sum;
    BOOST_TEST_EQ(sum.large(), 1);
    BOOST_TEST_EQ(sum.small(), 0);
    BOOST_TEST_EQ(str(sum), "sum(1 + 0)"s);
    sum += 1e100;
    BOOST_TEST_EQ(str(sum), "sum(1e+100 + 1)"s);
    ++sum;
    BOOST_TEST_EQ(str(sum), "sum(1e+100 + 2)"s);
    sum += -1e100;
    BOOST_TEST_EQ(str(sum), "sum(0 + 2)"s);
    BOOST_TEST_EQ(sum, 2); // correct answer
    BOOST_TEST_EQ(sum.large(), 0);
    BOOST_TEST_EQ(sum.small(), 2);

    accumulators::sum<double> a(3), b(2), c(3);
    BOOST_TEST_LT(b, c);
    BOOST_TEST_LE(b, c);
    BOOST_TEST_LE(a, c);
    BOOST_TEST_GT(a, b);
    BOOST_TEST_GE(a, b);
    BOOST_TEST_GE(a, c);
  }

  {
    accumulators::weighted_sum<accumulators::sum<double>> w;

    ++w;
    w += 1e100;
    ++w;
    w += -1e100;

    BOOST_TEST_EQ(w.value(), 2);
    BOOST_TEST_EQ(w.variance(), 2e200);
  }

  {
    accumulators::thread_safe<int> i;
    ++i;
    i += 1000;

    BOOST_TEST_EQ(i, 1001);
    BOOST_TEST_EQ(str(i), "1001"s);
  }

  return boost::report_errors();
}
