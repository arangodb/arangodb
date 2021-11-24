///////////////////////////////////////////////////////////////////////////////
//  Copyright 2016 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>

boost::multiprecision::cpp_rational rationalfromStr(const char* str)
{
   boost::multiprecision::cpp_dec_float_50 d1(str);
   boost::multiprecision::cpp_rational     result(d1); // <--- eats CPU forever
   return result;
}

boost::multiprecision::cpp_rational rationalfromStr2(const char* str)
{
   boost::multiprecision::cpp_bin_float_50 d1(str);
   boost::multiprecision::cpp_rational     result(d1); // <--- eats CPU forever
   return result;
}

int main()
{
   // This example is OK.
   {
      boost::multiprecision::cpp_rational expected = 1;
      BOOST_ASSERT(expected == rationalfromStr("1"));
   }
   // This example is OK.
   {
      boost::multiprecision::cpp_rational expected = boost::multiprecision::cpp_rational(25) / boost::multiprecision::cpp_rational(10);
      BOOST_ASSERT(expected == rationalfromStr("2.5"));
   }
   // This example is OK.
   {
      boost::multiprecision::cpp_rational expected = boost::multiprecision::cpp_rational(5) / boost::multiprecision::cpp_rational(1000);
      BOOST_ASSERT(expected == rationalfromStr("0.005"));
   }
   // This example is OK.
   {
      boost::multiprecision::cpp_rational expected = 0;
      BOOST_ASSERT(expected == boost::multiprecision::cpp_rational("0")); // direct cpp_rational from str is OK.
   }
   // This example fails.
   {
      boost::multiprecision::cpp_rational expected = 0;
      // reachable code
      BOOST_ASSERT(expected == rationalfromStr("0")); // cpp_rational from cpp_dec_float_50 is not OK.
            // unreachable code
   }
   {
      boost::multiprecision::cpp_rational expected = 0;
      // reacheble code
      BOOST_ASSERT(expected == rationalfromStr2("0")); // cpp_rational from cpp_dec_float_50 is not OK.
          // unreachable code
   }
   return 0;
}
