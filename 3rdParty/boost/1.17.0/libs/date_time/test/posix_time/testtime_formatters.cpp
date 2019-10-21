/* Copyright (c) 2002,2003 CrystalClear Software, Inc.
 * Use, modification and distribution is subject to the 
 * Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 * Author: Jeff Garland, Bart Garst
 */

#include "boost/date_time/posix_time/posix_time.hpp"
#include "../testfrmwk.hpp"

#define CHECK_ROUNDTRIP(_PT) check_equal("from_iso_string(to_iso_string()) roundtrip of \"" + to_iso_string(_PT) + "\"", from_iso_string(to_iso_string(_PT)), _PT)

int
main() 
{

  using namespace boost::posix_time;
  using namespace boost::gregorian;
  date d1(2002,Jan,1);
  std::string d1_string("2002-Jan-01");
  std::string t1_string("01:02:03");
  std::string t1_result = d1_string + " " + t1_string;
  ptime t1(d1,time_duration(1,2,3)); //2002-Jan-1 01:02:03
  std::cout << to_simple_string(t1) << std::endl;
  check("simple:      " + t1_result, t1_result == to_simple_string(t1));
  std::string iso_result = "20020101T010203";
  check("iso:         " + iso_result, iso_result == to_iso_string(t1));
  std::string iso_ext_result = "2002-01-01T01:02:03";
  check("iso ext:     " + iso_ext_result, iso_ext_result == to_iso_extended_string(t1));

  CHECK_ROUNDTRIP(t1);

#ifdef BOOST_DATE_TIME_HAS_MILLISECONDS

  if (time_duration::resolution() == boost::date_time::milli) {
    ptime t4(d1,hours(1)+minutes(2)+seconds(3)+millisec(4));
    std::string r3 = to_simple_string(t4);
    check("simple subsecond: "+r3 , 
          std::string("2002-Jan-01 01:02:03.004000") == r3);
    CHECK_ROUNDTRIP(t4);
  }

#endif

#ifdef BOOST_DATE_TIME_HAS_MICROSECONDS

  if (time_duration::resolution() == boost::date_time::micro) {
    ptime t3(d1,hours(1)+minutes(2)+seconds(3)+microsec(4));
    std::string result = to_simple_string(t3);
    check("microsecond: "+result , 
          std::string("2002-Jan-01 01:02:03.000004") == to_simple_string(t3));
    
    time_duration td2 =  hours(-12)+minutes(4)+seconds(2)+microsec(1);
    //  time_duration td2 =  hours(-12)+minutes(4)+seconds(2)+millisec(1);
    std::string r2 = to_simple_string(td2);
    check("microseond neg subsecond duration: "+r2 , 
          std::string("-11:55:57.999999") == r2);
  
    std::string s1("-01:25:00"), s2("-00:40:00"), is1("-012500"), is2("-004000");
    time_duration td1(-1,25,0);
    time_duration tds2(0,-40,0);
    check("to string: " + to_simple_string(td1), to_simple_string(td1) == s1);
    check("to string: " + to_simple_string(tds2), to_simple_string(tds2) == s2);
    check("to string: " + to_iso_string(td1), to_iso_string(td1) == is1);
    check("to string: " + to_iso_string(tds2), to_iso_string(tds2) == is2);

    CHECK_ROUNDTRIP(t3);
  }
#endif

#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS

  if (time_duration::resolution() == boost::date_time::nano) {
    ptime t2(d1,hours(12) + minutes(5) + seconds(1));
    time_period p1(t1,t2); //last value in period is 12:05:00 1/10000 sec less than t2
    std::string period_result("["+t1_result + "/" + d1_string + " " + "12:05:00.999999999]" );
    check("simple:      " + period_result + "==" + to_simple_string(p1), period_result == to_simple_string(p1));
    
    ptime t3(d1,hours(1)+minutes(2)+seconds(3)+nanosec(4));
    std::string result = to_simple_string(t3);
    check("simple subsecond: "+result , 
          std::string("2002-Jan-01 01:02:03.000000004") == to_simple_string(t3));
    
  
    std::string s1("-01:25:00"), s2("-00:40:00"), is1("-012500"), is2("-004000");
    time_duration td1(-1,25,0);
    time_duration tds2(0,-40,0);
    check("to string: " + to_simple_string(td1), to_simple_string(td1) == s1);
    check("to string: " + to_simple_string(tds2), to_simple_string(tds2) == s2);
    check("to string: " + to_iso_string(td1), to_iso_string(td1) == is1);
    check("to string: " + to_iso_string(tds2), to_iso_string(tds2) == is2);
    
    time_duration td2 =  hours(-12)+minutes(4)+seconds(2)+nanosec(100);
    std::string r2 = to_simple_string(td2);
    check("neg subsecond duration: "+r2 , 
          std::string("-11:55:57.999999900") == r2);
    
    ptime t4(d1,hours(1)+minutes(2)+seconds(3)+millisec(4));
    std::string r3 = to_simple_string(t4);
    check("simple subsecond: "+r3 , 
          std::string("2002-Jan-01 01:02:03.004000000") == r3);

    CHECK_ROUNDTRIP(t3);
}
#endif

  // Boost Trac 1078 (https://svn.boost.org/trac10/ticket/1078)
  // from_iso_string should be able to parse output of to_iso_string
  CHECK_ROUNDTRIP(ptime());
  CHECK_ROUNDTRIP(ptime(not_a_date_time)); // should be same as previous
  CHECK_ROUNDTRIP(ptime(pos_infin));
  CHECK_ROUNDTRIP(ptime(neg_infin));
  
  // when min_date_time is formatted out, it is done so as an actual date/time
  // i.e. it is not "special" in that sense
  check_equal("from_iso_string(\"minimum-date-time\")", from_iso_string("minimum-date-time"), ptime(min_date_time));
  check_equal("from_iso_string(\"maximum-date-time\")", from_iso_string("maximum-date-time"), ptime(max_date_time));

  return printTestStats();
}


