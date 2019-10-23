
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/assert.hpp>
#include <boost/vmd/is_number.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/list/at.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_LIST (eeb,(grist,(152,BOOST_PP_NIL)))
  
  BOOST_VMD_ASSERT(BOOST_VMD_IS_NUMBER(BOOST_PP_LIST_AT(A_LIST,1)),BOOST_VMD_TEST_FAIL_IS_NUMBER)
  
#endif

  return boost::report_errors();
  
  }
