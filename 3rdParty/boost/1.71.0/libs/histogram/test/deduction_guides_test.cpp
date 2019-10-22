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
#include "throw_exception.hpp"
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <tuple>
#include <type_traits>
#include <vector>
#include "std_ostream.hpp"

using namespace boost::histogram;
namespace tr = axis::transform;

// tests requires a C++17 compatible compiler

int main() {
  {
    axis::regular a(1, 0.0, 1.0);
    axis::regular b(1, 0, 1);
    axis::regular c(1, 0.0f, 1.0f);
    axis::regular d(1, 0, 1, "foo");
    axis::regular e(1, 0, 1, axis::null_type{});
    axis::regular f(tr::sqrt(), 1, 0, 1);
    axis::regular g(tr::sqrt(), 1, 0, 1, "foo");
    axis::regular h(tr::sqrt(), 1, 0, 1, axis::null_type{});

    BOOST_TEST_TRAIT_SAME(decltype(a), axis::regular<>);
    BOOST_TEST_TRAIT_SAME(decltype(b), axis::regular<>);
    BOOST_TEST_TRAIT_SAME(decltype(c), axis::regular<float>);
    BOOST_TEST_TRAIT_SAME(decltype(d), axis::regular<>);
    BOOST_TEST_TRAIT_SAME(decltype(e), axis::regular<double, tr::id, axis::null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(f), axis::regular<double, tr::sqrt>);
    BOOST_TEST_TRAIT_SAME(decltype(g), axis::regular<double, tr::sqrt>);
    BOOST_TEST_TRAIT_SAME(decltype(h), axis::regular<double, tr::sqrt, axis::null_type>);
  }

  {
    axis::integer a(1, 2);
    axis::integer b(1l, 2l);
    axis::integer c(1.0, 2.0);
    axis::integer d(1.0f, 2.0f);
    axis::integer e(1, 2, "foo");
    axis::integer f(1, 2, axis::null_type{});

    BOOST_TEST_TRAIT_SAME(decltype(a), axis::integer<int>);
    BOOST_TEST_TRAIT_SAME(decltype(b), axis::integer<int>);
    BOOST_TEST_TRAIT_SAME(decltype(c), axis::integer<double>);
    BOOST_TEST_TRAIT_SAME(decltype(d), axis::integer<float>);
    BOOST_TEST_TRAIT_SAME(decltype(e), axis::integer<int>);
    BOOST_TEST_TRAIT_SAME(decltype(f), axis::integer<int, axis::null_type>);
  }

  {
    axis::variable a{-1, 1};
    axis::variable b{-1.f, 1.f};
    axis::variable c{-1.0, 1.0};
    axis::variable d({-1, 1}, "foo");
    axis::variable e({-1, 1}, axis::null_type{});

    std::vector<int> vi{{-1, 1}};
    std::vector<float> vf{{-1.f, 1.f}};
    axis::variable f(vi);
    axis::variable g(vf);
    axis::variable h(vi, "foo");
    axis::variable i(vi, axis::null_type{});

    BOOST_TEST_TRAIT_SAME(decltype(a), axis::variable<>);
    BOOST_TEST_TRAIT_SAME(decltype(b), axis::variable<float>);
    BOOST_TEST_TRAIT_SAME(decltype(c), axis::variable<>);
    BOOST_TEST_TRAIT_SAME(decltype(d), axis::variable<>);
    BOOST_TEST_TRAIT_SAME(decltype(e), axis::variable<double, axis::null_type>);
    BOOST_TEST_TRAIT_SAME(decltype(f), axis::variable<>);
    BOOST_TEST_TRAIT_SAME(decltype(g), axis::variable<float>);
    BOOST_TEST_TRAIT_SAME(decltype(h), axis::variable<>);
    BOOST_TEST_TRAIT_SAME(decltype(i), axis::variable<double, axis::null_type>);
  }

  {
    axis::category a{1, 2};
    axis::category b{"x", "y"};
    axis::category c({1, 2}, "foo");
    axis::category d({1, 2}, axis::null_type{});

    BOOST_TEST_TRAIT_SAME(decltype(a), axis::category<int>);
    BOOST_TEST_TRAIT_SAME(decltype(b), axis::category<std::string>);
    BOOST_TEST_TRAIT_SAME(decltype(c), axis::category<int>);
    BOOST_TEST_TRAIT_SAME(decltype(d), axis::category<int, axis::null_type>);
  }

  {
    auto a0 = axis::regular(3, -1, 1, axis::null_type{});
    auto a1 = axis::integer(0, 4, axis::null_type{});
    auto a = histogram(std::make_tuple(a0, a1));
    BOOST_TEST_EQ(a.rank(), 2);
    BOOST_TEST_EQ(a.axis(0), a0);
    BOOST_TEST_EQ(a.axis(1), a1);

    auto a2 = axis::regular(5, 0, 5, axis::null_type{});
    // don't use deduction guides for vector, support depends on stdc++ version
    std::vector<decltype(a0)> axes{{a0, a2}};
    auto b = histogram(axes, weight_storage());
    BOOST_TEST_EQ(b.rank(), 2);
    BOOST_TEST_EQ(b.axis(0), a0);
    BOOST_TEST_EQ(b.axis(1), a2);
  }
  return boost::report_errors();
}
