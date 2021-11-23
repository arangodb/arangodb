// Copyright Douglas Gregor 2002-2003. Use, modification and
// distribution is subject to the Boost Software License, Version
// 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/logic/tribool.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>

BOOST_TRIBOOL_THIRD_STATE(maybe)

int main()
{
  using namespace boost::logic;

  tribool x; // false
  tribool y(true); // true
  tribool z(maybe); // maybe

  BOOST_TEST(!x);
  BOOST_TEST(x == false);
  BOOST_TEST(false == x);
  BOOST_TEST(x != true);
  BOOST_TEST(true != x);
  BOOST_TEST(maybe(x == maybe));
  BOOST_TEST(maybe(maybe == x));
  BOOST_TEST(maybe(x != maybe));
  BOOST_TEST(maybe(maybe != x));
  BOOST_TEST(x == x);
  BOOST_TEST(!(x != x));
  BOOST_TEST(!(x && true));
  BOOST_TEST(!(true && x));
  BOOST_TEST(x || true);
  BOOST_TEST(true || x);

  BOOST_TEST(y);
  BOOST_TEST(y == true);
  BOOST_TEST(true == y);
  BOOST_TEST(y != false);
  BOOST_TEST(false != y);
  BOOST_TEST(maybe(y == maybe));
  BOOST_TEST(maybe(maybe == y));
  BOOST_TEST(maybe(y != maybe));
  BOOST_TEST(maybe(maybe != y));
  BOOST_TEST(y == y);
  BOOST_TEST(!(y != y));

  BOOST_TEST(maybe(z || !z));
  BOOST_TEST(maybe(z == true));
  BOOST_TEST(maybe(true == z));
  BOOST_TEST(maybe(z == false));
  BOOST_TEST(maybe(false == z));
  BOOST_TEST(maybe(z == maybe));
  BOOST_TEST(maybe(maybe == z));
  BOOST_TEST(maybe(z != maybe));
  BOOST_TEST(maybe(maybe != z));
  BOOST_TEST(maybe(z == z));
  BOOST_TEST(maybe(z != z));

  BOOST_TEST(!(x == y));
  BOOST_TEST(x != y);
  BOOST_TEST(maybe(x == z));
  BOOST_TEST(maybe(x != z));
  BOOST_TEST(maybe(y == z));
  BOOST_TEST(maybe(y != z));

  BOOST_TEST(!(x && y));
  BOOST_TEST(x || y);
  BOOST_TEST(!(x && z));
  BOOST_TEST(maybe(y && z));
  BOOST_TEST(maybe(z && z));
  BOOST_TEST(maybe(z || z));
  BOOST_TEST(maybe(x || z));
  BOOST_TEST(y || z);

  BOOST_TEST(maybe(y && maybe));
  BOOST_TEST(maybe(maybe && y));
  BOOST_TEST(!(x && maybe));
  BOOST_TEST(!(maybe && x));

  BOOST_TEST(maybe || y);
  BOOST_TEST(y || maybe);
  BOOST_TEST(maybe(x || maybe));
  BOOST_TEST(maybe(maybe || x));

  // Test the if (z) ... else (!z) ... else ... idiom
  if (z) {
    BOOST_TEST(false);
  }
  else if (!z) {
    BOOST_TEST(false);
  }
  else {
    BOOST_TEST(true);
  }

  z = true;
  if (z) {
    BOOST_TEST(true);
  }
  else if (!z) {
    BOOST_TEST(false);
  }
  else {
    BOOST_TEST(false);
  }

  z = false;
  if (z) {
    BOOST_TEST(false);
  }
  else if (!z) {
    BOOST_TEST(true);
  }
  else {
    BOOST_TEST(false);
  }

  return boost::report_errors();
}
