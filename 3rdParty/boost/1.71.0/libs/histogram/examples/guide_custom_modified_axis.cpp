// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_modified_axis

#include <boost/histogram.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
  using namespace boost::histogram;

  // custom axis, which adapts builtin integer axis
  struct custom_axis : public axis::integer<> {
    using value_type = const char*; // type that is fed to the axis

    using integer::integer; // inherit ctors of base

    // the customization point
    // - accept const char* and convert to int
    // - then call index method of base class
    axis::index_type index(value_type s) const { return integer::index(std::atoi(s)); }
  };

  auto h = make_histogram(custom_axis(3, 6));
  h("-10");
  h("3");
  h("4");
  h("9");

  std::ostringstream os;
  for (auto&& b : indexed(h)) {
    os << "bin " << b.index() << " [" << b.bin() << "] " << *b << "\n";
  }

  std::cout << os.str() << std::endl;

  assert(os.str() == "bin 0 [3] 1\n"
                     "bin 1 [4] 1\n"
                     "bin 2 [5] 0\n");
}

//]
