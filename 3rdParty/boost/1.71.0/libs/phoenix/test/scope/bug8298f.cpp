/*=============================================================================
    Copyright (c) 2014 John Fletcher
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix.hpp>

#include <boost/detail/lightweight_test.hpp>

namespace phoenix = boost::phoenix;
using namespace phoenix::local_names;
using boost::phoenix::arg_names::_1;

int main(int argc, char *argv[])
{
  int y = phoenix::lambda(_a=_1)
  [
    _a = 18
  ](17)();
  BOOST_TEST(y==18);

  return boost::report_errors();
}
