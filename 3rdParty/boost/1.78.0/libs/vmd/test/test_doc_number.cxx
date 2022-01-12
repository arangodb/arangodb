
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_number.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS
 
 BOOST_TEST(BOOST_VMD_IS_NUMBER(0));
 BOOST_TEST(BOOST_VMD_IS_NUMBER(44));
 BOOST_TEST(!BOOST_VMD_IS_NUMBER(SQUARE));
 BOOST_TEST(!BOOST_VMD_IS_NUMBER(44 DATA));
 BOOST_TEST(!BOOST_VMD_IS_NUMBER(044));
 BOOST_TEST(BOOST_VMD_IS_NUMBER(256));
 BOOST_TEST(!BOOST_VMD_IS_NUMBER(1025));
 BOOST_TEST(!BOOST_VMD_IS_NUMBER((44)));
 
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
