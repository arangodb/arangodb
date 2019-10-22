
//  (C) Copyright Edward Diener 2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/list/to_tuple.hpp>
#include <boost/vmd/is_empty.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_LIST (1,(2,(3,(4,BOOST_PP_NIL))))
  #define AN_EMPTY_LIST BOOST_PP_NIL
  
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(0,BOOST_VMD_LIST_TO_TUPLE(A_LIST)),1);
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_LIST_TO_TUPLE(AN_EMPTY_LIST)));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
