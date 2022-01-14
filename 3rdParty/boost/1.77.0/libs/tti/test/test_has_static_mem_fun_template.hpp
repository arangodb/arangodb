
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(TEST_HAS_STATIC_MEM_FUN_TEMPLATE_HPP)
#define TEST_HAS_STATIC_MEM_FUN_TEMPLATE_HPP

#include "test_structs.hpp"
#include <boost/tti/has_static_member_function_template.hpp>


#if BOOST_PP_VARIADICS

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(StatFuncTemplate,short,long,7854)
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(TAnother,AnotherFuncTemplate,int,long,long)
BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(YetAnotherFuncTemplate,double,float)

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(StaticFTWithDefault,int,long,bool) // default parameter

#else

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(StatFuncTemplate,(3,(short,long,7854)))
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(TAnother,AnotherFuncTemplate,(3,(int,long,long)))
BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(YetAnotherFuncTemplate,(2,(double,float)))

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE(StaticFTWithDefault,(3,(int,long,bool))) // default parameter

#endif

#endif // TEST_HAS_STATIC_MEM_FUN_TEMPLATE_HPP
