// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/throw_exception.hpp>
#include <string>
#include <vector>
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;

template <typename Tag>
void run_tests() {
  // arithmetic operators
  {
    auto a = make(Tag(), axis::integer<int, use_default, axis::option::none_t>(0, 2));
    auto b = a;
    a(0);
    b(1);
    auto c = a + b;
    BOOST_TEST_EQ(c.at(0), 1);
    BOOST_TEST_EQ(c.at(1), 1);
    c += b;
    BOOST_TEST_EQ(c.at(0), 1);
    BOOST_TEST_EQ(c.at(1), 2);
    auto d = a + b + c;
    BOOST_TEST_TRAIT_SAME(decltype(d), decltype(a));
    BOOST_TEST_EQ(d.at(0), 2);
    BOOST_TEST_EQ(d.at(1), 3);

    auto d2 = d - a - b - c;
    BOOST_TEST_TRAIT_SAME(decltype(d2), decltype(a));
    BOOST_TEST_EQ(d2.at(0), 0);
    BOOST_TEST_EQ(d2.at(1), 0);
    d2 -= a;
    BOOST_TEST_EQ(d2.at(0), -1);
    BOOST_TEST_EQ(d2.at(1), 0);

    auto d3 = d;
    d3 *= d;
    BOOST_TEST_EQ(d3.at(0), 4);
    BOOST_TEST_EQ(d3.at(1), 9);
    auto d4 = d3 * (1 * d); // converted return type
    BOOST_TEST_TRAIT_FALSE((boost::core::is_same<decltype(d4), decltype(d3)>));
    BOOST_TEST_EQ(d4.at(0), 8);
    BOOST_TEST_EQ(d4.at(1), 27);
    d4 /= d;
    BOOST_TEST_EQ(d4.at(0), 4);
    BOOST_TEST_EQ(d4.at(1), 9);
    auto d5 = d4 / d;
    BOOST_TEST_EQ(d5.at(0), 2);
    BOOST_TEST_EQ(d5.at(1), 3);

    auto e = 3 * a; // converted return type
    auto f = b * 2; // converted return type
    BOOST_TEST_TRAIT_FALSE((boost::core::is_same<decltype(e), decltype(a)>));
    BOOST_TEST_TRAIT_FALSE((boost::core::is_same<decltype(f), decltype(a)>));
    BOOST_TEST_EQ(e.at(0), 3);
    BOOST_TEST_EQ(e.at(1), 0);
    BOOST_TEST_EQ(f.at(0), 0);
    BOOST_TEST_EQ(f.at(1), 2);
    auto r = 1.0 * a;
    r += b;
    r += e;
    BOOST_TEST_EQ(r.at(0), 4);
    BOOST_TEST_EQ(r.at(1), 1);
    BOOST_TEST_EQ(r, a + b + 3 * a);
    auto s = r / 4;
    r /= 4;
    BOOST_TEST_EQ(r.at(0), 1);
    BOOST_TEST_EQ(r.at(1), 0.25);
    BOOST_TEST_EQ(r, s);
  }

  // arithmetic operators with mixed storage: unlimited vs. vector<unsigned>
  {
    auto ia = axis::integer<int, axis::null_type, axis::option::none_t>(0, 2);
    auto a = make(Tag(), ia);
    a(0, weight(2));
    a(1, weight(2));
    auto b = a;
    auto c = make_s(Tag(), std::vector<int>(), ia);
    c(0, weight(2));
    c(1, weight(2));
    auto a2 = a;
    a2 += c;
    BOOST_TEST_EQ(a2, (a + b));
    auto a3 = a;
    a3 *= c;
    BOOST_TEST_EQ(a3, (a * b));
    auto a4 = a;
    a4 -= c;
    BOOST_TEST_EQ(a4, (a - b));
    auto a5 = a;
    a5 /= c;
    BOOST_TEST_EQ(a5, (a / b));
  }

  // arithmetic operators with mixed storage: vector<unsigned char> vs. vector<unsigned>
  {
    auto ia = axis::integer<int, axis::null_type, axis::option::none_t>(0, 2);
    auto a = make_s(Tag(), std::vector<unsigned long>{}, ia);
    auto c = make_s(Tag(), std::vector<unsigned>(), ia);
    a(0, weight(2u));
    a(1, weight(2u));
    auto b = a;
    c(0, weight(2u));
    c(1, weight(2u));
    auto a2 = a;
    a2 += c;
    BOOST_TEST_EQ(a2, (a + b));
    auto a3 = a;
    a3 *= c;
    BOOST_TEST_EQ(a3, (a * b));
    auto a4 = a;
    a4 -= c;
    BOOST_TEST_EQ(a4, (a - b));
    auto a5 = a;
    a5 /= c;
    BOOST_TEST_EQ(a5, (a / b));
  }

  // add operators with weighted storage
  {
    auto ia = axis::integer<int, axis::null_type, axis::option::none_t>(0, 2);
    auto a = make_s(Tag(), std::vector<accumulators::weighted_sum<>>(), ia);
    auto b = make_s(Tag(), std::vector<accumulators::weighted_sum<>>(), ia);

    a(0);
    BOOST_TEST_EQ(a.at(0).variance(), 1);
    b(weight(3), 1);
    BOOST_TEST_EQ(b.at(1).variance(), 9);
    auto c = a;
    c += b;
    BOOST_TEST_EQ(c.at(0).value(), 1);
    BOOST_TEST_EQ(c.at(0).variance(), 1);
    BOOST_TEST_EQ(c.at(1).value(), 3);
    BOOST_TEST_EQ(c.at(1).variance(), 9);
    auto d = a;
    d += b;
    BOOST_TEST_EQ(d.at(0).value(), 1);
    BOOST_TEST_EQ(d.at(0).variance(), 1);
    BOOST_TEST_EQ(d.at(1).value(), 3);
    BOOST_TEST_EQ(d.at(1).variance(), 9);
  }

  // bad operations
  {
    auto a = make(Tag(), axis::integer<>(0, 2));
    auto b = make(Tag(), axis::integer<>(0, 3));
    BOOST_TEST_THROWS(a += b, std::invalid_argument);
    BOOST_TEST_THROWS(a -= b, std::invalid_argument);
    BOOST_TEST_THROWS(a *= b, std::invalid_argument);
    BOOST_TEST_THROWS(a /= b, std::invalid_argument);
  }

  // histogram_ostream
  {
    auto a = make(Tag(), axis::regular<>(3, -1, 1, "r"));
    std::ostringstream os1;
    os1 << a;
    BOOST_TEST_EQ(os1.str(), std::string("histogram(regular(3, -1, 1, metadata=\"r\", "
                                         "options=underflow | overflow))"));

    auto b = make(Tag(), axis::regular<>(3, -1, 1, "r"), axis::integer<>(0, 2, "i"));
    std::ostringstream os2;
    os2 << b;
    BOOST_TEST_EQ(
        os2.str(),
        std::string("histogram(\n"
                    "  regular(3, -1, 1, metadata=\"r\", options=underflow | overflow),\n"
                    "  integer(0, 2, metadata=\"i\", options=underflow | overflow)\n"
                    ")"));
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  {
    auto h = histogram<std::vector<axis::regular<>>>();
    std::ostringstream os;
    os << h;
    BOOST_TEST_EQ(os.str(), std::string("histogram()"));
  }

  return boost::report_errors();
}
