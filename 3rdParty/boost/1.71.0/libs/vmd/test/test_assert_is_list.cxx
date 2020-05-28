
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/assert_is_list.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_LIST (e,(g,(&,BOOST_PP_NIL)))
  
  BOOST_VMD_ASSERT_IS_LIST((4,(x,BOOST_PP_NIL)))
  BOOST_VMD_ASSERT_IS_LIST(((3,6m,irj),(x,BOOST_PP_NIL)))
  BOOST_VMD_ASSERT_IS_LIST(A_LIST)
  BOOST_VMD_ASSERT_IS_LIST(BOOST_PP_NIL)

#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
