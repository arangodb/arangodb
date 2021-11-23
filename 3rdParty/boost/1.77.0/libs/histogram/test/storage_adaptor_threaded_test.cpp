// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/count.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include "throw_exception.hpp"

#include <array>
#include <deque>
#include <map>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace boost::histogram;

constexpr auto n_fill = 1000000;

template <class T>
void tests() {
  {
    storage_adaptor<T> s;
    s.reset(1);

    auto fill = [&s]() {
      for (unsigned i = 0; i < n_fill; ++i) {
        ++s[0];
        s[0] += 1;
      }
    };

    std::thread t1(fill);
    std::thread t2(fill);
    std::thread t3(fill);
    std::thread t4(fill);
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    BOOST_TEST_EQ(s[0], 4 * 2 * n_fill);
  }
}

int main() {
  using ts_int = accumulators::count<int, true>;
  tests<std::vector<ts_int>>();
  tests<std::array<ts_int, 100>>();
  tests<std::deque<ts_int>>();
  // stdlib maps are not thread-safe and not supported

  return boost::report_errors();
}
