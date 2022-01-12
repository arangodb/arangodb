// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/traits.hpp>
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_axis.hpp"

using namespace boost::histogram::axis;
using namespace std::literals;

struct ValueTypeOverride {
  index_type index(int);
};

namespace boost {
namespace histogram {
namespace detail {
template <>
struct value_type_deducer<ValueTypeOverride> {
  using type = char;
};
} // namespace detail
} // namespace histogram
} // namespace boost

int main() {
  // value_type
  {
    struct Foo {
      void index(const long&);
    };

    BOOST_TEST_TRAIT_SAME(traits::value_type<Foo>, long);
    BOOST_TEST_TRAIT_SAME(traits::value_type<ValueTypeOverride>, char);
    BOOST_TEST_TRAIT_SAME(traits::value_type<integer<int>>, int);
    BOOST_TEST_TRAIT_SAME(traits::value_type<category<int>>, int);
    BOOST_TEST_TRAIT_SAME(traits::value_type<regular<double>>, double);
  }

  // is_continuous
  {
    BOOST_TEST_TRAIT_TRUE((traits::is_continuous<regular<>>));
    BOOST_TEST_TRAIT_FALSE((traits::is_continuous<integer<int>>));
    BOOST_TEST_TRAIT_FALSE((traits::is_continuous<category<int>>));
    BOOST_TEST_TRAIT_TRUE((traits::is_continuous<integer<double>>));
  }

  // is_reducible
  {
    struct not_reducible {};
    struct reducible {
      reducible(const reducible&, index_type, index_type, unsigned);
    };

    BOOST_TEST_TRAIT_TRUE((traits::is_reducible<reducible>));
    BOOST_TEST_TRAIT_FALSE((traits::is_reducible<not_reducible>));

    BOOST_TEST_TRAIT_TRUE((traits::is_reducible<regular<>>));
    BOOST_TEST_TRAIT_TRUE((traits::is_reducible<variable<>>));
    BOOST_TEST_TRAIT_TRUE((traits::is_reducible<circular<>>));
    BOOST_TEST_TRAIT_TRUE((traits::is_reducible<integer<>>));
    BOOST_TEST_TRAIT_TRUE((traits::is_reducible<category<>>));
  }

  // get_options, options()
  {
    using A = integer<>;
    BOOST_TEST_EQ(traits::get_options<A>::test(option::growth), false);
    auto expected = option::underflow | option::overflow;
    auto a = A{};
    BOOST_TEST_EQ(traits::options(a), expected);
    BOOST_TEST_EQ(traits::options(static_cast<A&>(a)), expected);
    BOOST_TEST_EQ(traits::options(static_cast<const A&>(a)), expected);
    BOOST_TEST_EQ(traits::options(std::move(a)), expected);

    using B = integer<int, null_type, option::growth_t>;
    BOOST_TEST_EQ(traits::get_options<B>::test(option::growth), true);
    BOOST_TEST_EQ(traits::options(B{}), option::growth);

    struct growing {
      auto update(double) { return std::make_pair(0, 0); }
    };
    using C = growing;
    BOOST_TEST_EQ(traits::get_options<C>::test(option::growth), true);
    auto c = C{};
    BOOST_TEST_EQ(traits::options(c), option::growth);
    BOOST_TEST_EQ(traits::options(static_cast<C&>(c)), option::growth);
    BOOST_TEST_EQ(traits::options(static_cast<const C&>(c)), option::growth);
    BOOST_TEST_EQ(traits::options(std::move(c)), option::growth);

    struct notgrowing {
      auto index(double) { return 0; }
    };
    using D = notgrowing;
    BOOST_TEST_EQ(traits::get_options<D>::test(option::growth), false);
    auto d = D{};
    BOOST_TEST_EQ(traits::options(d), option::none);
    BOOST_TEST_EQ(traits::options(static_cast<D&>(d)), option::none);
    BOOST_TEST_EQ(traits::options(static_cast<const D&>(d)), option::none);
    BOOST_TEST_EQ(traits::options(std::move(d)), option::none);
  }

  // is_inclusive, inclusive()
  {
    struct empty {};
    struct with_opts_not_inclusive {
      static constexpr unsigned options() { return option::underflow | option::overflow; }
      static constexpr bool inclusive() { return false; }
    };

    BOOST_TEST_EQ(traits::inclusive(empty{}), false);
    BOOST_TEST_EQ(traits::inclusive(regular<>{}), true);

    BOOST_TEST_TRAIT_FALSE((traits::is_inclusive<empty>));
    BOOST_TEST_TRAIT_FALSE((traits::is_inclusive<with_opts_not_inclusive>));

    BOOST_TEST_TRAIT_TRUE((traits::is_inclusive<regular<>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<
            regular<double, boost::use_default, boost::use_default, option::growth_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<regular<double, boost::use_default, boost::use_default,
                                      option::circular_t>>));

    BOOST_TEST_TRAIT_TRUE((traits::is_inclusive<variable<>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<variable<double, boost::use_default, option::growth_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<variable<double, boost::use_default, option::circular_t>>));

    BOOST_TEST_TRAIT_TRUE((traits::is_inclusive<integer<int>>));
    BOOST_TEST_TRAIT_TRUE(
        (traits::is_inclusive<integer<int, boost::use_default, option::growth_t>>));
    BOOST_TEST_TRAIT_TRUE(
        (traits::is_inclusive<integer<int, boost::use_default, option::circular_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<integer<int, boost::use_default, option::underflow_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<integer<int, boost::use_default, option::overflow_t>>));
    BOOST_TEST_TRAIT_TRUE(
        (traits::is_inclusive<integer<int, boost::use_default,
                                      decltype(option::underflow | option::overflow)>>));

    BOOST_TEST_TRAIT_TRUE((traits::is_inclusive<integer<double>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<integer<double, boost::use_default, option::growth_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<integer<double, boost::use_default, option::circular_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<integer<double, boost::use_default, option::underflow_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<integer<double, boost::use_default, option::overflow_t>>));
    BOOST_TEST_TRAIT_TRUE(
        (traits::is_inclusive<integer<double, boost::use_default,
                                      decltype(option::underflow | option::overflow)>>));

    BOOST_TEST_TRAIT_TRUE((traits::is_inclusive<category<int>>));
    BOOST_TEST_TRAIT_TRUE(
        (traits::is_inclusive<category<int, boost::use_default, option::growth_t>>));
    BOOST_TEST_TRAIT_FALSE(
        (traits::is_inclusive<category<int, boost::use_default, option::none_t>>));
    BOOST_TEST_TRAIT_TRUE(
        (traits::is_inclusive<category<int, boost::use_default, option::overflow_t>>));
  }

  // is_ordered, ordered()
  {
    struct ordered_1 {
      constexpr static bool ordered() { return true; }
      index_type index(ordered_1) { return true; }
    };
    struct ordered_2 {
      index_type index(int);
    };
    struct not_ordered_1 {
      constexpr static bool ordered() { return false; }
      index_type index(int);
    };
    struct not_ordered_2 {
      index_type index(not_ordered_2);
    };

    BOOST_TEST_TRAIT_TRUE((traits::is_ordered<ordered_1>));
    BOOST_TEST_TRAIT_TRUE((traits::is_ordered<ordered_2>));
    BOOST_TEST_TRAIT_FALSE((traits::is_ordered<not_ordered_1>));
    BOOST_TEST_TRAIT_FALSE((traits::is_ordered<not_ordered_2>));

    BOOST_TEST(traits::ordered(integer<>{}));
    BOOST_TEST_NOT(traits::ordered(category<int>{}));
  }

  // index, rank, value, width
  {
    auto a = integer<>(1, 3);
    BOOST_TEST_EQ(traits::index(a, 1), 0);
    BOOST_TEST_EQ(traits::rank(a), 1);
    BOOST_TEST_EQ(traits::value(a, 0), 1);
    BOOST_TEST_EQ(traits::width(a, 0), 0);
    BOOST_TEST_EQ(traits::width(a, 0), 0);

    auto b = integer<double>(1, 3);
    BOOST_TEST_EQ(traits::index(b, 1), 0);
    BOOST_TEST_EQ(traits::rank(b), 1);
    BOOST_TEST_EQ(traits::value(b, 0), 1);
    BOOST_TEST_EQ(traits::width(b, 0), 1);
    BOOST_TEST(traits::get_options<decltype(b)>::test(option::underflow));

    auto c = category<std::string>{"red", "blue"};
    BOOST_TEST_EQ(traits::index(c, "blue"), 1);
    BOOST_TEST_EQ(traits::rank(c), 1);
    BOOST_TEST_EQ(traits::value(c, 0), "red"s);
    BOOST_TEST_EQ(traits::width(c, 0), 0);

    struct D {
      index_type index(const std::tuple<int, double>& args) const {
        return static_cast<index_type>(std::get<0>(args) + std::get<1>(args));
      }
      index_type size() const { return 5; }
    } d;
    BOOST_TEST_EQ(traits::index(d, std::make_tuple(1, 2.0)), 3.0);
    BOOST_TEST_EQ(traits::rank(d), 2u);

    variant<D, integer<>> v;
    v = a;
    BOOST_TEST_EQ(traits::rank(v), 1u);
    v = d;
    BOOST_TEST_EQ(traits::rank(v), 2u);

    struct E : integer<> {
      using integer::integer; // inherit ctors of base
      // customization point: convert argument and call base class
      auto index(const char* s) const { return integer::index(std::atoi(s)); }
    };

    BOOST_TEST_EQ(traits::index(E{0, 3}, "2"), 2);
  }

  // update
  {
    auto a = integer<int, null_type, option::growth_t>();
    BOOST_TEST_EQ(traits::update(a, 0), (std::pair<index_type, index_type>(0, -1)));
    BOOST_TEST_THROWS(traits::update(a, "foo"), std::invalid_argument);

    variant<integer<int, null_type, option::growth_t>, integer<>> v(a);
    BOOST_TEST_EQ(traits::update(v, 0), (std::pair<index_type, index_type>(0, 0)));
  }

  // metadata
  {
    struct None {};

    struct Const {
      const int& metadata() const { return m; };
      int m = 0;
    };

    struct Both {
      const int& metadata() const { return m; };
      int& metadata() { return m; };
      int m = 0;
    };

    None none;
    BOOST_TEST_TRAIT_SAME(decltype(traits::metadata(none)), null_type&);
    BOOST_TEST_TRAIT_SAME(decltype(traits::metadata(static_cast<None&>(none))),
                          null_type&);
    BOOST_TEST_TRAIT_SAME(decltype(traits::metadata(static_cast<const None&>(none))),
                          null_type&);

    Const c;
    BOOST_TEST_EQ(traits::metadata(c), 0);
    BOOST_TEST_EQ(traits::metadata(static_cast<Const&>(c)), 0);
    BOOST_TEST_EQ(traits::metadata(static_cast<const Const&>(c)), 0);

    Both b;
    BOOST_TEST_EQ(traits::metadata(b), 0);
    BOOST_TEST_EQ(traits::metadata(static_cast<Both&>(b)), 0);
    BOOST_TEST_EQ(traits::metadata(static_cast<const Both&>(b)), 0);
  }

  return boost::report_errors();
}
