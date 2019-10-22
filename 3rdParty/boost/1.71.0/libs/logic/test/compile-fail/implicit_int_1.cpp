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

  return i; // expect to see: error: no viable conversion from returned value of type 'boost::logic::tribool' to function return type 'int'
}
