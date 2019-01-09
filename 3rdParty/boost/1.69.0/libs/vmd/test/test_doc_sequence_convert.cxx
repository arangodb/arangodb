
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_empty_array.hpp>
#include <boost/vmd/is_identifier.hpp>
#include <boost/vmd/is_type.hpp>
#include <boost/vmd/to_array.hpp>
#include <boost/vmd/to_list.hpp>
#include <boost/vmd/to_seq.hpp>
#include <boost/vmd/to_tuple.hpp>
#include <boost/vmd/enum.hpp>
#include <boost/preprocessor/list/at.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/elem.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS
 
 #define BOOST_VMD_REGISTER_ANID (ANID)
 
 #define SEQUENCE_EMPTY
 #define SEQUENCE_SINGLE 35
 #define SEQUENCE_SINGLE_2 ANID
 #define SEQUENCE_MULTI (0,1) (2)(3)(4)
 #define SEQUENCE_MULTI_2 BOOST_VMD_TYPE_SEQ (2,(5,6))

 BOOST_TEST(BOOST_VMD_IS_EMPTY_ARRAY(BOOST_VMD_TO_ARRAY(SEQUENCE_EMPTY)));
 BOOST_TEST_EQ(BOOST_PP_LIST_AT(BOOST_VMD_TO_LIST(SEQUENCE_SINGLE),0),35);
 BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(BOOST_PP_SEQ_ELEM(0,BOOST_VMD_TO_SEQ(SEQUENCE_SINGLE_2))));
 BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(1,BOOST_PP_TUPLE_ELEM(0,BOOST_VMD_TO_TUPLE(SEQUENCE_MULTI))),1);
 BOOST_TEST(BOOST_VMD_IS_TYPE(BOOST_PP_VARIADIC_ELEM(0,BOOST_VMD_ENUM(SEQUENCE_MULTI_2))));
 
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
