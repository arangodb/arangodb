// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/detail/type_name.hpp>
#include <string>
#include <type_traits>
#include <vector>
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_allocator.hpp"
#include "utility_axis.hpp"
#include "utility_str.hpp"

int main() {
  using namespace boost::histogram;
  namespace tr = axis::transform;

  {
    (void)axis::variant<>{};
  }

  {
    using meta_type = std::vector<int>;
    using variant_type =
        axis::variant<axis::integer<double>, axis::category<std::string, meta_type>>;
    auto a = variant_type{axis::integer<double>(0, 2, "foo")};
    BOOST_TEST_EQ(a.index(-10), -1);
    BOOST_TEST_EQ(a.index(-1), -1);
    BOOST_TEST_EQ(a.index(0), 0);
    BOOST_TEST_EQ(a.index(0.5), 0);
    BOOST_TEST_EQ(a.index(1), 1);
    BOOST_TEST_EQ(a.index(2), 2);
    BOOST_TEST_EQ(a.index(10), 2);
    BOOST_TEST_EQ(a.bin(-1).lower(), -std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.bin(a.size()).upper(), std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.bin(-10).lower(), -std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.bin(a.size() + 10).upper(), std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.metadata(), "foo");
    a.metadata() = "bar";
    BOOST_TEST_EQ(static_cast<const variant_type&>(a).metadata(), "bar");
    BOOST_TEST_EQ(a.options(), axis::option::underflow | axis::option::overflow);

    a = axis::category<std::string, meta_type>({"A", "B"}, {1, 2, 3});
    BOOST_TEST_EQ(a.index("A"), 0);
    BOOST_TEST_EQ(a.index("B"), 1);
    BOOST_TEST_THROWS(a.metadata(), std::runtime_error);
    BOOST_TEST_THROWS(static_cast<const variant_type&>(a).metadata(), std::runtime_error);
    BOOST_TEST_EQ(a.options(), axis::option::overflow_t::value);
  }

  // axis::variant with pointers
  {
    using A = axis::integer<>;
    using B = axis::regular<>;
    auto a = A(1, 5, "foo");
    auto b = B(3, 1, 5, "bar");
    axis::variant<A*, B*> r1(&a);
    BOOST_TEST_EQ(r1, a);
    BOOST_TEST_NE(r1, A(2, 4));
    BOOST_TEST_NE(r1, b);
    BOOST_TEST_EQ(r1.size(), 4);
    BOOST_TEST_EQ(r1.value(0), 1);
    BOOST_TEST_EQ(r1.metadata(), a.metadata());
    BOOST_TEST_EQ(r1.options(), a.options());
    // change original through r1
    r1.metadata() = "bar";
    BOOST_TEST_EQ(a.metadata(), "bar");
    r1 = &b;
    BOOST_TEST_EQ(r1, b);

    axis::variant<const A*, const B*> r2(static_cast<const B*>(&b));
    BOOST_TEST_EQ(r2, b);
    BOOST_TEST_NE(r2, B(4, 1, 5));
    BOOST_TEST_NE(r2, a);
    BOOST_TEST_EQ(r2.size(), 3);
    BOOST_TEST_EQ(r2.value(0), 1);
    BOOST_TEST_EQ(r2.metadata(), "bar");
    r2.metadata() = "baz"; // change original through r2
    BOOST_TEST_EQ(b.metadata(), "baz");
  }

  // axis::variant copyable
  {
    axis::variant<axis::regular<>> a1(axis::regular<>(2, -1, 1));
    axis::variant<axis::regular<>> a2(a1);
    BOOST_TEST_EQ(a1, a2);
    axis::variant<axis::regular<>> a3;
    BOOST_TEST_NE(a3, a1);
    a3 = a1;
    BOOST_TEST_EQ(a3, a1);
    axis::variant<axis::regular<>> a4(axis::regular<>(3, -2, 2));
    axis::variant<axis::regular<>, axis::integer<>> a5(a4);
    BOOST_TEST_EQ(a4, a5);
    axis::variant<axis::regular<>> a6;
    a6 = a1;
    BOOST_TEST_EQ(a6, a1);
    axis::variant<axis::regular<>, axis::integer<>> a7(axis::integer<>(0, 2));
    BOOST_TEST_THROWS(axis::variant<axis::regular<>> a8(a7), std::runtime_error);
    BOOST_TEST_THROWS(a4 = a7, std::runtime_error);
  }

  // axis::variant movable
  {
    axis::variant<axis::regular<>> a(axis::regular<>(2, -1, 1));
    axis::variant<axis::regular<>> r(a);
    axis::variant<axis::regular<>> b(std::move(a));
    BOOST_TEST_EQ(b, r);
    axis::variant<axis::regular<>> c;
    BOOST_TEST_NE(c, b);
    c = std::move(b);
    BOOST_TEST(c == r);
  }

  // axis::variant streamable
  {
    auto test = [](auto&& a, const char* ref) {
      using T = std::decay_t<decltype(a)>;
      axis::variant<T> axis(std::move(a));
      BOOST_TEST_CSTR_EQ(str(axis).c_str(), ref);
    };

    test(axis::regular<>{2, -1, 1, "foo"},
         "regular(2, -1, 1, metadata=\"foo\", options=underflow | overflow)");

    test(axis::boolean<>{"bar"}, "boolean(metadata=\"bar\")");

    struct user_defined {};
    const auto ref = "integer(-1, 1, metadata=" + detail::type_name<user_defined>() +
                     ", options=none)";
    test(axis::integer<int, user_defined, axis::option::none_t>(-1, 1), ref.c_str());
  }

  // bin_type operator<<
  {
    auto test = [](auto&& a, const char* ref) {
      using T = std::decay_t<decltype(a)>;
      axis::variant<T> axis(std::move(a));
      BOOST_TEST_CSTR_EQ(str(axis.bin(0)).c_str(), ref);
    };

    test(axis::regular<>(2, 1, 2), "[1, 1.5)");
    test(axis::category<>({1, 2}), "1");
  }

  // axis::variant operator==
  {
    enum { A, B, C };
    using variant =
        axis::variant<axis::regular<>, axis::regular<double, axis::transform::pow>,
                      axis::category<>, axis::integer<>, axis::boolean<>>;
    std::vector<variant> axes;
    axes.push_back(axis::regular<>{2, -1, 1});
    axes.push_back(axis::regular<double, tr::pow>{tr::pow{0.5}, 2, 1, 4});
    axes.push_back(axis::category<>{A, B, C});
    axes.push_back(axis::integer<>{-1, 1});
    axes.push_back(axis::boolean<>{});
    for (const auto& a : axes) {
      BOOST_TEST_NE(a, variant{});
      BOOST_TEST_EQ(a, variant(a));
    }
    BOOST_TEST_NE(axes, std::vector<variant>{});
    BOOST_TEST(axes == std::vector<variant>(axes));
  }

  // axis::variant with axis that has incompatible bin type
  {
    auto a = axis::variant<axis::category<std::string>>{
        axis::category<std::string>{"A", "B", "C"}};
    BOOST_TEST_THROWS(a.bin(0), std::runtime_error);
    auto b = axis::variant<axis::category<int>>{axis::category<int>{2, 1, 3}};
    BOOST_TEST_EQ(b.bin(0), 2);
    BOOST_TEST_EQ(b.bin(0).lower(),
                  b.bin(0).upper()); // lower == upper for bin without interval
  }

  // axis::variant support for user-defined axis types
  {
    struct minimal_axis {
      int index(int x) const { return x % 2; }
      int size() const { return 2; }
    };

    axis::variant<minimal_axis, axis::category<std::string>> axis;
    BOOST_TEST_EQ(axis.index(0), 0);
    BOOST_TEST_EQ(axis.index(9), 1);
    BOOST_TEST_EQ(axis.size(), 2);
    BOOST_TEST_EQ(axis.metadata(), axis::null_type{});
    BOOST_TEST_EQ(str(axis), detail::type_name<minimal_axis>());
    BOOST_TEST_THROWS(axis.value(0), std::runtime_error);

    axis = axis::category<std::string>({"A", "B"}, "category");
    BOOST_TEST_EQ(axis.index("B"), 1);
    BOOST_TEST_THROWS(axis.value(0), std::runtime_error);
  }

  // vector of axes with custom allocators
  {
    using M = std::vector<char, tracing_allocator<char>>;
    using T1 = axis::regular<double, tr::id, M>;
    using T2 = axis::integer<int, axis::null_type>;
    using T3 = axis::category<long, axis::null_type, axis::option::overflow_t,
                              tracing_allocator<long>>;
    using axis_type = axis::variant<T1, T2, T3>; // no heap allocation
    using axes_type = std::vector<axis_type, tracing_allocator<axis_type>>;

    tracing_allocator_db db;
    {
      auto a = tracing_allocator<char>(db);
      axes_type axes(a);
      axes.reserve(3);
      axes.emplace_back(T1(1, 0, 1, M(3, 'c', a)));
      axes.emplace_back(T2(0, 4));
      axes.emplace_back(T3({1, 2, 3, 4, 5}, {}, a));
    }
    // 3 axis::variant objects
    BOOST_TEST_EQ(db.at<axis_type>().first, 0);
    BOOST_TEST_EQ(db.at<axis_type>().second, 3);

    // label of T1
    BOOST_TEST_EQ(db.at<char>().first, 0);
    BOOST_TEST_EQ(db.at<char>().second, 3);

    // T3 allocates storage for long array
    BOOST_TEST_EQ(db.at<long>().first, 0);
    BOOST_TEST_EQ(db.at<long>().second, 5);
  }

  // testing pass-through versions of get
  {
    axis::regular<> a(10, 0, 1);
    axis::integer<> b(0, 3);
    const auto& ta = axis::get<axis::regular<>>(a);
    BOOST_TEST_EQ(ta, a);
    const auto* tb = axis::get_if<axis::integer<>>(&b);
    BOOST_TEST_EQ(tb, &b);
    const auto* tc = axis::get_if<axis::regular<>>(&b);
    BOOST_TEST_EQ(tc, nullptr);
  }

  // iterators
  test_axis_iterator(axis::variant<axis::regular<>>(axis::regular<>(5, 0, 1)), 0, 5);

  return boost::report_errors();
}
