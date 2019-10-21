
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_parens_empty.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define DATA () * 4
  #define DATA2 4 * ()
  #define DATA3 (X)
  #define DATA4 ()
  #define DATA5 (   )
  
  BOOST_TEST(!BOOST_VMD_IS_PARENS_EMPTY(DATA));
  BOOST_TEST(!BOOST_VMD_IS_PARENS_EMPTY(DATA2));
  BOOST_TEST(!BOOST_VMD_IS_PARENS_EMPTY(DATA3));
  BOOST_TEST(BOOST_VMD_IS_PARENS_EMPTY(DATA4));
  BOOST_TEST(BOOST_VMD_IS_PARENS_EMPTY(DATA5));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
