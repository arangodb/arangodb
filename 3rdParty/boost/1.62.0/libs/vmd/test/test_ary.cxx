
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_unary.hpp>
#include <boost/vmd/is_multi.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define BOOST_VMD_REGISTER_ggh (ggh)
  #define BOOST_VMD_DETECT_ggh_ggh
  #define BOOST_VMD_REGISTER_dvd (dvd)
  #define BOOST_VMD_DETECT_dvd_dvd
  
  #define ANIDENTIFIER ggh
  #define ANIDENTIFIER2 dvd
  #define ANUMBER 249
  #define ANUMBER2 17
  #define ASEQ (25)(26)(27)
  #define ASEQ2 (1)(2)(3)
  #define ATUPLE (0,1,2,3,((a,b))((c,d))((e))((f,g,h)))
  #define ALIST (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ANARRAY (3,(a,b,38))
  #define ATYPE BOOST_VMD_TYPE_TUPLE
  #define ASEQUENCE ANUMBER ALIST ATUPLE ANIDENTIFIER ANARRAY ASEQ
  #define ASEQUENCE2 ANIDENTIFIER2 ASEQ ALIST ANUMBER ATUPLE
  #define ASEQUENCE3 ASEQ ANUMBER2 ATUPLE
  #define ASEQUENCE4
  #define ASEQUENCE5 ATYPE ASEQ ALIST
  
  BOOST_TEST(BOOST_VMD_IS_UNARY((3)));
  BOOST_TEST(BOOST_VMD_IS_UNARY((3)(4)));
  BOOST_TEST(BOOST_VMD_IS_UNARY((4,(a,b,c,d))));
  BOOST_TEST(BOOST_VMD_IS_UNARY((4,(5,BOOST_PP_NIL))));
  BOOST_TEST(BOOST_VMD_IS_UNARY(ANIDENTIFIER));
  BOOST_TEST(BOOST_VMD_IS_UNARY(ANUMBER));
  BOOST_TEST(!BOOST_VMD_IS_UNARY(ASEQUENCE));
  BOOST_TEST(!BOOST_VMD_IS_UNARY(ASEQUENCE4));
  BOOST_TEST(!BOOST_VMD_IS_UNARY(ASEQUENCE5));
  
  BOOST_TEST(BOOST_VMD_IS_MULTI(ASEQUENCE));
  BOOST_TEST(BOOST_VMD_IS_MULTI(ASEQUENCE2));
  BOOST_TEST(BOOST_VMD_IS_MULTI(ASEQUENCE3));
  BOOST_TEST(!BOOST_VMD_IS_MULTI(ASEQUENCE4));
  BOOST_TEST(BOOST_VMD_IS_MULTI(ASEQUENCE5));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
