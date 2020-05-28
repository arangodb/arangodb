
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
  #define BOOST_VMD_DETECT_aaa_aaa
  #define BOOST_VMD_DETECT_bbb_bbb
  #define BOOST_VMD_DETECT_ccc_ccc
  #define BOOST_VMD_DETECT_ddd_ddd
  
  #define ANARRAY (3,(aaa,bbb,38))
  #define ANARRAY2 (3,(bbb,aaa,38))
  #define ANARRAY3 (3,(bbb,aaa,38))
  #define ANARRAY4 (3,(aaa,(aaa,(bbb,(ccc,BOOST_PP_NIL))),(ccc,ddd,(1)(2))))
  #define ANARRAY5 (3,(aaa,(aaa,(bbb,(ccc,BOOST_PP_NIL))),(ccc,ddd,(1)(2))))
  #define ANARRAY6 (4,(aaa,(aaa,(bbb,(ccc,BOOST_PP_NIL))),(ccc,ddd,(1)(2),37)))
  #define ATUPLE (aaa)
  #define ATUPLE2 (aaa)
  
  BOOST_TEST(BOOST_VMD_EQUAL(ANARRAY4,ANARRAY5,BOOST_VMD_TYPE_ARRAY));
  BOOST_TEST(BOOST_VMD_EQUAL(ANARRAY2,ANARRAY3,BOOST_VMD_TYPE_ARRAY));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ANARRAY,ANARRAY2,BOOST_VMD_TYPE_ARRAY));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ANARRAY5,ANARRAY6,BOOST_VMD_TYPE_ARRAY));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ATUPLE,ATUPLE2,BOOST_VMD_TYPE_ARRAY));

#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
