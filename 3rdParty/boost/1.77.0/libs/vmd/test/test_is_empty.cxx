
//  (C) Copyright Edward Diener 2011-2015,2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_empty.hpp>
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/preprocessor/variadic/has_opt.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define DATA
  #define OBJECT OBJECT2
  #define OBJECT2
  #define FUNC(x) FUNC2(x)
  #define FUNC2(x)
  
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_PP_EMPTY()));
  BOOST_TEST(BOOST_VMD_IS_EMPTY(DATA BOOST_PP_EMPTY()));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(x BOOST_PP_EMPTY()));
  BOOST_TEST(BOOST_VMD_IS_EMPTY(OBJECT BOOST_PP_EMPTY()));
  BOOST_TEST(BOOST_VMD_IS_EMPTY(FUNC(z) BOOST_PP_EMPTY()));
  
  #define FUNC_GEN() ()
  #define FUNC_GEN2(x) ()
  #define FUNC_GEN3() anything
  #define FUNC_GEN4(x) anything
  
#if BOOST_PP_VARIADIC_HAS_OPT()

  #define FUNC_GEN5(x,y) ()
  #define FUNC_GEN6(x,y) anything
  #define FUNC_GEN7() (y,z)
  #define FUNC_GEN5A() (&)
  #define FUNC_GEN6A(x) (y)
  #define FUNC_GEN7A() (y,z)
  
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN2));
  
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN5));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN6));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN7));

  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN5A));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN6A));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN7A));
  
#elif BOOST_VMD_MSVC

  #define FUNC_GEN5(x,y) ()
  #define FUNC_GEN6(x,y) anything
  
  /* This shows that VC++ does not work correctly in these cases. */

  BOOST_TEST(BOOST_VMD_IS_EMPTY(FUNC_GEN));  /* This produces the wrong result */
  BOOST_TEST(BOOST_VMD_IS_EMPTY(FUNC_GEN2)); /* This produces the wrong result */
  
  BOOST_TEST(BOOST_VMD_IS_EMPTY(FUNC_GEN5));  /* This should produce a compiler error but does not and produces the incorrect result */
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN6)); /* This should produce a compiler error but does not */

#else

  #define FUNC_GEN5() (&)
  #define FUNC_GEN6(x) (y)
  #define FUNC_GEN7() (y,z)
  
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN2));
  
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN5));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN6));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN7));
  
#endif /* BOOST_VMD_MSVC */

  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN3));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(FUNC_GEN4));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif /* BOOST_PP_VARIADICS */

  return boost::report_errors();
  
  }
