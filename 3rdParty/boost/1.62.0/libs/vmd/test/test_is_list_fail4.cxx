
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_list.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS
  
  BOOST_TEST(BOOST_VMD_IS_LIST((tt,(5,(uu,BOOST_PP_NIL yy)))));
  
#endif

  return boost::report_errors();
  
  }
