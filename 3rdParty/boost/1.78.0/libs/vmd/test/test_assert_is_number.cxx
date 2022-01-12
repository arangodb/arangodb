
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/assert_is_number.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/list/at.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
  #if BOOST_PP_VARIADICS
    
  #define A_TUPLE (4,241,zzz)
  #define KDATA 213
  #define A_SEQ (num)(78)(42)
  #define A_LIST (eeb,(grist,(152,BOOST_PP_NIL)))
    
  BOOST_VMD_ASSERT_IS_NUMBER(BOOST_PP_TUPLE_ELEM(1,A_TUPLE))
  BOOST_VMD_ASSERT_IS_NUMBER(KDATA)
  BOOST_VMD_ASSERT_IS_NUMBER(BOOST_PP_SEQ_ELEM(2,A_SEQ))
  BOOST_VMD_ASSERT_IS_NUMBER(BOOST_PP_LIST_AT(A_LIST,2))
  
#else

  BOOST_VMD_ASSERT(0)
  
#endif

  return boost::report_errors();
  
  }
