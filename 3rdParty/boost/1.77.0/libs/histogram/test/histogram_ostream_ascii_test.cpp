// Copyright 2021 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/ostream.hpp>
#include <limits>
#include <sstream>
#include <string>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;

template <class Histogram>
auto str(const Histogram& h, const unsigned width = 0) {
  std::ostringstream os;
  // BEGIN and END make nicer error messages
  os << "BEGIN\n" << std::setw(width) << h << "END";
  return os.str();
}

template <class Tag>
void run_tests() {
  using I = axis::integer<>;

  {
    auto h = make(Tag(), I(0, 3));
    h.at(0) = 1;
    h.at(1) = 3;
    h.at(2) = 2;

    const auto expected = "BEGIN\n"
                          "histogram(integer(0, 3, options=underflow | overflow))\n"
                          "     +-------------+\n"
                          "-1 0 |             |\n"
                          " 0 1 |====         |\n"
                          " 1 3 |============ |\n"
                          " 2 2 |========     |\n"
                          " 3 0 |             |\n"
                          "     +-------------+\n"
                          "END";

    BOOST_TEST_CSTR_EQ(str(h).c_str(), expected);
  }

  {
    auto h = make(Tag(), I(0, 3));
    h.at(0) = -1;
    h.at(1) = 3;
    h.at(2) = 2;

    const auto expected = "BEGIN\n"
                          "histogram(integer(0, 3, options=underflow | overflow))\n"
                          "      +------------+\n"
                          "-1 0  |            |\n"
                          " 0 -1 |===         |\n"
                          " 1 3  |   ======== |\n"
                          " 2 2  |   ======   |\n"
                          " 3 0  |            |\n"
                          "      +------------+\n"
                          "END";

    BOOST_TEST_CSTR_EQ(str(h).c_str(), expected);
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
