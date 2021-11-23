/* Boost test/msvc_x64_flags.cpp
 * Test for amd64\ieee.c(102) : Assertion failed: (mask&~(_MCW_DN|_MCW_EM|_MCW_RC))==0.
 * This happens with MSVC on x64 in Debug mode. See ticket #4964.
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or
 * copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/numeric/interval.hpp>
#include <boost/core/lightweight_test.hpp>
#include "bugs.hpp"

int test_main(int, char *[]) {
  boost::numeric::interval<double> i(0.0, 0.0);
  boost::numeric::interval<double> i2 = 60.0 - i;
# ifdef BOOST_BORLANDC
  ::detail::ignore_warnings();
# endif
  return 0;
}
