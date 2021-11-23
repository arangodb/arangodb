// Copyright Douglas Gregor 2002-2003. Use, modification and
// distribution is subject to the Boost Software License, Version
// 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/static_assert.hpp>
#include <iostream>

int main()
{
  using namespace boost::logic;

  tribool x; // false
  tribool y(true); // true
  tribool z(indeterminate); // indeterminate

#if defined( BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS )
  // c++03 allows for implicit conversion to bool
  // c++11 uses an explicit conversion operator so this would not compile
  //       and that is tested in the compile-fail/implicit.cpp file
  // so we check the conversion to ensure it is sane
  bool bx = x;
  BOOST_TEST(bx == false);
  bool by = y;
  BOOST_TEST(by == true);
  bool bz = z;
  BOOST_TEST(bz == false);
#endif

  BOOST_TEST(!x);
  BOOST_TEST(x == false);
  BOOST_TEST(false == x);
  BOOST_TEST(x != true);
  BOOST_TEST(true != x);
  BOOST_TEST(indeterminate(x == indeterminate));
  BOOST_TEST(indeterminate(indeterminate == x));
  BOOST_TEST(indeterminate(x != indeterminate));
  BOOST_TEST(indeterminate(indeterminate != x));
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
  BOOST_TEST(indeterminate(y == indeterminate));
  BOOST_TEST(indeterminate(indeterminate == y));
  BOOST_TEST(indeterminate(y != indeterminate));
  BOOST_TEST(indeterminate(indeterminate != y));
  BOOST_TEST(y == y);
  BOOST_TEST(!(y != y));

  BOOST_TEST(indeterminate(z || !z));
  BOOST_TEST(indeterminate(z == true));
  BOOST_TEST(indeterminate(true == z));
  BOOST_TEST(indeterminate(z == false));
  BOOST_TEST(indeterminate(false == z));
  BOOST_TEST(indeterminate(z == indeterminate));
  BOOST_TEST(indeterminate(indeterminate == z));
  BOOST_TEST(indeterminate(z != indeterminate));
  BOOST_TEST(indeterminate(indeterminate != z));
  BOOST_TEST(indeterminate(z == z));
  BOOST_TEST(indeterminate(z != z));

  BOOST_TEST(!(x == y));
  BOOST_TEST(x != y);
  BOOST_TEST(indeterminate(x == z));
  BOOST_TEST(indeterminate(x != z));
  BOOST_TEST(indeterminate(y == z));
  BOOST_TEST(indeterminate(y != z));

  BOOST_TEST(!(x && y));
  BOOST_TEST(x || y);
  BOOST_TEST(!(x && z));
  BOOST_TEST(indeterminate(y && z));
  BOOST_TEST(indeterminate(z && z));
  BOOST_TEST(indeterminate(z || z));
  BOOST_TEST(indeterminate(x || z));
  BOOST_TEST(y || z);

  BOOST_TEST(indeterminate(y && indeterminate));
  BOOST_TEST(indeterminate(indeterminate && y));
  BOOST_TEST(!(x && indeterminate));
  BOOST_TEST(!(indeterminate && x));

  BOOST_TEST(indeterminate || y);
  BOOST_TEST(y || indeterminate);
  BOOST_TEST(indeterminate(x || indeterminate));
  BOOST_TEST(indeterminate(indeterminate || x));

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

#if !defined(BOOST_NO_CXX11_CONSTEXPR) 
  constexpr bool res_ors = indeterminate(false || tribool(false) || false || indeterminate); // true
  BOOST_TEST(res_ors);
  char array_ors[res_ors ? 2 : 3];
  BOOST_TEST(sizeof(array_ors) / sizeof(char) == 2);

  constexpr bool res_ands = !indeterminate(!(true && tribool(true) && true && indeterminate)); // false
  BOOST_TEST(!res_ands);
  char array_ands[res_ands ? 2 : 3];
  BOOST_TEST(sizeof(array_ands) / sizeof(char) == 3);

  constexpr bool res_safe_bool = static_cast<bool>( tribool(true) );
  BOOST_STATIC_ASSERT(res_safe_bool);

// gcc 4.6 chokes on the xxx assignment
# if !BOOST_WORKAROUND(BOOST_GCC, < 40700)
    constexpr tribool xxx = (tribool(true) || tribool(indeterminate));
    BOOST_STATIC_ASSERT(xxx);
# endif
#endif

  return boost::report_errors();
}
