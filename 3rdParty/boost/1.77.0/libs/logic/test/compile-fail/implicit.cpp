// Copyright 2018 James E. King III. Use, modification and
// distribution is subject to the Boost Software License, Version
// 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/logic/tribool.hpp>

int main(int, char*[])
{
  using boost::logic::indeterminate;
  using boost::logic::tribool;

  tribool i(indeterminate);

#if !defined( BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS )
  bool b = i; // expect to see: no viable conversion from 'boost::logic::tribool' to 'bool'
  return (int)b;
#else
#error in c++03 explicit conversions are allowed
#endif
  // NOTREACHED
  return 0;
}
