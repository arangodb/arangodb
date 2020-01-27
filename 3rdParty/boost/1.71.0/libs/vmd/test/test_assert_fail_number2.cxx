
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/assert.hpp>
#include <boost/vmd/number2.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_SEQ (73 (split) clear)(red)(green 44)
  
  BOOST_VMD_ASSERT(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(0,BOOST_VMD_NUMBER(BOOST_PP_SEQ_ELEM(0,A_SEQ))),72),BOOST_VMD_TEST_FAIL_NUMBER)
  
#else
  
  typedef char BOOST_VMD_TEST_FAIL_NUMBER[-1];
  
#endif

  return boost::report_errors();
  
  }
