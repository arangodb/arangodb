// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/axis/category.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/traits.hpp>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_axis.hpp"
#include "utility_str.hpp"

int main() {
  using namespace boost::histogram;

  BOOST_TEST(std::is_nothrow_move_constructible<axis::category<int>>::value);
  BOOST_TEST(std::is_nothrow_move_constructible<axis::category<std::string>>::value);
  BOOST_TEST(std::is_nothrow_move_assignable<axis::category<int>>::value);
  BOOST_TEST(std::is_nothrow_move_assignable<axis::category<std::string>>::value);

  // bad ctor
  {
    int x[2];
    (void)x;
    BOOST_TEST_THROWS(axis::category<int>(x + 1, x), std::invalid_argument);
  }

  // value should return copy for arithmetic types and const reference otherwise
  {
    enum class Foo { foo };

    BOOST_TEST_TRAIT_SAME(axis::traits::value_type<axis::category<std::string>>,
                          std::string);
    BOOST_TEST_TRAIT_SAME(decltype(std::declval<axis::category<std::string>>().value(0)),
                          const std::string&);
    BOOST_TEST_TRAIT_SAME(axis::traits::value_type<axis::category<const char*>>,
                          const char*);
    BOOST_TEST_TRAIT_SAME(decltype(std::declval<axis::category<const char*>>().value(0)),
                          const char*);
    BOOST_TEST_TRAIT_SAME(axis::traits::value_type<axis::category<Foo>>, Foo);
    BOOST_TEST_TRAIT_SAME(decltype(std::declval<axis::category<Foo>>().value(0)), Foo);
    BOOST_TEST_TRAIT_SAME(axis::traits::value_type<axis::category<int>>, int);
    BOOST_TEST_TRAIT_SAME(decltype(std::declval<axis::category<int>>().value(0)), int);
  }

  // empty axis::category
  {
    axis::category<int> a;
    axis::category<int> b(std::vector<int>(0));
    BOOST_TEST_EQ(a, b);
    BOOST_TEST_EQ(a.size(), 0);
    BOOST_TEST_EQ(a.index(-1), 0);
    BOOST_TEST_EQ(a.index(0), 0);
    BOOST_TEST_EQ(a.index(1), 0);
  }

  // axis::category
  {
    std::string A("A"), B("B"), C("C"), other;

    axis::category<std::string> a({A, B, C}, "foo");
    BOOST_TEST_EQ(a.metadata(), "foo");
    BOOST_TEST_EQ(static_cast<const axis::category<std::string>&>(a).metadata(), "foo");
    a.metadata() = "bar";
    BOOST_TEST_EQ(static_cast<const axis::category<std::string>&>(a).metadata(), "bar");
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(a.index(A), 0);
    BOOST_TEST_EQ(a.index(B), 1);
    BOOST_TEST_EQ(a.index(C), 2);
    BOOST_TEST_EQ(a.index(other), 3);
    BOOST_TEST_EQ(a.value(0), A);
    BOOST_TEST_EQ(a.value(1), B);
    BOOST_TEST_EQ(a.value(2), C);
    BOOST_TEST_THROWS(a.value(3), std::out_of_range);

    BOOST_TEST_CSTR_EQ(
        str(a).c_str(),
        "category(\"A\", \"B\", \"C\", metadata=\"bar\", options=overflow)");
  }

  // category<int, axis::null_type>: copy, move
  {
    using C = axis::category<int, axis::null_type>;
    C a({1, 2, 3});
    C a2(a);
    BOOST_TEST_EQ(a2, a);
    C b;
    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    b = C{{2, 1, 3}};
    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    C c = std::move(b);
    BOOST_TEST_EQ(c, a);
    C d;
    BOOST_TEST_NE(c, d);
    d = std::move(c);
    BOOST_TEST_EQ(d, a);
  }

  // category<std::string>: copy, move
  {
    using C = axis::category<std::string>;

    C a({"A", "B", "C"}, "foo");
    C a2(a);
    BOOST_TEST_EQ(a2, a);
    C b;
    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    b = C{{"B", "A", "C"}};
    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    C c = std::move(b);
    BOOST_TEST_EQ(c, a);
    C d;
    BOOST_TEST_NE(c, d);
    d = std::move(c);
    BOOST_TEST_EQ(d, a);
  }

  // axis::category with growth
  {
    using pii_t = std::pair<axis::index_type, axis::index_type>;
    axis::category<int, axis::null_type, axis::option::growth_t> a;
    BOOST_TEST_EQ(a.size(), 0);
    BOOST_TEST_EQ(a.update(5), pii_t(0, -1));
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(a.update(1), pii_t(1, -1));
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(a.update(10), pii_t(2, -1));
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(a.update(10), pii_t(2, 0));
    BOOST_TEST_EQ(a.size(), 3);

    BOOST_TEST_EQ(str(a), "category(5, 1, 10, options=growth)");
  }

  // iterators
  {
    test_axis_iterator(axis::category<>({3, 1, 2}, ""), 0, 3);
    test_axis_iterator(axis::category<std::string>({"A", "B"}, ""), 0, 2);
  }

  return boost::report_errors();
}
