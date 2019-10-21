
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/tuple/is_vmd_tuple.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_TUPLE (*,#,zz)
  #define AN_EMPTY_VMD_TUPLE
  
#if !BOOST_VMD_MSVC_V8  

  #define AN_EMPTY_TUPLE ()
  #define AN_EMPTY_TUPLE_PLUS () 83
  
#endif

  #define A_TUPLE2 (*,#,(zz,44,(e7)))
  #define A_TUPLE_PLUS (mmf,34,^^,!) 456
  #define PLUS_ATUPLE yyt (j,ii%)
  #define JDATA ggh 
  #define KDATA (ggh)
  #define NOT_TUPLE y6()
  #define NOT_TUPLE2 &(kkkgg,(e))
  #define A_SEQ (r)(%)(#)
  #define AN_ARRAY (4,(5,7,f,x))
  #define A_LIST (e,(g,(&,BOOST_PP_NIL)))
  
  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE());
  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE(AN_EMPTY_VMD_TUPLE));
  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE(A_TUPLE));
  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE(A_TUPLE2));
  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE(A_TUPLE_PLUS));
  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE(PLUS_ATUPLE));
  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE(JDATA));
  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE(KDATA));
  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE(NOT_TUPLE));
  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE(NOT_TUPLE2));
  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE(A_SEQ));
  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE(AN_ARRAY));
  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE(A_LIST));
  
#if !BOOST_VMD_MSVC_V8  

  BOOST_TEST(BOOST_VMD_IS_VMD_TUPLE(AN_EMPTY_TUPLE));
  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE(AN_EMPTY_TUPLE_PLUS));
  
#endif

  BOOST_TEST(!BOOST_VMD_IS_VMD_TUPLE((y)2(x)));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
