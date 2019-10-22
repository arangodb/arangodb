
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_empty_array.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define AN_ARRAY_PLUS (4,(mmf,34,^^,!)) 456
  #define PLUS_ANARRAY yyt (2,(j,ii%))
  #define JDATA ggh
  #define KDATA (2,(a,b)) name
  #define A_SEQ ((1,(25)))((1,(26)))((1,(27)))
  #define AN_EMPTY_ARRAY_PLUS (0,()) 46
  #define EMPTY_ARRAY_INVALID ("string",() xx)
  #define EMPTY_ARRAY (0,())
  
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(anything));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(AN_ARRAY_PLUS));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(PLUS_ANARRAY));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(JDATA));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(KDATA));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(A_SEQ));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(AN_EMPTY_ARRAY_PLUS));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(EMPTY_ARRAY_INVALID));
  BOOST_TEST(BOOST_VMD_IS_EMPTY_ARRAY(EMPTY_ARRAY));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
