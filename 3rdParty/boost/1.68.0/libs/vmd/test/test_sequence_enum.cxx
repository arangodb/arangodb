
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_empty.hpp>
#include <boost/vmd/is_identifier.hpp>
#include <boost/vmd/enum.hpp>
#include <boost/vmd/equal.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/array/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/elem.hpp>
#include <boost/preprocessor/variadic/size.hpp>

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
  #define ATUPLE (0,1,2,3,((a,b))((c,d))((e))((f,g,h)))
  #define ALIST (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ANARRAY (3,(a,b,38))
  #define ASEQUENCE ANUMBER ALIST ATUPLE ANIDENTIFIER ANARRAY ASEQ
  #define ASEQUENCE2 ANIDENTIFIER2 ASEQ ALIST ANUMBER ATUPLE
  #define ASEQUENCE3 ASEQ ANUMBER2 ATUPLE
  #define ASEQUENCE4
  
  /* ENUM */
  
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ENUM(ASEQUENCE4,BOOST_VMD_RETURN_TYPE)));
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(1,BOOST_PP_VARIADIC_ELEM(3,BOOST_VMD_ENUM(ASEQUENCE2,BOOST_VMD_RETURN_TYPE))),249);
  BOOST_TEST_EQ(BOOST_PP_VARIADIC_SIZE(BOOST_VMD_ENUM(ASEQUENCE,BOOST_VMD_RETURN_TYPE)),6);
  BOOST_TEST(BOOST_VMD_EQUAL(BOOST_PP_TUPLE_ELEM(0,BOOST_PP_VARIADIC_ELEM(0,BOOST_VMD_ENUM(ASEQUENCE3,BOOST_VMD_RETURN_TYPE))),BOOST_VMD_TYPE_SEQ,BOOST_VMD_TYPE_TYPE));
  
  /* ENUM DATA */
  
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ENUM(ASEQUENCE4)));
  BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(BOOST_PP_VARIADIC_ELEM(0,BOOST_VMD_ENUM(ASEQUENCE2)),(aaa,bbb,dvd)));
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(2,BOOST_PP_VARIADIC_ELEM(2,BOOST_VMD_ENUM(ASEQUENCE3))),2);
  BOOST_TEST_EQ(BOOST_PP_ARRAY_ELEM(2,BOOST_PP_VARIADIC_ELEM(4,BOOST_VMD_ENUM(ASEQUENCE))),38);
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
