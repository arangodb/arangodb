///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_bin_float.hpp>
#include "test_mixed.hpp"

int main()
{
#ifndef BOOST_NO_EXCEPTIONS
   try{
#endif
      typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<100>, boost::multiprecision::et_on> big_type1;
      typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<50>, boost::multiprecision::et_on> small_type1;
      typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<100>, boost::multiprecision::et_off> big_type2;
      typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<50>, boost::multiprecision::et_off> small_type2;

      test<big_type1, small_type1>();
      test<big_type2, small_type2>();
      test<big_type1, small_type2>();
      test<big_type2, small_type1>();
#ifndef BOOST_NO_EXCEPTIONS
   }
   catch(const std::exception& e)
   {
      std::cout << "Failed with unexpected exception: " << e.what() << std::endl;
      return 1;
   }
#endif
   return boost::report_errors();
}

