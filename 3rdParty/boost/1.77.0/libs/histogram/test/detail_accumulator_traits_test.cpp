// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/accumulator_traits.hpp>
#include <boost/histogram/weight.hpp>
#include <tuple>

namespace dtl = boost::histogram::detail;

int main() {
  using boost::histogram::weight_type;

  BOOST_TEST(dtl::accumulator_traits<int>::weight_support);
  BOOST_TEST_TRAIT_SAME(dtl::accumulator_traits<int>::args, std::tuple<>);

  BOOST_TEST(dtl::accumulator_traits<float>::weight_support);
  BOOST_TEST_TRAIT_SAME(dtl::accumulator_traits<float>::args, std::tuple<>);

  struct A1 {
    void operator()();
  };

  BOOST_TEST_NOT(dtl::accumulator_traits<A1>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<A1>::args, std::tuple<>);

  struct A2 {
    void operator()(int, double);
  };

  BOOST_TEST_NOT(dtl::accumulator_traits<A2>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<A2>::args,
                        std::tuple<int, double>);

  struct A3 {
    void operator()();
    void operator()(weight_type<int>);
  };

  BOOST_TEST(dtl::accumulator_traits<A3>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<A3>::args, std::tuple<>);

  struct A4 {
    void operator()(int, double, char);
    void operator()(weight_type<int>, int, double, char);
  };

  BOOST_TEST(dtl::accumulator_traits<A4>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<A4>::args,
                        std::tuple<int, double, char>);

  struct A5 {
    void operator()(const weight_type<int>, int);
  };

  BOOST_TEST(dtl::accumulator_traits<A5>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<A5>::args, std::tuple<int>);

  struct A6 {
    void operator()(const weight_type<int>&, const int);
  };

  BOOST_TEST(dtl::accumulator_traits<A6>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<A6>::args, std::tuple<int>);

  struct A7 {
    void operator()(weight_type<int>&&, int&&);
  };

  BOOST_TEST(dtl::accumulator_traits<A7>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<A7>::args, std::tuple<int&&>);

  struct B1 {
    int operator+=(int);
  };

  BOOST_TEST(dtl::accumulator_traits<B1>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<B1>::args, std::tuple<>);

  struct B2 {
    int operator+=(weight_type<int>);
  };

  BOOST_TEST(dtl::accumulator_traits<B2>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<B2>::args, std::tuple<>);

  struct B3 {
    int operator+=(const weight_type<double>&);
  };

  BOOST_TEST(dtl::accumulator_traits<B3>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<B3>::args, std::tuple<>);

  // potentially ambiguous case that mimicks accumulators::weighted_sum
  struct B4 {
    B4(int) {}
    int operator+=(const weight_type<double>&);
    int operator+=(const B4&); // *this += 0 succeeds as *this += B4(0)
  };

  BOOST_TEST(dtl::accumulator_traits<B4>::weight_support);
  BOOST_TEST_TRAIT_SAME(typename dtl::accumulator_traits<B4>::args, std::tuple<>);

  return boost::report_errors();
}
