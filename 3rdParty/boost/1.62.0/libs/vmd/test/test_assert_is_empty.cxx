
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/assert_is_empty.hpp>
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define DATA
  #define OBJECT OBJECT2
  #define OBJECT2
  #define FUNC(x) FUNC2(x)
  #define FUNC2(x)
  
  BOOST_VMD_ASSERT_IS_EMPTY(BOOST_PP_EMPTY())
  BOOST_VMD_ASSERT_IS_EMPTY(DATA BOOST_PP_EMPTY())
  BOOST_VMD_ASSERT_IS_EMPTY(OBJECT BOOST_PP_EMPTY())
  BOOST_VMD_ASSERT_IS_EMPTY(FUNC(z) BOOST_PP_EMPTY())
  
#if BOOST_VMD_MSVC

  #define FUNC_GEN() ()
  #define FUNC_GEN2(x) ()
  #define FUNC_GEN3(x,y) ()
  
  /* This shows that VC++ does not work correctly in these cases. */

  BOOST_VMD_ASSERT_IS_EMPTY(FUNC_GEN)  /* This incorrectly does not assert */
  BOOST_VMD_ASSERT_IS_EMPTY(FUNC_GEN2) /* This incorrectly does not assert */
  BOOST_VMD_ASSERT_IS_EMPTY(FUNC_GEN3) /* This should produce a compiler error but does not and does not assert */

#endif /* BOOST_VMD_MSVC */

#else

BOOST_ERROR("No variadic macro support");
  
#endif /* BOOST_PP_VARIADICS */

  return boost::report_errors();
  
  }
