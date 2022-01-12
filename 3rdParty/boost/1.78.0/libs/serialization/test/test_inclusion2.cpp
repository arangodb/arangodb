/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_const.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <sstream>
#include <vector>

void f()
{
  std::stringstream iss;
  boost::archive::binary_iarchive ar(iss);
  std::vector<int> out;
  ar >> out;
}

int
main(int /*argc*/, char * /*argv*/[]){
  return 0;
}
