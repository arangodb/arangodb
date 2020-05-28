/* Copyright (c) 2002,2003 CrystalClear Software, Inc.
 * Use, modification and distribution is subject to the 
 * Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 * Author: Jeff Garland 
 */

#include "boost/date_time/gregorian/greg_year.hpp"
#include "../testfrmwk.hpp"
#include <iostream>
#include <sstream>

void test_yearlimit(int yr, bool allowed)
{
    std::stringstream sdesc;
    sdesc << "should" << (allowed ? "" : " not") << " be able to make a year " << yr;

    try {
        boost::gregorian::greg_year chkyr(yr);
        check(sdesc.str(), allowed);
        if (allowed) {
            check_equal("year operator ==", chkyr, yr);
        }
    }
    catch (std::out_of_range&) { check(sdesc.str(), !allowed); }
}

int
main() 
{
  // trac-13159 better limit testing
  test_yearlimit(    0, false);
  test_yearlimit( 1399, false);
  test_yearlimit( 1400,  true);
  test_yearlimit( 1401,  true);
  test_yearlimit( 9999,  true);
  test_yearlimit(10000, false);
  test_yearlimit(10001, false);

  check("traits min year", (boost::gregorian::greg_year::min)() == 1400);
  check("traits max year", (boost::gregorian::greg_year::max)() == 9999);

  return printTestStats();
}

