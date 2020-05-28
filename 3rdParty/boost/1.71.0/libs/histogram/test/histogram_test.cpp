// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>
#include "is_close.hpp"
#include "throw_exception.hpp"
#include "utility_allocator.hpp"
#include "utility_axis.hpp"
#include "utility_histogram.hpp"
#include "std_ostream.hpp"

using namespace boost::histogram;
using namespace boost::histogram::literals; // to get _c suffix

template <typename A, typename S>
void pass_histogram(boost::histogram::histogram<A, S>& h) {
  BOOST_TEST_EQ(h.at(0), 0);
  BOOST_TEST_EQ(h.at(1), 1);
  BOOST_TEST_EQ(h.at(2), 0);
  BOOST_TEST_EQ(h.axis(0_c), axis::integer<>(0, 3));
}

template <typename Tag>
void run_tests() {
  // init_1
  {
    auto h = make(Tag(), axis::regular<>{3, -1, 1});
    BOOST_TEST_EQ(h.rank(), 1);
    BOOST_TEST_EQ(h.size(), 5);
    BOOST_TEST_EQ(h.axis(0_c).size(), 3);
    BOOST_TEST_EQ(h.axis().size(), 3);
    auto h2 = make_s(Tag(), std::vector<unsigned>(), axis::regular<>{3, -1, 1});
    BOOST_TEST_EQ(h2, h);
  }

  // init_2
  {
    auto h = make(Tag(), axis::regular<>{3, -1, 1}, axis::integer<>{-1, 3});
    BOOST_TEST_EQ(h.rank(), 2);
    BOOST_TEST_EQ(h.size(), 30);
    BOOST_TEST_EQ(h.axis(0_c).size(), 3);
    BOOST_TEST_EQ(h.axis(1_c).size(), 4);
    auto h2 = make_s(Tag(), std::vector<unsigned>(), axis::regular<>{3, -1, 1},
                     axis::integer<>{-1, 3});
    BOOST_TEST_EQ(h2, h);
  }

  // init_3
  {
    auto h = make(Tag(), axis::regular<>{3, -1, 1}, axis::integer<>{-1, 2},
                  axis::circular<>{2, 0, 360});
    BOOST_TEST_EQ(h.rank(), 3);
    BOOST_TEST_EQ(h.size(), 5 * 5 * 3);
    auto h2 = make_s(Tag(), std::vector<unsigned>(), axis::regular<>{3, -1, 1},
                     axis::integer<>{-1, 2}, axis::circular<>{2, 0, 360});
    BOOST_TEST_EQ(h2, h);
  }

  // init_4
  {
    auto h = make(Tag(), axis::regular<>{3, -1, 1}, axis::integer<>{-1, 2},
                  axis::circular<>{2, 0, 360}, axis::variable<>{-1, 0, 1});
    BOOST_TEST_EQ(h.rank(), 4);
    BOOST_TEST_EQ(h.size(), 5 * 5 * 3 * 4);
    auto h2 = make_s(Tag(), std::vector<unsigned>(), axis::regular<>{3, -1, 1},
                     axis::integer<>{-1, 2}, axis::circular<>{2, 0, 360},
                     axis::variable<>{-1, 0, 1});
    BOOST_TEST_EQ(h2, h);
  }

  // init_5
  {
    auto h = make(Tag(), axis::regular<>{3, -1, 1}, axis::integer<>{-1, 2},
                  axis::circular<>{2, 0, 360}, axis::variable<>{-1, 0, 1},
                  axis::category<>{{3, 1, 2}});
    BOOST_TEST_EQ(h.rank(), 5);
    BOOST_TEST_EQ(h.size(), 5 * 5 * 3 * 4 * 4);
    auto h2 = make_s(Tag(), std::vector<unsigned>(), axis::regular<>{3, -1, 1},
                     axis::integer<>{-1, 2}, axis::circular<>{2, 0, 360},
                     axis::variable<>{-1, 0, 1}, axis::category<>{{3, 1, 2}});
    BOOST_TEST_EQ(h2, h);
  }

  // copy_ctor
  {
    auto h = make(Tag(), axis::integer<>{0, 2}, axis::integer<>{0, 3});
    h(0, 0);
    auto h2 = decltype(h)(h);
    BOOST_TEST_EQ(h2, h);
    auto h3 =
        histogram<std::tuple<axis::integer<>, axis::integer<>>, dense_storage<double>>(h);
    BOOST_TEST_EQ(h3, h);
  }

  // copy_assign
  {
    auto h = make(Tag(), axis::integer<>(0, 1), axis::integer<>(0, 2));
    h(0, 0);
    auto h2 = decltype(h)();
    BOOST_TEST_NE(h, h2);
    h2 = h;
    BOOST_TEST_EQ(h, h2);
    auto h3 =
        histogram<std::tuple<axis::integer<>, axis::integer<>>, dense_storage<double>>();
    h3 = h;
    BOOST_TEST_EQ(h, h3);
  }

  // move
  {
    auto h = make(Tag(), axis::integer<>(0, 1), axis::integer<>(0, 2));
    h(0, 0);
    const auto href = h;
    decltype(h) h2(std::move(h));
    BOOST_TEST_EQ(algorithm::sum(h), 0);
    BOOST_TEST_EQ(h.size(), 0);
    BOOST_TEST_EQ(h2, href);
    decltype(h) h3;
    h3 = std::move(h2);
    BOOST_TEST_EQ(algorithm::sum(h2), 0);
    BOOST_TEST_EQ(h2.size(), 0);
    BOOST_TEST_EQ(h3, href);
  }

  // axis methods
  {
    auto a = make(Tag(), axis::regular<>(1, 1, 2, "foo"));
    BOOST_TEST_EQ(a.axis().size(), 1);
    BOOST_TEST_EQ(a.axis().bin(0).lower(), 1);
    BOOST_TEST_EQ(a.axis().bin(0).upper(), 2);
    BOOST_TEST_EQ(a.axis().metadata(), "foo");
    unsafe_access::axis(a, 0).metadata() = "bar";
    BOOST_TEST_EQ(a.axis().metadata(), "bar");

    auto b = make(Tag(), axis::regular<>(1, 1, 2, "foo"), axis::integer<>(1, 3));

    // check static access
    BOOST_TEST_EQ(b.axis(0_c).size(), 1);
    BOOST_TEST_EQ(b.axis(0_c).bin(0).lower(), 1);
    BOOST_TEST_EQ(b.axis(0_c).bin(0).upper(), 2);
    BOOST_TEST_EQ(b.axis(1_c).size(), 2);
    BOOST_TEST_EQ(b.axis(1_c).bin(0), 1);
    BOOST_TEST_EQ(b.axis(1_c).bin(1), 2);
    unsafe_access::axis(b, 1_c).metadata() = "bar";
    BOOST_TEST_EQ(b.axis(0_c).metadata(), "foo");
    BOOST_TEST_EQ(b.axis(1_c).metadata(), "bar");

    // check dynamic access
    BOOST_TEST_EQ(b.axis(0).size(), 1);
    BOOST_TEST_EQ(b.axis(0).bin(0).lower(), 1);
    BOOST_TEST_EQ(b.axis(0).bin(0).upper(), 2);
    BOOST_TEST_EQ(b.axis(1).size(), 2);
    BOOST_TEST_EQ(b.axis(1).bin(0), 1);
    BOOST_TEST_EQ(b.axis(1).bin(1), 2);
    BOOST_TEST_EQ(b.axis(0).metadata(), "foo");
    BOOST_TEST_EQ(b.axis(1).metadata(), "bar");
    unsafe_access::axis(b, 0).metadata() = "baz";
    BOOST_TEST_EQ(b.axis(0).metadata(), "baz");

    enum class C { A = 3, B = 5 };
    auto c = make(Tag(), axis::category<C>({C::A, C::B}));
    BOOST_TEST_EQ(c.axis().size(), 2);
    unsafe_access::axis(c, 0).metadata() = "foo";
    BOOST_TEST_EQ(c.axis().metadata(), "foo");
    // need to cast here for this to work with Tag == dynamic_tag, too
    auto ca = axis::get<axis::category<C>>(c.axis());
    BOOST_TEST(ca.bin(0) == C::A);
  }

  // equal_compare
  {
    auto a = make(Tag(), axis::integer<>(0, 2));
    auto b = make(Tag(), axis::integer<>(0, 2), axis::integer<>(0, 3));
    BOOST_TEST(a != b);
    BOOST_TEST(b != a);
    auto c = make(Tag(), axis::integer<>(0, 2));
    BOOST_TEST(b != c);
    BOOST_TEST(c != b);
    BOOST_TEST(a == c);
    BOOST_TEST(c == a);
    auto d = make(Tag(), axis::regular<>(2, 0, 1));
    BOOST_TEST(c != d);
    BOOST_TEST(d != c);
    c(0);
    BOOST_TEST(a != c);
    BOOST_TEST(c != a);
    a(0);
    BOOST_TEST(a == c);
    BOOST_TEST(c == a);
    a(0);
    BOOST_TEST(a != c);
    BOOST_TEST(c != a);
  }

  // d1
  {
    auto h = make_s(Tag(), std::vector<unsigned>(),
                    axis::integer<double, axis::null_type>{0, 2});
    h(0);
    auto i = h(0);
    BOOST_TEST(i == h.begin() + 1); // +1 because of underflow
    i = h(-1);
    BOOST_TEST(i == h.begin()); // underflow
    i = h(10);
    BOOST_TEST(i == h.end() - 1); // overflow

    BOOST_TEST_EQ(h.rank(), 1);
    BOOST_TEST_EQ(h.axis().size(), 2);
    BOOST_TEST_EQ(algorithm::sum(h), 4);

    BOOST_TEST_EQ(h.at(-1), 1);
    BOOST_TEST_EQ(h.at(0), 2);
    BOOST_TEST_EQ(h.at(1), 0);
    BOOST_TEST_EQ(h.at(2), 1);
  }

  // d1_2
  {
    auto h = make(Tag(), axis::integer<int, axis::null_type, axis::option::none_t>(0, 2));
    h(0);
    auto i = h(-0);
    BOOST_TEST(i == h.begin());
    i = h(-1);
    BOOST_TEST(i == h.end());
    i = h(10);
    BOOST_TEST(i == h.end());

    BOOST_TEST_EQ(h.rank(), 1);
    BOOST_TEST_EQ(h.axis().size(), 2);
    BOOST_TEST_EQ(algorithm::sum(h), 2);

    BOOST_TEST_EQ(h.at(0), 2);
    BOOST_TEST_EQ(h.at(1), 0);
  }

  // d1_3
  {
    auto h = make(Tag(), axis::category<std::string>({"A", "B"}));
    h("A");
    h("B");
    h("D");
    h("E");

    BOOST_TEST_EQ(h.rank(), 1);
    BOOST_TEST_EQ(h.axis().size(), 2);
    BOOST_TEST_EQ(algorithm::sum(h), 4);

    BOOST_TEST_EQ(h.at(0), 1);
    BOOST_TEST_EQ(h.at(1), 1);
    BOOST_TEST_EQ(h.at(2), 2); // overflow bin
  }

  // d1 weight
  {
    auto h =
        make_s(Tag(), std::vector<accumulators::weighted_sum<>>(), axis::integer<>(0, 2));
    h(-1);
    h(0);
    h(weight(0.5), 0);
    h(1);
    h(weight(2), 2);

    BOOST_TEST_EQ(algorithm::sum(h).value(), 5.5);
    BOOST_TEST_EQ(algorithm::sum(h).variance(), 7.25);

    BOOST_TEST_EQ(h[-1].value(), 1);
    BOOST_TEST_EQ(h[-1].variance(), 1);
    BOOST_TEST_EQ(h[0].value(), 1.5);
    BOOST_TEST_EQ(h[0].variance(), 1.25);
    BOOST_TEST_EQ(h[1].value(), 1);
    BOOST_TEST_EQ(h[1].variance(), 1);
    BOOST_TEST_EQ(h[2].value(), 2);
    BOOST_TEST_EQ(h[2].variance(), 4);
  }

  // d1 mean
  {
    auto h =
        make_s(Tag(), std::vector<accumulators::mean<double>>(), axis::integer<>(0, 2));

    h(0, sample(1));
    h(0, sample(2));
    h(0, sample(3));
    h(sample(4), 1);
    h(sample(5), 1);
    h(sample(6), 1);

    BOOST_TEST_EQ(h[0].count(), 3);
    BOOST_TEST_EQ(h[0].value(), 2);
    BOOST_TEST_EQ(h[0].variance(), 1);
    BOOST_TEST_EQ(h[1].count(), 3);
    BOOST_TEST_EQ(h[1].value(), 5);
    BOOST_TEST_EQ(h[1].variance(), 1);
  }

  // d1 weighted mean
  {
    auto h = make_s(Tag(), std::vector<accumulators::weighted_mean<double>>(),
                    axis::integer<>(0, 2));

    h(0, sample(1));
    h(sample(1), 0);

    h(0, weight(2), sample(3));
    h(0, sample(5), weight(2));

    h(weight(2), 1, sample(1));
    h(sample(2), 1, weight(2));

    h(weight(2), sample(3), 1);
    h(sample(4), weight(2), 1);

    BOOST_TEST_EQ(h[0].sum_of_weights(), 6);
    BOOST_TEST_EQ(h[0].value(), 3);
    BOOST_TEST_EQ(h[1].sum_of_weights(), 8);
    BOOST_TEST_EQ(h[1].value(), 2.5);
  }

  // d2
  {
    auto h = make(Tag(), axis::regular<>(2, -1, 1),
                  axis::integer<int, axis::null_type, axis::option::none_t>(-1, 2));
    h(-1, -1);
    h(-1, 0);
    h(-1, -10);
    h(-10, 0);

    BOOST_TEST_EQ(h.rank(), 2);
    BOOST_TEST_EQ(h.axis(0_c).size(), 2);
    BOOST_TEST_EQ(h.axis(1_c).size(), 3);
    BOOST_TEST_EQ(algorithm::sum(h), 3);

    BOOST_TEST_EQ(h.at(-1, 0), 0);
    BOOST_TEST_EQ(h.at(-1, 1), 1);
    BOOST_TEST_EQ(h.at(-1, 2), 0);

    BOOST_TEST_EQ(h.at(0, 0), 1);
    BOOST_TEST_EQ(h.at(0, 1), 1);
    BOOST_TEST_EQ(h.at(0, 2), 0);

    BOOST_TEST_EQ(h.at(1, 0), 0);
    BOOST_TEST_EQ(h.at(1, 1), 0);
    BOOST_TEST_EQ(h.at(1, 2), 0);

    BOOST_TEST_EQ(h.at(2, 0), 0);
    BOOST_TEST_EQ(h.at(2, 1), 0);
    BOOST_TEST_EQ(h.at(2, 2), 0);
  }

  // d2w
  {
    auto h = make_s(Tag(), std::vector<accumulators::weighted_sum<>>(),
                    axis::regular<>(2, -1, 1),
                    axis::integer<int, axis::null_type, axis::option::none_t>(-1, 2));
    h(-1, 0);              // -> 0, 1
    h(weight(10), -1, -1); // -> 0, 0
    h(weight(5), -1, -10); // is ignored
    h(weight(7), -10, 0);  // -> -1, 1

    BOOST_TEST_EQ(algorithm::sum(h).value(), 18);
    BOOST_TEST_EQ(algorithm::sum(h).variance(), 150);

    BOOST_TEST_EQ(h.at(-1, 0).value(), 0);
    BOOST_TEST_EQ(h.at(-1, 1).value(), 7);
    BOOST_TEST_EQ(h.at(-1, 2).value(), 0);

    BOOST_TEST_EQ(h.at(0, 0).value(), 10);
    BOOST_TEST_EQ(h.at(0, 1).value(), 1);
    BOOST_TEST_EQ(h.at(0, 2).value(), 0);

    BOOST_TEST_EQ(h.at(1, 0).value(), 0);
    BOOST_TEST_EQ(h.at(1, 1).value(), 0);
    BOOST_TEST_EQ(h.at(1, 2).value(), 0);

    BOOST_TEST_EQ(h.at(2, 0).value(), 0);
    BOOST_TEST_EQ(h.at(2, 1).value(), 0);
    BOOST_TEST_EQ(h.at(2, 2).value(), 0);

    BOOST_TEST_EQ(h.at(-1, 0).variance(), 0);
    BOOST_TEST_EQ(h.at(-1, 1).variance(), 49);
    BOOST_TEST_EQ(h.at(-1, 2).variance(), 0);

    BOOST_TEST_EQ(h.at(0, 0).variance(), 100);
    BOOST_TEST_EQ(h.at(0, 1).variance(), 1);
    BOOST_TEST_EQ(h.at(0, 2).variance(), 0);

    BOOST_TEST_EQ(h.at(1, 0).variance(), 0);
    BOOST_TEST_EQ(h.at(1, 1).variance(), 0);
    BOOST_TEST_EQ(h.at(1, 2).variance(), 0);

    BOOST_TEST_EQ(h.at(2, 0).variance(), 0);
    BOOST_TEST_EQ(h.at(2, 1).variance(), 0);
    BOOST_TEST_EQ(h.at(2, 2).variance(), 0);
  }

  // d3w
  {
    auto h = make_s(Tag(), std::vector<accumulators::weighted_sum<>>(),
                    axis::integer<>(0, 3), axis::integer<>(0, 4), axis::integer<>(0, 5));
    for (auto i = 0; i < h.axis(0_c).size(); ++i) {
      for (auto j = 0; j < h.axis(1_c).size(); ++j) {
        for (auto k = 0; k < h.axis(2_c).size(); ++k) { h(i, j, k, weight(i + j + k)); }
      }
    }

    for (auto i = 0; i < h.axis(0_c).size(); ++i) {
      for (auto j = 0; j < h.axis(1_c).size(); ++j) {
        for (auto k = 0; k < h.axis(2_c).size(); ++k) {
          BOOST_TEST_EQ(h.at(i, j, k).value(), i + j + k);
          BOOST_TEST_EQ(h.at(i, j, k).variance(), (i + j + k) * (i + j + k));
        }
      }
    }
  }

  // STL support
  {
    auto v = std::vector<int>{0, 1, 2};
    auto h = std::for_each(v.begin(), v.end(), make(Tag(), axis::integer<>(0, 3)));
    BOOST_TEST_EQ(h.at(0), 1);
    BOOST_TEST_EQ(h.at(1), 1);
    BOOST_TEST_EQ(h.at(2), 1);
    BOOST_TEST_EQ(algorithm::sum(h), 3);

    auto a = std::vector<double>();
    // walks over all bins, including underflow and overflow
    std::partial_sum(h.begin(), h.end(), std::back_inserter(a));
    BOOST_TEST_EQ(a.size(), 5);
    BOOST_TEST_EQ(a[0], 0);
    BOOST_TEST_EQ(a[1], 1);
    BOOST_TEST_EQ(a[2], 2);
    BOOST_TEST_EQ(a[3], 3);
    BOOST_TEST_EQ(a[4], 3);
  }

  // histogram_reset
  {
    auto h = make(Tag(), axis::integer<int, axis::null_type, axis::option::none_t>(0, 2));
    h(0);
    h(1);
    BOOST_TEST_EQ(h.at(0), 1);
    BOOST_TEST_EQ(h.at(1), 1);
    BOOST_TEST_EQ(algorithm::sum(h), 2);
    h.reset();
    BOOST_TEST_EQ(h.at(0), 0);
    BOOST_TEST_EQ(h.at(1), 0);
    BOOST_TEST_EQ(algorithm::sum(h), 0);
  }

  // custom axes
  {
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

    auto h = make(Tag(), modified_axis(0, 3), minimal(), axis2d());
    h("0", 1, std::make_tuple(1.0, 2.0));
    h("1", 2, std::make_tuple(2.0, 1.0));

    BOOST_TEST_EQ(h.rank(), 3);
    BOOST_TEST_EQ(h.at(0, 0, 0), 0);
    BOOST_TEST_EQ(h.at(0, 1, 1), 1);
    BOOST_TEST_EQ(h.at(1, 0, 0), 1);
  }

  // using containers or input and output
  {
    auto h = make_s(Tag(), std::vector<accumulators::weighted_sum<>>(),
                    axis::integer<>(0, 2), axis::regular<>(2, 2, 4));
    // tuple in
    h(std::make_tuple(0, 2.0));
    h(std::make_tuple(1, 3.0));

    auto i00 = std::make_tuple(0, 0);
    auto i11 = std::make_tuple(1, 1);

    // tuple out
    BOOST_TEST_EQ(h.at(i00).value(), 1);
    BOOST_TEST_EQ(h[i00].value(), 1);
    BOOST_TEST_EQ(h[i11].value(), 1);

    // iterable out
    int j11[] = {1, 1};
    BOOST_TEST_EQ(h.at(j11), 1);
    BOOST_TEST_EQ(h[j11], 1);
#ifndef BOOST_NO_EXCEPTIONS
    int j111[] = {1, 1, 1};
    BOOST_TEST_THROWS((void)h.at(j111), std::invalid_argument);
    int j13[] = {1, 3};
    BOOST_TEST_THROWS((void)h.at(j13), std::out_of_range);
#endif

    // tuple with weight
    h(std::make_tuple(weight(2), 0, 2.0));
    h(std::make_tuple(1, 3.0, weight(2)));

    BOOST_TEST_EQ(h.at(i00).value(), 3);
    BOOST_TEST_EQ(h[i00].value(), 3);
    BOOST_TEST_EQ(h.at(i11).variance(), 5);
    BOOST_TEST_EQ(h[i11].variance(), 5);

    // test special case of 1-dimensional histogram, which should unpack
    // 1-dimensional tuple normally, but forward larger tuples to the axis
    auto h1 = make(Tag(), axis::integer<>(0, 2));
    h1(std::make_tuple(0));                      // as if one had passed 0 directly
    BOOST_TEST_EQ(h1.at(std::make_tuple(0)), 1); // as if one had passed 0 directly

    struct axis2d {
      auto index(std::tuple<int, int> x) const {
        return axis::index_type{std::get<0>(x) == 1 && std::get<1>(x) == 2};
      }
      auto size() const { return axis::index_type{2}; }
    };
    auto h2 = make(Tag(), axis2d());
    h2(std::make_tuple(1, 2));  // ok, forwards 2d tuple to axis
    BOOST_TEST_EQ(h2.at(0), 0); // ok, bin access is still 1d
    BOOST_TEST_EQ(h2[std::make_tuple(1)], 1);
    // passing two arguments directly also works
    h2(1, 2);
    // also works with weights
    h2(1, 2, weight(2));
    h2(std::make_tuple(weight(3), 1, 2));
    BOOST_TEST_EQ(h2.at(1), 7);
  }

  // bad bin access
  {
    auto h = make(Tag(), axis::integer<>(0, 1), axis::integer<>(0, 1));
    BOOST_TEST_THROWS(h.at(0, 2), std::out_of_range);
    BOOST_TEST_THROWS(h.at(std::make_tuple(2, 0)), std::out_of_range);
  }

  // pass histogram to function
  {
    auto h = make(Tag(), axis::integer<>(0, 3));
    h(1);
    pass_histogram(h);
  }

  // allocator support
  {
    tracing_allocator_db db;
    {
      tracing_allocator<char> a(db);
      auto h = make_s(Tag(), std::vector<int, tracing_allocator<int>>(a),
                      axis::integer<>(0, 1000));
      h(0);
    }

    // int allocation for std::vector
    BOOST_TEST_EQ(db.at<int>().first, 0);
    BOOST_TEST_EQ(db.at<int>().second, 1002);

    if (Tag()) { // axis::variant allocation, only for dynamic histogram
      using T = axis::variant<axis::integer<>>;
      BOOST_TEST_EQ(db.at<T>().first, 0);
      // may be zero if vector uses small-vector-optimisation
      BOOST_TEST_LE(db.at<T>().second, 1);
    }
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
