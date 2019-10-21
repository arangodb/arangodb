///////////////////////////////////////////////////////////////////////////////
//  Copyright 2016 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/multiprecision/cpp_int.hpp>

struct E
{
   E(boost::multiprecision::cpp_rational const&)
   {

   }
};

void g(boost::multiprecision::cpp_rational const& r)
{
   std::cout << r << std::endl;
}

int main()
{
#if !defined(BOOST_MP_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS) && !BOOST_WORKAROUND(BOOST_MSVC, < 1900)\
      && !(defined(__APPLE_CC__) && defined(CI_SUPPRESS_KNOWN_ISSUES))
   boost::multiprecision::cpp_int r = 3;
   g(r);
   E x1(r);  // triggers explicit conversion operator.
#endif
   return 0;
}

