
//  (C) Copyright Edward Diener 2011-2015,2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_empty.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/elem.hpp>
#include <boost/preprocessor/variadic/has_opt.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

 #define USE_VA_OPT BOOST_PP_VARIADIC_HAS_OPT()

 #define SMACRO() someoutput
 #define EMACRO(x) otheroutput x
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(SMACRO()));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(EMACRO(somedata)));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(EMACRO()));
 
 #define MMACRO(x,y,z) x y z
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(MMACRO(1,,2)));
 
 #define VMACRO(x,...) x __VA_ARGS__
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(VMACRO(somedata,)));
 BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_PP_VARIADIC_ELEM(1,VMACRO(somedata,vdata1,,vdata3))));
 
 #define TMACRO(x,atuple) x atuple
 
#if !BOOST_VMD_MSVC_V8
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(TMACRO(somedata,())));
 
#endif
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(TMACRO(somedata,(telem1,,telem2,teleem3))));
 
 #define RMACRO(x,y,z)
 
 BOOST_TEST(BOOST_VMD_IS_EMPTY(RMACRO(data1,data2,data3)));
 
#if !BOOST_VMD_MSVC_V8

 #define TRETMACRO(x,y,z) ()
 
 BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_PP_TUPLE_ELEM(0,TRETMACRO(1,2,3))));
 
#endif

 #define TRETMACRO1(x,y,z) (x,,y,,z)
 
 BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_PP_TUPLE_ELEM(3,TRETMACRO1(1,2,3))));
 
 #define VRETMACRO(x,y,z) x,,y,,z
 
 BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_PP_VARIADIC_ELEM(3,VRETMACRO(1,2,3))));
 
 #define FMACRO(x,y) any_output
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(FMACRO(arg1,arg2)));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(someinput FMACRO(arg1,arg2)));
 
#if BOOST_VMD_MSVC

 BOOST_TEST(!BOOST_VMD_IS_EMPTY(FMACRO(1)));
 
#endif
 
#if BOOST_VMD_MSVC

 #define FMACRO1(parameter) FMACRO3 parameter()
 #define FMACRO2() ()
 #define FMACRO3() 1
 
 int MSVC_number_one = FMACRO1(FMACRO2);

#endif
 
 #define FMACRO4() ( any_number_of_tuple_elements )
 #define FMACRO5(param) ( any_number_of_tuple_elements )
 
#if USE_VA_OPT

 #define FMACRO6(param1,param2) ( any_number_of_tuple_elements )
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(FMACRO4));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(FMACRO5));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(FMACRO6));
 
#elif BOOST_VMD_MSVC

 #define FMACRO6(param1,param2) ( any_number_of_tuple_elements )

 BOOST_TEST(BOOST_VMD_IS_EMPTY(FMACRO4));
 BOOST_TEST(BOOST_VMD_IS_EMPTY(FMACRO5));
 BOOST_TEST(BOOST_VMD_IS_EMPTY(FMACRO6));
 
#else
 
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(FMACRO4));
 BOOST_TEST(!BOOST_VMD_IS_EMPTY(FMACRO5));
 
#endif

#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
