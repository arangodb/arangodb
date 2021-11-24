// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/detail/axes.hpp>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>
#include "std_ostream.hpp"
#include "throw_exception.hpp"

using namespace boost::histogram;

int main() {
  // dynamic axis_get with tuples
  {
    auto a1 = axis::integer<>(0, 1);
    auto a2 = axis::integer<>(1, 2);
    auto tup = std::make_tuple(a1, a2);
    using E1 = axis::variant<axis::integer<>*>;
    BOOST_TEST_TRAIT_SAME(decltype(detail::axis_get(tup, 0)), E1);
    BOOST_TEST_EQ(detail::axis_get(tup, 0), a1);
    BOOST_TEST_EQ(detail::axis_get(tup, 1), a2);
    BOOST_TEST_NE(detail::axis_get(tup, 0), a2);
  }

  // sequence equality
  {
    using R = axis::regular<>;
    using I = axis::integer<>;
    using V = axis::variable<>;
    auto r = R(2, -1, 1);
    auto i = I(-1, 1);
    auto v = V{-1, 0, 1};

    std::vector<axis::variant<R, I, V>> v1 = {r, i};
    std::vector<axis::variant<R, I>> v2 = {r, i};
    std::vector<axis::variant<R, I>> v3 = {i, r};
    std::vector<axis::variant<I, R>> v4 = {r, i};
    std::vector<axis::variant<R, I>> v5 = {r, r};
    std::vector<R> v6 = {r, r};

    BOOST_TEST(detail::axes_equal(v1, v2));
    BOOST_TEST(detail::axes_equal(v1, v4));
    BOOST_TEST(detail::axes_equal(v5, v6));
    BOOST_TEST_NOT(detail::axes_equal(v1, v3));
    BOOST_TEST_NOT(detail::axes_equal(v2, v3));
    BOOST_TEST_NOT(detail::axes_equal(v3, v4));
    BOOST_TEST_NOT(detail::axes_equal(v1, v5));

    auto t1 = std::make_tuple(r, i);
    auto t2 = std::make_tuple(i, r);
    auto t3 = std::make_tuple(v, i);
    auto t4 = std::make_tuple(r, r);

    BOOST_TEST(detail::axes_equal(t1, t1));
    BOOST_TEST(detail::axes_equal(t1, v1));
    BOOST_TEST(detail::axes_equal(t1, v2));
    BOOST_TEST(detail::axes_equal(t1, v4));
    BOOST_TEST(detail::axes_equal(v1, t1));
    BOOST_TEST(detail::axes_equal(v2, t1));
    BOOST_TEST(detail::axes_equal(v4, t1));
    BOOST_TEST(detail::axes_equal(t2, v3));
    BOOST_TEST(detail::axes_equal(v3, t2));
    BOOST_TEST(detail::axes_equal(t4, v5));
    BOOST_TEST(detail::axes_equal(t4, v6));
    BOOST_TEST_NOT(detail::axes_equal(t1, t2));
    BOOST_TEST_NOT(detail::axes_equal(t2, t3));
    BOOST_TEST_NOT(detail::axes_equal(t1, v3));
    BOOST_TEST_NOT(detail::axes_equal(t1, v3));
    BOOST_TEST_NOT(detail::axes_equal(t3, v1));
    BOOST_TEST_NOT(detail::axes_equal(t3, v2));
    BOOST_TEST_NOT(detail::axes_equal(t3, v3));
    BOOST_TEST_NOT(detail::axes_equal(t3, v4));
  }

  // sequence assign
  {
    using R = axis::regular<>;
    using I = axis::integer<>;
    using V = axis::variable<>;
    auto r = R(2, -1, 1);
    auto i = I(-1, 1);
    auto v = V{-1, 0, 1};

    std::vector<axis::variant<R, V, I>> v1 = {r, i};
    std::vector<axis::variant<I, R>> v2;
    std::vector<R> v3 = {r, r};

    BOOST_TEST_NOT(detail::axes_equal(v2, v1));
    detail::axes_assign(v2, v1);
    BOOST_TEST(detail::axes_equal(v2, v1));
    detail::axes_assign(v2, v3);
    BOOST_TEST(detail::axes_equal(v2, v3));

    auto t1 = std::make_tuple(r);
    detail::axes_assign(v3, t1);
    BOOST_TEST(detail::axes_equal(v3, t1));

    auto t2 = std::make_tuple(r, i);
    detail::axes_assign(v2, t2);
    BOOST_TEST(detail::axes_equal(v2, t2));

    auto t3 = std::make_tuple(R{3, -1, 1}, i);
    BOOST_TEST_NOT(detail::axes_equal(t2, t3));
    detail::axes_assign(t2, t3);
    BOOST_TEST(detail::axes_equal(t2, t3));
  }

  // axes_transform
  {
    using R = axis::regular<>;
    using I = axis::integer<double>;

    {
      auto t = std::make_tuple(R(1, 0, 1), R(2, 0, 2), I(0, 3));
      auto t2 = detail::axes_transform(
          t, [](std::size_t, const auto& a) { return I(0, a.size()); });
      BOOST_TEST_EQ(t2, std::make_tuple(I(0, 1), I(0, 2), I(0, 3)));
    }
    {
      auto t = std::vector<I>{{I(0, 1), I(0, 2)}};
      auto t2 = detail::axes_transform(
          t, [](std::size_t, const auto& a) { return I(0, a.size() + 1); });
      auto t3 = std::vector<I>{{I(0, 2), I(0, 3)}};
      BOOST_TEST(detail::axes_equal(t2, t3));
    }
    {
      using V = axis::variant<R, I>;
      auto t = std::vector<V>{{V{I(0, 1)}, V{R(2, 0, 2)}}};
      auto t2 = detail::axes_transform(
          t, [](std::size_t, const auto& a) { return I(0, a.size() + 1); });
      auto t3 = std::vector<V>{{I(0, 2), I(0, 3)}};
      BOOST_TEST(detail::axes_equal(t2, t3));
    }

    {
      using V = axis::variant<R, I>;
      auto t1 = std::vector<V>{{V{I(0, 1)}, V{R(2, 0, 2)}}};
      auto t2 = std::vector<V>{{V{I(0, 1)}, V{R(2, 0, 2)}}};
      auto t3 = detail::axes_transform(
          t1, t2, [](const auto& a, const auto& b) { return I(0, a.size() + b.size()); });
      auto t4 = std::vector<V>{{I(0, 2), I(0, 4)}};
      BOOST_TEST(detail::axes_equal(t3, t4));
    }

    {
      // test otherwise unreachable code
      auto a = R(2, 0, 2);
      auto b = I(0, 2);
      BOOST_TEST_THROWS(detail::axis_merger{}(a, b), std::invalid_argument);
    }
  }

  // axes_rank
  {
    std::tuple<int, int> a;
    std::vector<int> b(3);
    std::array<int, 4> c;
    const std::tuple<int> d;
    BOOST_TEST_EQ(detail::axes_rank(a), 2);
    BOOST_TEST_EQ(detail::axes_rank(b), 3);
    BOOST_TEST_EQ(detail::axes_rank(c), 4);
    BOOST_TEST_EQ(detail::axes_rank(d), 1);
  }

  // bincount overflow
  {
    auto v = std::vector<axis::integer<>>(
        100, axis::integer<>(0, (std::numeric_limits<int>::max)() - 2));
    BOOST_TEST_THROWS(detail::bincount(v), std::overflow_error);
  }

  // has_growing_axis
  {
    struct growing {
      auto update(int) { return std::make_pair(0, 0); }
    };
    using T = growing;
    using I = axis::integer<>;

    using A = std::tuple<I, T>;
    using B = std::vector<T>;
    using C = std::vector<axis::variant<I, T>>;
    using D = std::tuple<I>;
    using E = std::vector<I>;
    using F = std::vector<axis::variant<I>>;

    BOOST_TEST_TRAIT_TRUE((detail::has_growing_axis<A>));
    BOOST_TEST_TRAIT_TRUE((detail::has_growing_axis<B>));
    BOOST_TEST_TRAIT_TRUE((detail::has_growing_axis<C>));
    BOOST_TEST_TRAIT_FALSE((detail::has_growing_axis<D>));
    BOOST_TEST_TRAIT_FALSE((detail::has_growing_axis<E>));
    BOOST_TEST_TRAIT_FALSE((detail::has_growing_axis<F>));
  }

  // value_types
  {
    using R = axis::regular<float>;
    using I = axis::integer<int>;
    using CI = axis::category<int>;
    using CS = axis::category<std::string>;
    using A = std::vector<axis::variant<R, I, CS>>;
    using B = std::vector<axis::variant<CS, I, CI, R>>;
    using C = std::tuple<I, R, CS>;
    using D = std::tuple<CS, I, CI, R>;
    using Expected = boost::mp11::mp_list<int, float, std::string>;
    BOOST_TEST_TRAIT_SAME(detail::value_types<A>, Expected);
    BOOST_TEST_TRAIT_SAME(detail::value_types<B>, Expected);
    BOOST_TEST_TRAIT_SAME(detail::value_types<C>, Expected);
    BOOST_TEST_TRAIT_SAME(detail::value_types<D>, Expected);
  }

  return boost::report_errors();
}
