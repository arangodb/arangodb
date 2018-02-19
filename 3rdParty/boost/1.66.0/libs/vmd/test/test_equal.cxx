
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
  
  #define ANIDENTIFIER ggh
  #define ANUMBER 249
  #define ANUMBER2 17
  #define ASEQ (25)(26)(27)
  #define ASEQ2 (1)(2)(3)
  #define ATUPLE (0,(ggh,45,(2,(89,(dvd)(57)(99)))),170)
  #define ALIST (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ALIST2 (0,(1,(2,(4,BOOST_PP_NIL))))
  #define ANARRAY (3,(ggh,dvd,38))
  #define ANARRAY2 (3,(dvd,ggh,38))
  #define ASEQUENCE4
  
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ASEQ,249));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ALIST,17));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ANIDENTIFIER,ANARRAY));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ANUMBER2,dvd));
  BOOST_TEST(BOOST_VMD_EQUAL(ASEQUENCE4,));
  BOOST_TEST(BOOST_VMD_EQUAL(,));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ANARRAY2,ASEQ2));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ASEQ,22));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ATUPLE,ANARRAY));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ALIST2,ANUMBER2));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ANARRAY,ASEQUENCE4));

#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
