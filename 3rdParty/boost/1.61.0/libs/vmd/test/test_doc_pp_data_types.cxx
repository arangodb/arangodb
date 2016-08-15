
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/elem.hpp>
#include <boost/vmd/is_array.hpp>
#include <boost/vmd/is_list.hpp>
#include <boost/vmd/is_seq.hpp>
#include <boost/vmd/is_tuple.hpp>
#include <boost/vmd/is_empty_array.hpp>
#include <boost/vmd/is_empty_list.hpp>
#include <boost/vmd/is_parens_empty.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS
 
 #define ANARRAY (3,(a,b,c))
 #define ALIST (a,(b,(c,BOOST_PP_NIL)))
 #define ATUPLE (a,b,c)
 #define ASEQ (a)(b)(c)
 
 BOOST_TEST(BOOST_VMD_IS_TUPLE(ANARRAY));
 BOOST_TEST(BOOST_VMD_IS_TUPLE(ALIST));
 BOOST_TEST(BOOST_VMD_IS_TUPLE(ATUPLE));
 BOOST_TEST(!BOOST_VMD_IS_TUPLE(ASEQ));
 
 #define ALIST1 (2,(3,BOOST_PP_NIL))
 #define ALIST2 (2,(3,(4,BOOST_PP_NIL)))
 #define ALIST3 (2,BOOST_PP_NIL)
 
 BOOST_TEST(BOOST_VMD_IS_LIST(ALIST1));
 BOOST_TEST(BOOST_VMD_IS_LIST(ALIST2));
 BOOST_TEST(BOOST_VMD_IS_LIST(ALIST3));
 BOOST_TEST(BOOST_VMD_IS_ARRAY(ALIST1));
 BOOST_TEST(BOOST_VMD_IS_ARRAY(ALIST2));
 BOOST_TEST(!BOOST_VMD_IS_ARRAY(ALIST3));
 
 #define ASE_TUPLE (a)
 
 BOOST_TEST(BOOST_VMD_IS_TUPLE(ASE_TUPLE));
 BOOST_TEST(BOOST_VMD_IS_SEQ(ASE_TUPLE));
 
 #define A_TUPLE2 (&anything,(1,2))
 
 BOOST_TEST(BOOST_VMD_IS_TUPLE(A_TUPLE2));
// BOOST_VMD_IS_ARRAY(A_TUPLE2) will give a preprocessing error due to the constraint
 
 #define A_TUPLE3 (element,&anything)

 BOOST_TEST(BOOST_VMD_IS_TUPLE(A_TUPLE3));
// BOOST_VMD_IS_LIST(A_TUPLE3) will give a preprocessing error due to the constraint
 
// #define A_BAD_EMPTY_LIST &BOOST_PP_NIL

// BOOST_VMD_IS_LIST(A_BAD_EMPTY_LIST) will give a preprocessing error due to the constraint
// BOOST_VMD_IS_IDENTIFIER(A_BAD_EMPTY_LIST) will give a preprocessing error due to the constraint
 
 #define ST_DATA (somedata)(some_other_data)
 
 BOOST_TEST(BOOST_VMD_IS_SEQ(ST_DATA));
 BOOST_TEST(!BOOST_VMD_IS_TUPLE(ST_DATA));
 
 #define ST_DATA2 (somedata)(element1,element2)
 #define ST_DATA3 (element1,element2)(somedata)
 #define ST_DATA4 (somedata)(some_other_data)(element1,element2)
 
 BOOST_TEST(BOOST_VMD_IS_TUPLE(BOOST_VMD_ELEM(0,ST_DATA2)));
 BOOST_TEST(BOOST_VMD_IS_TUPLE(BOOST_VMD_ELEM(1,ST_DATA2)));
 BOOST_TEST(BOOST_VMD_IS_TUPLE(BOOST_VMD_ELEM(0,ST_DATA3)));
 BOOST_TEST(BOOST_VMD_IS_TUPLE(BOOST_VMD_ELEM(1,ST_DATA3)));
 BOOST_TEST(BOOST_VMD_IS_SEQ(BOOST_VMD_ELEM(0,ST_DATA4)));
 BOOST_TEST(BOOST_VMD_IS_TUPLE(BOOST_VMD_ELEM(1,ST_DATA4)));
 
 #define AN_ARRAY (1,(1))
 #define AN_EMPTY_ARRAY (0,())
 
 BOOST_TEST(BOOST_VMD_IS_ARRAY(AN_ARRAY));
 BOOST_TEST(BOOST_VMD_IS_ARRAY(AN_EMPTY_ARRAY));
 
 BOOST_TEST(BOOST_VMD_IS_EMPTY_ARRAY(AN_EMPTY_ARRAY));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY());
 BOOST_TEST(!BOOST_VMD_IS_EMPTY_ARRAY(AN_ARRAY));
 
 #define A_LIST (1,BOOST_PP_NIL)
 #define AN_EMPTY_LIST BOOST_PP_NIL
 
 BOOST_TEST(BOOST_VMD_IS_LIST(A_LIST));
 BOOST_TEST(BOOST_VMD_IS_LIST(AN_EMPTY_LIST));
 
 BOOST_TEST(BOOST_VMD_IS_EMPTY_LIST(AN_EMPTY_LIST));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY_LIST());
 BOOST_TEST(!BOOST_VMD_IS_EMPTY_LIST(A_LIST));
 
#if !BOOST_VMD_MSVC_V8
 
 #define EMPTY_PARENS ()
 
 BOOST_TEST(BOOST_VMD_IS_TUPLE(EMPTY_PARENS));
 BOOST_TEST(BOOST_VMD_IS_SEQ(EMPTY_PARENS));
 BOOST_TEST(BOOST_VMD_IS_PARENS_EMPTY(EMPTY_PARENS));
 
#endif
 
 #define TUPLE (0)
 #define SEQ (0)(1)
 
 BOOST_TEST(!BOOST_VMD_IS_PARENS_EMPTY());
 BOOST_TEST(!BOOST_VMD_IS_PARENS_EMPTY(TUPLE));
 BOOST_TEST(!BOOST_VMD_IS_PARENS_EMPTY(SEQ));
 
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
