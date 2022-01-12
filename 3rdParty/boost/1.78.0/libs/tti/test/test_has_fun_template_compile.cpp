
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_fun_template.hpp"
#include <boost/mpl/assert.hpp>

int main()
  {
  
  // You can always instantiate without compiler errors
  
  Sftem<AType,double,boost::mpl::vector<int,unsigned *,float &> > aVar;
  BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(Nonexistent)<AType,int,boost::mpl::vector<short,bool,int> > aVar2;
  NotExist<AnotherType,AType,boost::mpl::vector<long,char> > aVar3;
  BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<AType::AStructType,int,boost::mpl::vector<int *,bool>,boost::function_types::const_qualified> aVar4;
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<AType::AStructType,int,boost::mpl::vector<int *,bool> >));
  BOOST_MPL_ASSERT((Sftem<AType::BType::CType,double,boost::mpl::vector<char,unsigned *,float &> >));
  BOOST_MPL_ASSERT((AnFT<AType,int,boost::mpl::vector<const unsigned &> >));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<AType,void,boost::mpl::vector<double *,unsigned char,int &> >));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(FTHasDef)<AType,double,boost::mpl::vector<int, long, char> >));
  BOOST_MPL_ASSERT((ACFunTem<AType,double,boost::mpl::vector<unsigned,int>,boost::function_types::const_qualified>));
  BOOST_MPL_ASSERT((WConstFT<AType,void,boost::mpl::vector<int **, long &, unsigned long>,boost::function_types::const_qualified>));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(AVolatileFT)<AType,double,boost::mpl::vector<int, long, char>,boost::function_types::volatile_qualified>));
  BOOST_MPL_ASSERT((VTempl<AType,void,boost::mpl::vector<int &>,boost::function_types::volatile_qualified>));
  BOOST_MPL_ASSERT((ACVF<AType,double,boost::mpl::vector<int,long>,boost::function_types::cv_qualified>));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(ConstVolTTFun)<AType,void,boost::mpl::vector<float,double>,boost::function_types::cv_qualified>));
  BOOST_MPL_ASSERT((AnotherFT<AType,void,boost::mpl::vector<int,long &,const bool &> >));
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(MyFuncTemplate)<AnotherType,long,boost::mpl::vector<unsigned char &> >));
  BOOST_MPL_ASSERT((VWDef<AnotherType,void,boost::mpl::vector<float,double>,boost::function_types::volatile_qualified>));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(YetAnotherFuncTemplate)<AnotherType,void,boost::mpl::vector<const int &,unsigned char &> >));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(StaticFTWithDefault)<AnotherType,void,boost::mpl::vector<const long &,int &,long *,long> >));
  
  return 0;

  }
