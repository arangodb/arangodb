
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
  
  #define BOOST_VMD_REGISTER_aaa (aaa)
  #define BOOST_VMD_REGISTER_bbb (bbb)
  #define BOOST_VMD_REGISTER_ccc (ccc)
  #define BOOST_VMD_REGISTER_ddd (ddd)
  #define BOOST_VMD_REGISTER_ggh (ggh)
  #define BOOST_VMD_REGISTER_dvd (dvd)
  #define BOOST_VMD_DETECT_aaa_aaa
  #define BOOST_VMD_DETECT_bbb_bbb
  #define BOOST_VMD_DETECT_ccc_ccc
  #define BOOST_VMD_DETECT_ddd_ddd
  #define BOOST_VMD_DETECT_ggh_ggh
  #define BOOST_VMD_DETECT_dvd_dvd
  
  BOOST_TEST(BOOST_VMD_EQUAL(aaa,aaa,BOOST_VMD_TYPE_IDENTIFIER));
  BOOST_TEST(BOOST_VMD_EQUAL(ddd,ddd,BOOST_VMD_TYPE_IDENTIFIER));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ggg,dvd,BOOST_VMD_TYPE_IDENTIFIER));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(bbb,ccc,BOOST_VMD_TYPE_IDENTIFIER));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(eee,eee,BOOST_VMD_TYPE_IDENTIFIER));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL((1,2),(1,2),BOOST_VMD_TYPE_IDENTIFIER));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
