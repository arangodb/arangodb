/* Copyright (c) 2020 CrystalClear Software, Inc.
 * Use, modification and distribution is subject to the 
 * Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 * Author: Jeff Garland
 */

#include "boost/date_time/wrapping_int.hpp"

//#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time_duration.hpp"

#include "testfrmwk.hpp"
#include <iostream>

using std::cout;
using std::endl;

int
main() 
{
  using namespace boost::date_time;
  using namespace boost::posix_time;

#ifdef BOOST_NO_CXX14_CONSTEXPR
  check("constexpr not configured", true);
#else
  constexpr wrapping_int<int, 3600> wi(3599);
  static_assert(wi == 3599, "constexpr construction/conversion");
  check("constexpr wrapping construct and equal", true);

  constexpr microseconds ms(1000);
  static_assert(ms.is_special()  == false, "constexpr duration is_special");
  static_assert(ms.is_positive() == true,  "constexpr duration is_positive");
  static_assert(ms.is_negative() == false, "constexpr duration is_negative");
  static_assert(ms.total_microseconds() == 1000, "constexpr total_microseconds");
  check("constexpr microseconds - total_microseconds", true);
  
#endif 
  
#ifdef BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
  cout << "Standard Config" << endl;
#else 
  cout << "NOT Standard Config" << endl;
#endif

#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
  cout << "Has NANO: " << endl;
#else
  cout << "NO NANO: " << endl;
#endif
  
  check("success", true);

  return printTestStats();

}


