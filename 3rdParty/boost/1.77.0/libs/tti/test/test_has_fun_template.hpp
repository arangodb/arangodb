
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(BOOST_TEST_HAS_FUNCTION_TEMPLATE_HPP)
#define BOOST_TEST_HAS_FUNCTION_TEMPLATE_HPP

#include "test_structs.hpp"
#include <boost/tti/has_function_template.hpp>

#if BOOST_PP_VARIADICS

BOOST_TTI_HAS_FUNCTION_TEMPLATE(StatFuncTemplate,int,bool,37)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(Sftem,SomeFuncTemplate,char,unsigned,float,925)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(AnFT,AFuncTemplate,unsigned,45623)
BOOST_TTI_HAS_FUNCTION_TEMPLATE(AFuncTemplate,double,unsigned char,int)
BOOST_TTI_HAS_FUNCTION_TEMPLATE(FTHasDef,int)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(ACFunTem,AConstFunctionTemplate,unsigned,int)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(WConstFT,WFunctionTmp,int,long,unsigned long)
BOOST_TTI_HAS_FUNCTION_TEMPLATE(AVolatileFT,int)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(VTempl,VolFTem,int,44339)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(ACVF,ACVFunTemplate,int,long)
BOOST_TTI_HAS_FUNCTION_TEMPLATE(ConstVolTTFun,7371)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(AnotherFT,AnotherFuncTemplate,int,long,bool)

BOOST_TTI_HAS_FUNCTION_TEMPLATE(MyFuncTemplate,unsigned char)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(VWDef,VWithDefault,3281)
BOOST_TTI_HAS_FUNCTION_TEMPLATE(YetAnotherFuncTemplate,int,unsigned char)
BOOST_TTI_HAS_FUNCTION_TEMPLATE(StaticFTWithDefault,long,int,long)

BOOST_TTI_HAS_FUNCTION_TEMPLATE(Nonexistent,int,long)
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(NotExist,TDoesNotExist,char)

#else

BOOST_TTI_HAS_FUNCTION_TEMPLATE(StatFuncTemplate,(3,(int,bool,37)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(Sftem,SomeFuncTemplate,(4,(char,unsigned,float,925)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(AnFT,AFuncTemplate,(2,(unsigned,45623)))
BOOST_TTI_HAS_FUNCTION_TEMPLATE(AFuncTemplate,(3,(double,unsigned char,int)))
BOOST_TTI_HAS_FUNCTION_TEMPLATE(FTHasDef,(1,(int)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(ACFunTem,AConstFunctionTemplate,(2,(unsigned,int)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(WConstFT,WFunctionTmp,(3,(int,long,unsigned long)))
BOOST_TTI_HAS_FUNCTION_TEMPLATE(AVolatileFT,(1,(int)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(VTempl,VolFTem,(2,(int,44339)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(ACVF,ACVFunTemplate,(2,(int,long)))
BOOST_TTI_HAS_FUNCTION_TEMPLATE(ConstVolTTFun,(1,(7371)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(AnotherFT,AnotherFuncTemplate,(3,(int,long,bool)))

BOOST_TTI_HAS_FUNCTION_TEMPLATE(MyFuncTemplate,(1,(unsigned char)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(VWDef,VWithDefault,(1,(3281)))
BOOST_TTI_HAS_FUNCTION_TEMPLATE(YetAnotherFuncTemplate,(2,(int,unsigned char)))
BOOST_TTI_HAS_FUNCTION_TEMPLATE(StaticFTWithDefault,(3,(long,int,long)))

BOOST_TTI_HAS_FUNCTION_TEMPLATE(Nonexistent,(2,(int,long)))
BOOST_TTI_TRAIT_HAS_FUNCTION_TEMPLATE(NotExist,TDoesNotExist,(1,(char)))

#endif

#endif // BOOST_TEST_HAS_FUNCTION_TEMPLATE_HPP
