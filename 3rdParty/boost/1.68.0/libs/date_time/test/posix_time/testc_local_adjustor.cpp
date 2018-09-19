/* Copyright (c) 2002,2003 CrystalClear Software, Inc.
 * Use, modification and distribution is subject to the 
 * Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 * Author: Jeff Garland 
 */

#include <stdexcept>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/c_local_time_adjustor.hpp"
#include "../testfrmwk.hpp"

int
main() 
{
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  //These are a compile check / test.  They have to be hand inspected
  //b/c they depend on the TZ settings of the machine and hence it is
  //unclear what the results will be
  typedef boost::date_time::c_local_adjustor<ptime> local_adj;
  
  bool btd1e = false;
  bool btd2e = false;
  bool btd3e = false;
  time_duration td1;
  time_duration td2;
  time_duration td3;
  
  try
      {
      ptime t1(date(2002,Jan,1), hours(7)+millisec(5)); 
      std::cout << "UTC <--> TZ Setting of Machine -- No DST" << std::endl;
      ptime t2 = local_adj::utc_to_local(t1);
      std::cout << to_simple_string(t2) << " LOCAL is " 
                << to_simple_string(t1) << " UTC time "
                << std::endl;
      td1 = t2 - t1;
      std::cout << "A difference of: " << to_simple_string(td1)
                << std::endl;
      }
  catch (std::runtime_error & re)
      {
      btd1e = true;
      check(re.what(), false);
      }
  
  try
      {
      ptime t3(date(2002,May,1), hours(5)+millisec(5)); 
      std::cout << "UTC <--> TZ Setting of Machine -- In DST" << std::endl;
      ptime t4 = local_adj::utc_to_local(t3);
      std::cout << to_simple_string(t4) << " LOCAL is " 
                << to_simple_string(t3) << " UTC time "
                << std::endl;
      td2 = t4 - t3;
      std::cout << "A difference of: " << to_simple_string(td2)
                << std::endl;
      }
  catch (std::runtime_error & re)
      {
      btd2e = true;
      check(re.what(), false);
      }

  try
      {
      ptime t5(date(2040,May,1), hours(5)+millisec(5));
      std::cout << "UTC <--> TZ Setting of Machine -- In DST" << std::endl;
      ptime t6 = local_adj::utc_to_local(t5);
      std::cout << to_simple_string(t6) << " LOCAL is " 
                << to_simple_string(t5) << " UTC time "
                << std::endl;
      td3 = t6 - t5;
      std::cout << "a difference of: " << to_simple_string(td3)
                << std::endl;
      }
  catch (std::runtime_error & re)
      {
      btd3e = true;
      check(re.what(), false);
      }
  catch (std::bad_cast&)
      {
      btd3e = true;
      check("32-bit time_t overflow detected", sizeof(std::time_t) < 8);
      }

  // The following tests are unaware of the local time zone, but they
  // should help spot some errors. Manual inspection could still be
  // required.

  // Based on http://stackoverflow.com/questions/8131023
  // All time zones are between -12 and +14
  if (!btd1e)
      {
      check("td1 isn't too low", td1 >= hours(-12));
      check("td1 isn't too high", td1 <= hours(14));
      }
  if (!btd2e)
      {
      check("td2 isn't too low", td2 >= hours(-12));
      check("td2 isn't too high", td2 <= hours(14));
      }
  if (!btd3e)
      {
      check("td3 isn't too low", td3 >= hours(-12));
      check("td3 isn't too high", td3 <= hours(14));
      }

  // Assuming that no one uses DST of more than an hour.
  if (!btd1e && !btd2e)
      {
      check("td1 and td2 are close",
              td1 - td2 <= hours(1) && td2 - td1 <= hours(1));
      }
  if (!btd2e && !btd3e)
      {
      check("td2 and td3 are close",
              td2 - td3 <= hours(2) && td3 - td2 <= hours(2));
      }
  if (!btd1e && !btd3e)
      {
      check("td1 and td3 are close",
              td1 - td3 <= hours(1) && td3 - td1 <= hours(1));
      }

  return printTestStats();
}

