
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/assert_is_tuple.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comparison/greater.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/tuple/size.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

#if BOOST_VMD_ASSERT_DATA
 
#if BOOST_VMD_MSVC

 #define AMACRO(atuple) \
     BOOST_PP_CAT \
        ( \
        BOOST_VMD_ASSERT_IS_TUPLE(atuple), \
        BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_TUPLE_SIZE(atuple), 2),1,0) \
        )
        
#else

 #define AMACRO(atuple) \
     BOOST_VMD_ASSERT_IS_TUPLE(atuple) \
     BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_TUPLE_SIZE(atuple), 2),1,0)
     
#endif

 BOOST_TEST(AMACRO(73));
 
#else
 
  typedef char BOOST_VMD_DOC_ASSERT_FAIL_ERROR[-1];
  
#endif

#endif

  return boost::report_errors();
  
  }
