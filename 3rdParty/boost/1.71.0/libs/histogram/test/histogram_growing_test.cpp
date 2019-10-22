// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <string>
#include <utility>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"
#include "std_ostream.hpp"

using namespace boost::histogram;

using def = use_default;

using regular = axis::regular<double, def, def, axis::option::growth_t>;

using integer = axis::integer<double, def,
                              decltype(axis::option::underflow | axis::option::overflow |
                                       axis::option::growth)>;

using category = axis::category<std::string, def, axis::option::growth_t>;

class custom_2d_axis {
public:
  auto index(std::tuple<double, double> xy) const {
    const auto x = std::get<0>(xy);
    const auto y = std::get<1>(xy);
    const auto r = std::sqrt(x * x + y * y);
    return std::min(static_cast<axis::index_type>(r), size());
  }

  auto update(std::tuple<double, double> xy) {
    const auto x = std::get<0>(xy);
    const auto y = std::get<1>(xy);
    const auto r = std::sqrt(x * x + y * y);
    const auto n = static_cast<int>(r);
    const auto old = size_;
    if (n >= size_) size_ = n + 1;
    return std::make_pair(n, old - size_);
  }

  axis::index_type size() const { return size_; }

private:
  axis::index_type size_ = 0;
};

template <typename Tag>
void run_tests() {
  {
    auto h = make(Tag(), regular(2, 0, 1));
    const auto& a = h.axis();
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(h.size(), 2);
    // [0.0, 0.5, 1.0]
    h(0.1);
    h(0.9);
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(h.size(), 2);
    h(-std::numeric_limits<double>::infinity());
    h(std::numeric_limits<double>::quiet_NaN());
    h(std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(h.size(), 2);
    h(-0.3);
    // [-0.5, 0.0, 0.5, 1.0]
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(h.size(), 3);
    BOOST_TEST_EQ(h[0], 1);
    BOOST_TEST_EQ(h[1], 1);
    BOOST_TEST_EQ(h[2], 1);
    h(1.9);
    // [-0.5, 0.0, 0.5, 1.0, 1.5, 2.0]
    BOOST_TEST_EQ(a.size(), 5);
    BOOST_TEST_EQ(h.size(), 5);
    BOOST_TEST_EQ(h[0], 1);
    BOOST_TEST_EQ(h[1], 1);
    BOOST_TEST_EQ(h[2], 1);
    BOOST_TEST_EQ(h[3], 0);
    BOOST_TEST_EQ(h[4], 1);
  }

  {
    auto h = make_s(Tag(), std::vector<int>(), integer());
    const auto& a = h.axis();
    h(-std::numeric_limits<double>::infinity());
    h(std::numeric_limits<double>::quiet_NaN());
    h(std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.size(), 0);
    BOOST_TEST_EQ(h.size(), 2);
    BOOST_TEST_EQ(h[-1], 1);
    BOOST_TEST_EQ(h[0], 2);
    h(0);
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(h.size(), 3);
    BOOST_TEST_EQ(h[-1], 1);
    BOOST_TEST_EQ(h[0], 1);
    BOOST_TEST_EQ(h[1], 2);
    h(2);
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(h.size(), 5);
    BOOST_TEST_EQ(h[-1], 1);
    BOOST_TEST_EQ(h[0], 1);
    BOOST_TEST_EQ(h[1], 0);
    BOOST_TEST_EQ(h[2], 1);
    BOOST_TEST_EQ(h[3], 2);
    h(-2);
    BOOST_TEST_EQ(a.size(), 5);
    BOOST_TEST_EQ(h.size(), 7);
    // BOOST_TEST_EQ(h[-1], 1)
    BOOST_TEST_EQ(h[0], 1);
    BOOST_TEST_EQ(h[1], 0);
    BOOST_TEST_EQ(h[2], 1);
    BOOST_TEST_EQ(h[3], 0);
    BOOST_TEST_EQ(h[4], 1);
    BOOST_TEST_EQ(h[5], 2);
  }

  {
    auto h = make_s(Tag(), std::vector<int>(), integer(), category());
    const auto& a = h.axis(0);
    const auto& b = h.axis(1);
    BOOST_TEST_EQ(a.size(), 0);
    BOOST_TEST_EQ(b.size(), 0);
    BOOST_TEST_EQ(h.size(), 0);
    h(0, "x");
    h(-std::numeric_limits<double>::infinity(), "x");
    h(std::numeric_limits<double>::infinity(), "x");
    h(std::numeric_limits<double>::quiet_NaN(), "x");
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(b.size(), 1);
    BOOST_TEST_EQ(h.size(), 3);
    h(2, "x");
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(b.size(), 1);
    BOOST_TEST_EQ(h.size(), 5);
    h(1, "y");
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(b.size(), 2);
    BOOST_TEST_EQ(h.size(), 10);
    BOOST_TEST_EQ(h.at(-1, 0), 1);
    BOOST_TEST_EQ(h.at(-1, 1), 0);
    BOOST_TEST_EQ(h.at(3, 0), 2);
    BOOST_TEST_EQ(h.at(3, 1), 0);
    BOOST_TEST_EQ(h.at(a.index(0), b.index("x")), 1);
    BOOST_TEST_EQ(h.at(a.index(1), b.index("x")), 0);
    BOOST_TEST_EQ(h.at(a.index(2), b.index("x")), 1);
    BOOST_TEST_EQ(h.at(a.index(0), b.index("y")), 0);
    BOOST_TEST_EQ(h.at(a.index(1), b.index("y")), 1);
    BOOST_TEST_EQ(h.at(a.index(2), b.index("y")), 0);

    BOOST_TEST_THROWS(h(0, "x", 42), std::invalid_argument);
  }

  {
    auto h = make_s(Tag{}, std::vector<int>{}, custom_2d_axis{});
    BOOST_TEST_EQ(h.size(), 0);
    h(0, 0);
    BOOST_TEST_EQ(h.size(), 1);
    h(1, 0);
    h(0, 1);
    BOOST_TEST_EQ(h.size(), 2);
    h(10, 0);
    BOOST_TEST_EQ(h.size(), 11);
    BOOST_TEST_EQ(h[0], 1);
    BOOST_TEST_EQ(h[1], 2);
    BOOST_TEST_EQ(h[10], 1);
    BOOST_TEST_THROWS(h(0), std::invalid_argument);
  }

  // mix of a growing and a non-growing axis
  {
    using reg_nogrow = axis::regular<>;
    auto h = make(Tag(), reg_nogrow{2, 0.0, 1.0}, regular{2, 0.0, 1.0});
    BOOST_TEST_EQ(h.size(), 4 * 2);
    h(0.0, 0.0);
    BOOST_TEST_EQ(h.size(), 4 * 2);
    BOOST_TEST_EQ(h.at(0, 0), 1);
    h(-1.0, -0.1);
    BOOST_TEST_EQ(h.size(), 4 * 3);
    BOOST_TEST_EQ(h.at(-1, 0), 1);
    h(2.0, 1.1);
    BOOST_TEST_EQ(h.size(), 4 * 4);
    // axis 0: [0.0, 0.5, 1.0] + under/overflow
    // axis 1: [-0.5, 0.0, 0.5, 1.0, 1.5]
    BOOST_TEST_EQ(h.at(-1, 0), 1);
    BOOST_TEST_EQ(h.at(0, 1), 1);
    BOOST_TEST_EQ(h.at(2, 3), 1);
    BOOST_TEST_EQ(algorithm::sum(h), 3);
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
