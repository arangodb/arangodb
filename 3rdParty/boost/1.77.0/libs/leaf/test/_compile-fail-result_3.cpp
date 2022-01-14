// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.
// Thanks @strager

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf.hpp>
#include <string>
#include <vector>

boost::leaf::result<std::size_t> g() {
  return 42;
}

boost::leaf::result<std::vector<std::string>> f() {
  return g(); // vector constructor is explicit
}
