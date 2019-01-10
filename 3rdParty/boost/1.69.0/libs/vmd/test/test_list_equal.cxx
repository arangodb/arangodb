
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
  
  #define ALIST (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ALIST2 (0,(1,(2,(4,BOOST_PP_NIL))))
  #define ALIST3 (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ALIST4 (0,((aaa)(ccc),(2,(3,BOOST_PP_NIL))))
  #define ALIST5 (0,((aaa)(ccc),(2,(3,BOOST_PP_NIL))))
  #define ALIST6 (0,((bbb)(ccc),(2,(3,BOOST_PP_NIL))))
  #define ATUPLE (1)
  #define ATUPLE2 (1)
  
  BOOST_TEST(BOOST_VMD_EQUAL(ALIST,ALIST3,BOOST_VMD_TYPE_LIST));
  BOOST_TEST(BOOST_VMD_EQUAL(ALIST4,ALIST5,BOOST_VMD_TYPE_LIST));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ALIST,ALIST2,BOOST_VMD_TYPE_LIST));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ALIST4,ALIST6,BOOST_VMD_TYPE_LIST));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ATUPLE,ATUPLE2,BOOST_VMD_TYPE_LIST));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
