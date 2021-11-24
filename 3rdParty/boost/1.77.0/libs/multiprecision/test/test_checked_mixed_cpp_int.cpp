///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include "test.hpp"

template <class T, class U>
void check_result_type(const T&, const U&)
{
   BOOST_CHECK(0);
}

void check_result_type(const boost::multiprecision::checked_int1024_t&, const boost::multiprecision::checked_int1024_t&) {}

int main()
{
#ifndef BOOST_NO_EXCEPTIONS
   try
   {
#endif
      typedef boost::multiprecision::checked_int1024_t                                                                                                                                                    big_type;
      typedef boost::multiprecision::checked_int512_t                                                                                                                                                     small_type;
      typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<32, 32, boost::multiprecision::signed_magnitude, boost::multiprecision::checked, void>, boost::multiprecision::et_off> little_type;

      big_type    big_val    = (big_type(1) << 1000) + 1;
      small_type  small_val  = 1;
      little_type little_val = 1;

      check_result_type(big_val, big_val + small_val);
      check_result_type(big_val, big_val - small_val);
      check_result_type(big_val, big_val * small_val);
      check_result_type(big_val, big_val / small_val);
      check_result_type(big_val, big_val | small_val);
      check_result_type(big_val, big_val & small_val);
      check_result_type(big_val, big_val ^ small_val);
      check_result_type(big_val, small_val + big_val);
      check_result_type(big_val, small_val - big_val);
      check_result_type(big_val, small_val * big_val);
      check_result_type(big_val, small_val / big_val);
      check_result_type(big_val, small_val | big_val);
      check_result_type(big_val, small_val & big_val);
      check_result_type(big_val, small_val ^ big_val);

      check_result_type(big_val, big_val + little_val);
      check_result_type(big_val, big_val - little_val);
      check_result_type(big_val, big_val * little_val);
      check_result_type(big_val, big_val / little_val);
      check_result_type(big_val, big_val | little_val);
      check_result_type(big_val, big_val & little_val);
      check_result_type(big_val, big_val ^ little_val);
      check_result_type(big_val, little_val + big_val);
      check_result_type(big_val, little_val - big_val);
      check_result_type(big_val, little_val * big_val);
      check_result_type(big_val, little_val / big_val);
      check_result_type(big_val, little_val | big_val);
      check_result_type(big_val, little_val & big_val);
      check_result_type(big_val, little_val ^ big_val);

      BOOST_CHECK_EQUAL(big_val == small_val, false);
      BOOST_CHECK_EQUAL(big_val <= small_val, false);
      BOOST_CHECK_EQUAL(big_val >= small_val, true);
      BOOST_CHECK_EQUAL(big_val < small_val, false);
      BOOST_CHECK_EQUAL(big_val > small_val, true);
      BOOST_CHECK_EQUAL(big_val != small_val, true);
      BOOST_CHECK_EQUAL(small_val == big_val, false);
      BOOST_CHECK_EQUAL(small_val <= big_val, true);
      BOOST_CHECK_EQUAL(small_val >= big_val, false);
      BOOST_CHECK_EQUAL(small_val < big_val, true);
      BOOST_CHECK_EQUAL(small_val > big_val, false);
      BOOST_CHECK_EQUAL(small_val != big_val, true);

      BOOST_CHECK_EQUAL(big_val == little_val, false);
      BOOST_CHECK_EQUAL(big_val <= little_val, false);
      BOOST_CHECK_EQUAL(big_val >= little_val, true);
      BOOST_CHECK_EQUAL(big_val < little_val, false);
      BOOST_CHECK_EQUAL(big_val > little_val, true);
      BOOST_CHECK_EQUAL(big_val != little_val, true);
      BOOST_CHECK_EQUAL(little_val == big_val, false);
      BOOST_CHECK_EQUAL(little_val <= big_val, true);
      BOOST_CHECK_EQUAL(little_val >= big_val, false);
      BOOST_CHECK_EQUAL(little_val < big_val, true);
      BOOST_CHECK_EQUAL(little_val > big_val, false);
      BOOST_CHECK_EQUAL(little_val != big_val, true);
#ifndef BOOST_NO_EXCEPTIONS
   }
   catch (const std::exception& e)
   {
      std::cout << "Failed with unexpected exception: " << e.what() << std::endl;
      return 1;
   }
#endif
   return boost::report_errors();
}
