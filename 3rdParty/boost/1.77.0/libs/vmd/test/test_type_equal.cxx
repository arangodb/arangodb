
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
  
  BOOST_TEST(BOOST_VMD_EQUAL(BOOST_VMD_TYPE_LIST,BOOST_VMD_TYPE_LIST,BOOST_VMD_TYPE_TYPE));
  BOOST_TEST(BOOST_VMD_EQUAL(BOOST_VMD_TYPE_SEQ,BOOST_VMD_TYPE_SEQ,BOOST_VMD_TYPE_TYPE));
  BOOST_TEST(BOOST_VMD_EQUAL(BOOST_VMD_TYPE_SEQ,BOOST_VMD_TYPE_SEQ,BOOST_VMD_TYPE_IDENTIFIER));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(BOOST_VMD_TYPE_EMPTY,BOOST_VMD_TYPE_SEQUENCE,BOOST_VMD_TYPE_TYPE));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(BOOST_VMD_TYPE_TUPLE,NOT_TYPE,BOOST_VMD_TYPE_TYPE));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(BOOST_VMD_TYPE_TYPE,(3),BOOST_VMD_TYPE_TYPE));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
