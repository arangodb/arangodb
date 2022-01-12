
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(TEST_HAS_MEM_FUN_TEMPLATE_HPP)
#define TEST_HAS_MEM_FUN_TEMPLATE_HPP

#include "test_structs.hpp"
#include <boost/tti/has_member_function_template.hpp>

#if BOOST_PP_VARIADICS

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(SomeFuncTemplate,int,long,double,50)
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(SameName,AFuncTemplate,int,int,float)
BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(AFuncTemplate,long,9983)
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(AnotherName,MyFuncTemplate,bool)

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(someFunctionMemberTemplate,short,int) // does not exist anywhere

BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(FTD,FTHasDef,int) // default parameter

BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(FirstCMFT,AConstFunctionTemplate,short,long) // const
BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(WFunctionTmp,int,long,bool) // const

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(AVolatileFT,float) // volatile
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(VolG,VolFTem,long,44) // volatile

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(VWithDefault,46389) // volatile default parameter

BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(ACV,ACVFunTemplate,int,short) // const volatile
BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(ConstVolTTFun,8764) // const volatile

#else

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(SomeFuncTemplate,(4,(int,long,double,50)))
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(SameName,AFuncTemplate,(3,(int,int,float)))
BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(AFuncTemplate,(2,(long,9983)))
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(AnotherName,MyFuncTemplate,(1,(bool)))

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(someFunctionMemberTemplate,(2,(short,int))) // does not exist anywhere

BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(FTD,FTHasDef,(1,(int))) // default parameter

BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(FirstCMFT,AConstFunctionTemplate,(2,(short,long))) // const
BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(WFunctionTmp,(3,(int,long,bool))) // const

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(AVolatileFT,(1,(float))) // volatile
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(VolG,VolFTem,(2,(long,44))) // volatile

BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(VWithDefault,(1,(46389))) // volatile default parameter

BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION_TEMPLATE(ACV,ACVFunTemplate,(2,(int,short))) // const volatile
BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE(ConstVolTTFun,(1,(8764))) // const volatile

#endif

#endif // TEST_HAS_MEM_FUN_TEMPLATE_HPP
