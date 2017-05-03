
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/elem.hpp>
#include <boost/vmd/is_empty.hpp>
#include <boost/vmd/is_identifier.hpp>
#include <boost/vmd/is_list.hpp>
#include <boost/vmd/is_type.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS
 
 #define BOOST_VMD_REGISTER_ANAME (ANAME)
 #define A_SEQUENCE (1,2,3) 46 (list_data1,(list_data2,BOOST_PP_NIL)) BOOST_VMD_TYPE_SEQ ANAME
 #define AN_EMPTY_SEQUENCE
 
 BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(2,BOOST_VMD_ELEM(0,A_SEQUENCE)),3);
 BOOST_TEST_EQ(BOOST_VMD_ELEM(1,A_SEQUENCE),46);
 BOOST_TEST(BOOST_VMD_IS_LIST(BOOST_VMD_ELEM(2,A_SEQUENCE)));
 BOOST_TEST(BOOST_VMD_IS_TYPE(BOOST_VMD_ELEM(3,A_SEQUENCE)));
 BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(BOOST_VMD_ELEM(4,A_SEQUENCE)));
 BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ELEM(5,A_SEQUENCE)));
 BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ELEM(0,AN_EMPTY_SEQUENCE)));
 
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
