// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/option.hpp>
#include <iostream>

using namespace boost::histogram::axis;

template <unsigned N, unsigned M>
bool operator==(option::bitset<N>, option::bitset<M>) {
  return N == M;
}

template <unsigned N>
std::ostream& operator<<(std::ostream& os, option::bitset<N>) {
  os << "underflow " << static_cast<bool>(N & option::underflow) << " "
     << "overflow " << static_cast<bool>(N & option::overflow) << " "
     << "circular " << static_cast<bool>(N & option::circular) << " "
     << "growth " << static_cast<bool>(N & option::growth);
  return os;
}

int main() {
  using namespace option;
  using uoflow = decltype(underflow | overflow);
  constexpr auto uoflow_growth = uoflow{} | growth;

  BOOST_TEST_EQ(uoflow::value, underflow | overflow);
  BOOST_TEST_EQ(underflow | overflow, overflow | underflow);

  BOOST_TEST(underflow.test(underflow));
  BOOST_TEST_NOT(underflow.test(overflow));
  BOOST_TEST(uoflow::test(underflow));
  BOOST_TEST(uoflow::test(overflow));
  BOOST_TEST_NOT(uoflow::test(circular));
  BOOST_TEST_NOT(uoflow::test(growth));
  BOOST_TEST(uoflow_growth.test(underflow));
  BOOST_TEST(uoflow_growth.test(overflow));
  BOOST_TEST(uoflow_growth.test(growth));
  BOOST_TEST_NOT(uoflow_growth.test(circular));

  BOOST_TEST_EQ(uoflow_growth & uoflow_growth, uoflow_growth);
  BOOST_TEST_EQ(uoflow_growth & growth, growth);
  BOOST_TEST_EQ(uoflow_growth & uoflow{}, uoflow::value);

  BOOST_TEST_EQ(uoflow_growth - growth, uoflow{});
  BOOST_TEST_EQ(uoflow_growth - uoflow{}, growth);
  BOOST_TEST_EQ(uoflow_growth - underflow, growth | overflow);

  return boost::report_errors();
}
