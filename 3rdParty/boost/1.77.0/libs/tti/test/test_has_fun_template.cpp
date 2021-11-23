
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_fun_template.hpp"
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<AType::AStructType,int,boost::mpl::vector<int *,bool> >::value));
  BOOST_TEST((Sftem<AType::BType::CType,double,boost::mpl::vector<char,unsigned *,float &> >::value));
  BOOST_TEST((AnFT<AType,int,boost::mpl::vector<const unsigned &> >::value));
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<AType,void,boost::mpl::vector<double *,unsigned char,int &> >::value));
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(FTHasDef)<AType,double,boost::mpl::vector<int, long, char> >::value));
  BOOST_TEST((ACFunTem<AType,double,boost::mpl::vector<unsigned,int>,boost::function_types::const_qualified>::value));
  BOOST_TEST((WConstFT<AType,void,boost::mpl::vector<int **, long &, unsigned long>,boost::function_types::const_qualified>::value));
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(AVolatileFT)<AType,double,boost::mpl::vector<int, long, char>,boost::function_types::volatile_qualified>::value));
  BOOST_TEST((VTempl<AType,void,boost::mpl::vector<int &>,boost::function_types::volatile_qualified>::value));
  BOOST_TEST((ACVF<AType,double,boost::mpl::vector<int,long>,boost::function_types::cv_qualified>::value));
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(ConstVolTTFun)<AType,void,boost::mpl::vector<float,double>,boost::function_types::cv_qualified>::value));
  BOOST_TEST((AnotherFT<AType,void,boost::mpl::vector<int,long &,const bool &> >::value));
  
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(MyFuncTemplate)<AnotherType,long,boost::mpl::vector<unsigned char &> >::value));
  BOOST_TEST((VWDef<AnotherType,void,boost::mpl::vector<float,double>,boost::function_types::volatile_qualified>::value));
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(YetAnotherFuncTemplate)<AnotherType,void,boost::mpl::vector<const int &,unsigned char &> >::value));
  BOOST_TEST((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(StaticFTWithDefault)<AnotherType,void,boost::mpl::vector<const long &,int &,long *,long> >::value));
  
  return boost::report_errors();

  }
