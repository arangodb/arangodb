// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/accumulators/weighted_sum.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <tuple>
#include <type_traits>
#include <vector>
#include "std_ostream.hpp"
#include "throw_exception.hpp"

using namespace boost::histogram;
namespace tr = axis::transform;

// tests requires a C++17 compatible compiler

int main() {
  using axis::null_type;

  {
    using axis::regular;
    BOOST_TEST_TRAIT_SAME(decltype(regular(1, 0.0, 1.0)),
                          regular<double, tr::id, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(regular(1, 0, 1)), regular<double, tr::id, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(regular(1, 0.0f, 1.0f)),
                          regular<float, tr::id, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(regular(1, 0, 1, 0)), regular<double, tr::id, int>);
    BOOST_TEST_TRAIT_SAME(decltype(regular(1, 0.0f, 1.0f, "x")),
                          regular<float, tr::id, std::string>);
    BOOST_TEST_TRAIT_SAME(decltype(regular(tr::sqrt(), 1, 0, 1)),
                          regular<double, tr::sqrt, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(regular(tr::sqrt(), 1, 0.0f, 1.0f, "x")),
                          regular<float, tr::sqrt, std::string>);
    BOOST_TEST_TRAIT_SAME(decltype(regular(tr::sqrt(), 1, 0, 1, 0)),
                          regular<double, tr::sqrt, int>);
  }

  {
    using axis::integer;
    BOOST_TEST_TRAIT_SAME(decltype(integer(1, 2)), integer<int, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(integer(1l, 2l)), integer<int, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(integer(1.0, 2.0)), integer<double, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(integer(1.0f, 2.0f)), integer<float, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(integer(1, 2, "foo")), integer<int, std::string>);
    BOOST_TEST_TRAIT_SAME(decltype(integer(1, 2, 0)), integer<int, int>);
  }

  {
    using axis::variable;
    BOOST_TEST_TRAIT_SAME(decltype(variable{-1.0f, 1.0f}), variable<float, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(variable{-1, 0, 1, 2}), variable<double, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(variable{-1.0, 1.0}), variable<double, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(variable({-1, 0, 1}, "foo")),
                          variable<double, std::string>);
    BOOST_TEST_TRAIT_SAME(decltype(variable({-1, 1}, 0)), variable<double, int>);

    BOOST_TEST_TRAIT_SAME(decltype(variable(std::vector<int>{{-1, 1}})),
                          variable<double, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(variable(std::vector<float>{{-1.0f, 1.0f}})),
                          variable<float, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(variable(std::vector<int>{{-1, 1}}, "foo")),
                          variable<double, std::string>);
    BOOST_TEST_TRAIT_SAME(decltype(variable(std::vector<int>{{-1, 1}}, 0)),
                          variable<double, int>);
  }

  {
    using axis::category;
    BOOST_TEST_TRAIT_SAME(decltype(category{1, 2}), category<int, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(category{"x", "y"}), category<std::string, null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(category({1, 2}, "foo")), category<int, std::string>);
    BOOST_TEST_TRAIT_SAME(decltype(category({1, 2}, 0)), category<int, int>);
  }

  {
    using axis::boolean;
    BOOST_TEST_TRAIT_SAME(decltype(boolean{}), boolean<null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(boolean{"foo"}), boolean<std::string>);
  }

  {
    auto h = histogram(axis::regular(3, -1, 1), axis::integer(0, 4));
    BOOST_TEST_TRAIT_SAME(decltype(h),
                          histogram<std::tuple<axis::regular<double, tr::id, null_type>,
                                               axis::integer<int, null_type>>>);
    BOOST_TEST_EQ(h.axis(0), axis::regular(3, -1, 1));
    BOOST_TEST_EQ(h.axis(1), axis::integer(0, 4));
  }

  {
    auto h = histogram(std::tuple(axis::regular(3, -1, 1), axis::integer(0, 4)),
                       weight_storage());
    BOOST_TEST_TRAIT_SAME(decltype(h),
                          histogram<std::tuple<axis::regular<double, tr::id, null_type>,
                                               axis::integer<int, null_type>>,
                                    weight_storage>);
    BOOST_TEST_EQ(h.axis(0), axis::regular(3, -1, 1));
    BOOST_TEST_EQ(h.axis(1), axis::integer(0, 4));
  }

  {
    auto a0 = axis::regular(5, 0, 5);
    auto a1 = axis::regular(3, -1, 1);
    auto axes = {a0, a1};
    auto h = histogram(axes);
    BOOST_TEST_TRAIT_SAME(
        decltype(h), histogram<std::vector<axis::regular<double, tr::id, null_type>>>);
    BOOST_TEST_EQ(h.rank(), 2);
    BOOST_TEST_EQ(h.axis(0), a0);
    BOOST_TEST_EQ(h.axis(1), a1);
  }

  {
    auto a0 = axis::regular(5, 0, 5);
    auto a1 = axis::regular(3, -1, 1);
    auto axes = {a0, a1};
    auto h = histogram(axes, weight_storage());
    BOOST_TEST_TRAIT_SAME(
        decltype(h),
        histogram<std::vector<axis::regular<double, tr::id, null_type>>, weight_storage>);
    BOOST_TEST_EQ(h.rank(), 2);
    BOOST_TEST_EQ(h.axis(0), a0);
    BOOST_TEST_EQ(h.axis(1), a1);
  }

  {
    auto a0 = axis::regular(5, 0, 5);
    auto a1 = axis::regular(3, -1, 1);
    auto axes = std::vector<decltype(a0)>{{a0, a1}};
    auto h = histogram(axes);
    BOOST_TEST_TRAIT_SAME(
        decltype(h), histogram<std::vector<axis::regular<double, tr::id, null_type>>>);
    BOOST_TEST_EQ(h.rank(), 2);
    BOOST_TEST_EQ(h.axis(0), a0);
    BOOST_TEST_EQ(h.axis(1), a1);
  }

  return boost::report_errors();
}
