
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/equal.hpp>
#include <boost/vmd/not_equal.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define BOOST_VMD_REGISTER_ggh (ggh)
  #define BOOST_VMD_DETECT_ggh_ggh
  #define BOOST_VMD_REGISTER_dvd (dvd)
  #define BOOST_VMD_DETECT_dvd_dvd
  
  #define ATUPLE (0,(ggh,45,(2,(89,(dvd)(57)(99)))),170)
  #define ATUPLE2 (0,(ggh,45,(2,(89,(dvd)(57)(99)))),170)
  #define ATUPLE3 (0,(ggh,45,(2,(89,(ggh)(57)(99)))),170)
  #define ATUPLE4 (0,1,(2,3,4,(5,6,7,8)))
  #define ATUPLE5 (0,1,(2,3,4,(5,6,7,8)))
  #define ATUPLE6 (0,1,(2,3,4,(5,6,7,9)))
  
  BOOST_TEST(BOOST_VMD_EQUAL(ATUPLE,ATUPLE2));
  BOOST_TEST(BOOST_VMD_EQUAL(ATUPLE4,ATUPLE5));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ATUPLE2,ATUPLE3));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ATUPLE4,ATUPLE6));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
