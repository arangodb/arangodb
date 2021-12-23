// Copyright Antony Polukhin, 2018.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


// This file tests for memory leaks. Some of the backtrace implementations
// consume memory for internal needs and incorrect usage of those implementations
// could lead to segfaults. Sanitizers do not detect such misuse, but this
// test and `top` does.


#include <boost/stacktrace.hpp>
#include <iostream>

#include "test_impl.hpp"

int main() {
  int result = 0;
  for (unsigned i = 0; i < 10000000; ++i) {
    result += make_some_stacktrace1()[0].source_line();
  }

  std::cerr << "OK\nLines count " << result;
}
