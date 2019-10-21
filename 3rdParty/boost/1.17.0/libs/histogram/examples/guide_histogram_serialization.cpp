// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_histogram_serialization

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/histogram.hpp>
#include <boost/histogram/serialization.hpp> // includes serialization code
#include <cassert>
#include <sstream>

int main() {
  using namespace boost::histogram;

  auto a = make_histogram(axis::regular<>(3, -1.0, 1.0, "axis 0"),
                          axis::integer<>(0, 2, "axis 1"));
  a(0.5, 1);

  std::string buf; // to hold persistent representation

  // store histogram
  {
    std::ostringstream os;
    boost::archive::text_oarchive oa(os);
    oa << a;
    buf = os.str();
  }

  auto b = decltype(a)(); // create a default-constructed second histogram

  assert(b != a); // b is empty, a is not

  // load histogram
  {
    std::istringstream is(buf);
    boost::archive::text_iarchive ia(is);
    ia >> b;
  }

  assert(b == a); // now b is equal to a
}

//]
