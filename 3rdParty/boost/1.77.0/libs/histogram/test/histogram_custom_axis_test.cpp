// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/mean.hpp>
#include <boost/histogram/accumulators/weighted_mean.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;

struct modified_axis : public axis::integer<> {
  using integer::integer; // inherit ctors of base
  // customization point: convert argument and call base class
  auto index(const char* s) const { return integer::index(std::atoi(s)); }
};

struct minimal {
  auto index(int x) const { return static_cast<axis::index_type>(x % 2); }
  auto size() const { return axis::index_type{2}; }
};

struct axis2d {
  auto index(const std::tuple<double, double>& x) const {
    return axis::index_type{std::get<0>(x) == 1 && std::get<1>(x) == 2};
  }
  auto size() const { return axis::index_type{2}; }
};

class axis2d_growing {
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

template <class Tag>
void run_tests() {
  // one 2d axis
  {
    auto h = make(Tag(), axis2d());
    h(1, 2);                  // ok, forwards 2d tuple to axis
    h(std::make_tuple(1, 2)); // also ok, forwards 2d tuple to axis
    BOOST_TEST_THROWS(h(1), std::invalid_argument);
    BOOST_TEST_THROWS(h(1, 2, 3), std::invalid_argument);
    BOOST_TEST_EQ(h.at(0), 0); // ok, bin access is still 1d
    BOOST_TEST_EQ(h[std::make_tuple(1)], 2);
    // also works with weights
    h(1, 2, weight(2));
    h(std::make_tuple(weight(3), 1, 2));
    BOOST_TEST_EQ(h.at(0), 0);
    BOOST_TEST_EQ(h.at(1), 7);

    auto h2 = make_s(Tag(), profile_storage(), axis2d());
    h2(1, 2, sample(2));
    BOOST_TEST_EQ(h2[1].count(), 1);
    BOOST_TEST_EQ(h2[1].value(), 2);

    auto h3 = make_s(Tag(), weighted_profile_storage(), axis2d());
    h3(1, 2, weight(3), sample(2));
    BOOST_TEST_EQ(h3[1].sum_of_weights(), 3);
    BOOST_TEST_EQ(h3[1].value(), 2);
  }

  // several axes, one 2d
  {
    auto h = make(Tag(), modified_axis(0, 3), minimal(), axis2d());
    h("0", 1, std::make_tuple(1.0, 2.0));
    h("1", 2, std::make_tuple(2.0, 1.0));

    BOOST_TEST_EQ(h.rank(), 3);
    BOOST_TEST_EQ(h.at(0, 0, 0), 0);
    BOOST_TEST_EQ(h.at(0, 1, 1), 1);
    BOOST_TEST_EQ(h.at(1, 0, 0), 1);
  }

  // growing axis
  {
    auto h = make_s(Tag{}, std::vector<int>{}, axis2d_growing{});
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
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();
  return boost::report_errors();
}
