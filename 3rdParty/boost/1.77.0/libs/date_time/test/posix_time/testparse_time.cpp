/* Copyright (c) 2002,2003,2005 CrystalClear Software, Inc.
 * Use, modification and distribution is subject to the 
 * Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 * Author: Jeff Garland, Bart Garst
 */

#include "boost/date_time/posix_time/posix_time.hpp"
#include "../testfrmwk.hpp"


int 
main()
{
  using namespace boost::gregorian;
  using namespace boost::posix_time;

#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
  {
  std::string s1("12:11:10.123456789");
  time_duration td1= duration_from_string(s1);
  check("parse time duration: " + s1, 
        td1 == time_duration(12,11,10,123456789));
  std::string s2("12:11:10,123456789");
  time_duration td2= boost::date_time::parse_delimited_time_duration<time_duration>(s2);
  check("parse time duration: " + s2, 
        td2 == time_duration(12,11,10,123456789));
  std::string s3("12:11:10");
  time_duration td3= boost::date_time::parse_delimited_time_duration<time_duration>(s3);
  check("parse time duration: " + s3, 
        td3 == time_duration(12,11,10,0));
  std::string s4("23:59:59.000000001");
  time_duration td4= boost::date_time::parse_delimited_time_duration<time_duration>(s4);
  check("parse time duration: " + s4, 
        td4 == time_duration(23,59,59)+nanosec(1));
  std::string s5("23:59:59.999999999");
  time_duration td5= boost::date_time::parse_delimited_time_duration<time_duration>(s5);
  check("parse time duration: " + s5, 
        td5 == time_duration(23,59,59)+nanosec(999999999));
  std::string s5b("-23:59:59.999999999");
  time_duration td5b= boost::date_time::parse_delimited_time_duration<time_duration>(s5b);
  check("parse time duration: " + s5b, 
        td5b == time_duration(-23,59,59)-nanosec(999999999));
  
  std::string s6b("1:00:00.1"); // we want 1/10th
  time_duration td6b= boost::date_time::parse_delimited_time_duration<time_duration>(s6b);
  check("parse time duration: " + s6b, 
        td6b == time_duration(1,0,0)+nanosec(100000000)); // we want 1/10th
  
  std::string s7b("-1:00:00.0010"); // we want 1/1000th
  time_duration td7b= boost::date_time::parse_delimited_time_duration<time_duration>(s7b);
  check("parse time duration: " + s7b, 
        td7b == time_duration(-1,0,0)-nanosec(1000000)); // we want 1/1000th
  
  std::string s8b("1:22:33.123456789321"); // too many digits
  time_duration td8b= boost::date_time::parse_delimited_time_duration<time_duration>(s8b);
  check("parse time duration: " + s8b, 
        td8b == time_duration(1,22,33,123456789)); // excess digits should be dropped
  }
  {

    std::string s3("12:11:10");
    time_duration td3= boost::date_time::parse_delimited_time_duration<time_duration>(s3);
    check("parse time duration: " + s3, 
          td3 == time_duration(12,11,10,0));
    std::string s4("23:59:59.000001");
    time_duration td4= boost::date_time::parse_delimited_time_duration<time_duration>(s4);
    check("parse time duration: " + s4, 
          td4 == time_duration(23,59,59)+microsec(1));
    std::string s5("23:59:59.999999");
    time_duration td5= boost::date_time::parse_delimited_time_duration<time_duration>(s5);
    check("parse time duration: " + s5, 
          td5 == time_duration(23,59,59)+microsec(999999));
    std::string s5b("-23:59:59.999999");
    time_duration td5b= boost::date_time::parse_delimited_time_duration<time_duration>(s5b);
    check("parse time duration: " + s5b, 
          td5b == time_duration(-23,59,59)-microsec(999999));
  
    std::string s6b("1:00:00.1"); // we want 1/10th
    time_duration td6b= boost::date_time::parse_delimited_time_duration<time_duration>(s6b);
    check("parse time duration: " + s6b,
          td6b == time_duration(1,0,0)+microsec(100000)); // we want 1/10th
    
    std::string s7b("-1:00:00.0010"); // we want 1/1000th
    time_duration td7b= boost::date_time::parse_delimited_time_duration<time_duration>(s7b);
    check("parse time duration: " + s7b, 
          td7b == time_duration(-1,0,0)-microsec(1000)); // we want 1/1000th

  }

  {
    std::string ts2("2002-12-31 00:00:00.999999999");
    ptime t2 = time_from_string(ts2);
    check("parse time: " + ts2, 
          t2 == ptime(date(2002,12,31),time_duration(0,0,0)+nanosec(999999999)));
  }
  {
    std::string ts2("2002-12-31 00:00:00.");
    ptime t2 = time_from_string(ts2);
    check("parse time (decimal but no digits): " + ts2, 
          t2 == ptime(date(2002,12,31),time_duration(0,0,0)));
  }
#endif

  if (time_duration::resolution() != boost::date_time::nano)
  {

    std::string s1("12:11:10.123456");
    time_duration td1= duration_from_string(s1);
    check("parse time duration: " + s1,
          td1 == time_duration(12,11,10,123456));
    std::cout << "td1: " << td1 << std::endl;

    std::string s2("12:11:10,123456");
    time_duration td2= boost::date_time::parse_delimited_time_duration<time_duration>(s2);
    check("parse time duration: " + s2,
          td2 == time_duration(12,11,10,123456));
    std::cout << "td2: " << td2 << std::endl;

    //truncate for non-nanosecond case
    std::string s8b("1:22:33.123456321"); // too many digits
    time_duration td8b= boost::date_time::parse_delimited_time_duration<time_duration>(s8b);
    check("parse time duration: " + s8b,
          td8b == time_duration(1,22,33,123456)); // excess digits should be dropped
  }


  std::string date_1, tod_1;
  std::string ts1("2002-01-20 23:59:59.000");
  boost::date_time::split(ts1, ' ', date_1, tod_1);
  check("split function date part of " + ts1, 
        date_1 == std::string("2002-01-20"));
  check("time part of " + ts1, 
        tod_1 == std::string("23:59:59.000"));
//   std::cout << date_1 << "|" << std::endl;
//   std::cout << tod_1  << "|" << std::endl;


  {
    ptime t1 = time_from_string(ts1);
    check("parse time: " + ts1, 
          t1 == ptime(date(2002,1,20),time_duration(23,59,59)));
  }
  {
    std::string ts1x("2002-01-20 23:59:59.");
    ptime t1 = time_from_string(ts1x);
    check("parse time (decimal but no digits): " + ts1x, 
          t1 == ptime(date(2002,1,20),time_duration(23,59,59)));
  }

  std::string s6("235859");
  time_duration td6= boost::date_time::parse_undelimited_time_duration<time_duration>(s6);
  check("parse time duration: " + s6, 
        td6 == time_duration(23,58,59));

  s6 = "-235859";
  td6= boost::date_time::parse_undelimited_time_duration<time_duration>(s6);
  check("parse negative time duration: " + s6, 
        td6 == time_duration(-23,58,59));

  {
    std::string ts3("20020120T235859");
    ptime t20 = from_iso_string(ts3);
    check("parse iso time: " + ts3, 
          t20 == ptime(date(2002,1,20),time_duration(23,58,59)));
  }  

  {
    std::string ts4("19001231T000000");
    ptime t21 = from_iso_string(ts4);
    check("parse iso time: " + ts4, 
          t21 == ptime(date(1900,12,31),time_duration(0,0,0)));
  }

  {
    std::string ts5("19001231T23");
    ptime t22 = from_iso_string(ts5);
    check("parse iso time: " + ts5, 
          t22 == ptime(date(1900,12,31),time_duration(23,0,0)));
  }

  {
#if defined(BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG)
    std::string ts3("20020120T235859.123456789");
    ptime t20 = from_iso_string(ts3);
    check("parse iso time w/ frac sec: " + ts3, 
          t20 == ptime(date(2002,1,20),time_duration(23,58,59,123456789)));
    
    std::string ts4("19001231T000000.123");
    ptime t21 = from_iso_string(ts4);
    check("parse iso time w/ frac sec (too short): " + ts4, 
          t21 == ptime(date(1900,12,31),time_duration(0,0,0,123000000)));

    std::string ts5("19001231T230000.123456789876");
    ptime t22 = from_iso_string(ts5);
    check("parse iso time w/ frac sec (too long): " + ts5, 
          t22 == ptime(date(1900,12,31),time_duration(23,0,0,123456789)));

    std::string ts6("19001231T230000.");
    ptime t23 = from_iso_string(ts6);
    check("parse iso time w/ frac sec (dec only): " + ts6, 
          t23 == ptime(date(1900,12,31),time_duration(23,0,0,0)));

    std::string ts7("2002-01-20T23:58:59.123456789");
    ptime t24 = from_iso_extended_string(ts7);
    check("parse iso extended time w/ frac sec: " + ts7,
          t24 == ptime(date(2002,1,20),time_duration(23,58,59,123456789)));

    std::string ts8("1900-12-31T00:00:00.123");
    ptime t25 = from_iso_extended_string(ts8);
    check("parse iso extended time w/ frac sec (too short): " + ts8,
          t25 == ptime(date(1900,12,31),time_duration(0,0,0,123000000)));

    std::string ts9("1900-12-31T23:00:00.123456789876");
    ptime t26 = from_iso_extended_string(ts9);
    check("parse iso extended time w/ frac sec (too long): " + ts9,
          t26 == ptime(date(1900,12,31),time_duration(23,0,0,123456789)));

    std::string ts10("1900-12-31T23:00:00.");
    ptime t27 = from_iso_extended_string(ts10);
    check("parse iso extended time w/ frac sec (dec only): " + ts10,
          t27 == ptime(date(1900,12,31),time_duration(23,0,0,0)));


#else
    std::string ts3("20020120T235859.123456");
    ptime t20 = from_iso_string(ts3);
    check("parse iso time w/ frac sec: " + ts3, 
          t20 == ptime(date(2002,1,20),time_duration(23,58,59,123456)));
    
    std::string ts4("19001231T000000.123");
    ptime t21 = from_iso_string(ts4);
    check("parse iso time w/ frac sec (too short): " + ts4, 
          t21 == ptime(date(1900,12,31),time_duration(0,0,0,123000)));

    std::string ts5("19001231T230000.123456789876");
    ptime t22 = from_iso_string(ts5);
    check("parse iso time w/ frac sec (too long): " + ts5, 
          t22 == ptime(date(1900,12,31),time_duration(23,0,0,123456)));

    std::string ts6("19001231T230000.");
    ptime t23 = from_iso_string(ts6);
    check("parse iso time w/ frac sec (dec only): " + ts6, 
          t23 == ptime(date(1900,12,31),time_duration(23,0,0,0)));

    std::string ts7("2002-01-20T23:58:59.123456");
    ptime t24 = from_iso_extended_string(ts7);
    check("parse iso extended time w/ frac sec: " + ts7,
          t24 == ptime(date(2002,1,20),time_duration(23,58,59,123456)));

    std::string ts8("1900-12-31T00:00:00.123");
    ptime t25 = from_iso_extended_string(ts8);
    check("parse iso extended time w/ frac sec (too short): " + ts8,
          t25 == ptime(date(1900,12,31),time_duration(0,0,0,123000)));

    std::string ts9("1900-12-31T23:00:00.123456789876");
    ptime t26 = from_iso_extended_string(ts9);
    check("parse iso extended time w/ frac sec (too long): " + ts9,
          t26 == ptime(date(1900,12,31),time_duration(23,0,0,123456)));

    std::string ts10("1900-12-31T23:00:00.");
    ptime t27 = from_iso_extended_string(ts10);
    check("parse iso extended time w/ frac sec (dec only): " + ts10,
          t27 == ptime(date(1900,12,31),time_duration(23,0,0,0)));

    //the following test cases are for this issue:
    //https://github.com/boostorg/date_time/issues/187
    try {
      std::string ts11("1900-12-31"); //date only string
      ptime t28 = from_iso_extended_string(ts11);
      check("parse iso extended time date only): " + ts11,
            t28 == ptime(date(1900,12,31),time_duration(0,0,0,0)));
    }
    catch( std::exception& e )
    {
      std::cout << e.what() << std::endl;
      check("parse iso extended time date only): 1900-12-32", false);
    }

    try {
      //expect a date-time exception here
      std::string ts12("1900-1"); //completely bogus string
      ptime t29 = from_iso_extended_string(ts12);
      check("parse iso extended time bad string: " + ts12, false);
      (void) t29; //supress unused compiler warning, this is never reached
    }
    catch( std::exception& e )
    {
      //Day of month value is out of range 1..31
      // std::cout << e.what() << std::endl;
      check("parse iso extended time date only): 1900-1", true);
    }

#endif // BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
  }

  std::string s7("-01:25:00"), s8("-00:40:00"), s9("0:45"), s10("0:-40");
  time_duration tds1 = duration_from_string(s7);
  time_duration tds2 = duration_from_string(s8);
  time_duration tds3 = duration_from_string(s9);
  time_duration tds4 = duration_from_string(s10);
  check("from string construct", tds1 == time_duration(-1,25,0));
  check("from string construct", tds2 == minutes(-40));
  check("from string construct", tds3 == minutes(45));
  // '-' in middle of string s10 should be ignored resulting in pos duration
  check("from string construct", tds4 == minutes(40));

  return printTestStats();
 
}

