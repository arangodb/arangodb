//  (C) Copyright Edward Diener 2020
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/tti/detail/dmacro_sunfix.hpp>

  struct AType 
    {
    template<class X,class Y,class Z> double SomeFuncTemplateName(X,Y *,Z &) { return 0.0; }
    template<class X,class Y,short AA> static int StatFuncTemplate(X *,Y) { int ret(AA); return ret; }
    };
    
  template
    <
    class C,
    class T
    > 
  struct TestFunctionTemplate
    {
    typedef char Bad;
    struct Good { char x[2]; };
    template<T> struct helper BOOST_TTI_DETAIL_MACRO_SUNFIX ;
    template<class U> static Good check(helper<&U::template SomeFuncTemplateName<int,long,double> > *);
    template<class U> static Bad check(...);
    static const bool value=sizeof(check<C>(0))==sizeof(Good);
    };
    
  template
    <
    class C,
    class T
    > 
  struct TestStaticFunctionTemplate
    {
    typedef char Bad;
    struct Good { char x[2]; };
    template<T *> struct helper BOOST_TTI_DETAIL_MACRO_SUNFIX ;
    template<class U> static Good check(helper<&U::template StatFuncTemplate<long,int,1234> > *);
    template<class U> static Bad check(...);
    static const bool value=sizeof(check<C>(0))==sizeof(Good);
    };
    
  int main()
    {
    
    bool bv =
    
    TestFunctionTemplate
    <
    AType,
    double (AType::*) (int,long *,double &)
    >::value
    
    &&
    
    TestStaticFunctionTemplate
    <
    AType,
    int (long *,int)
    >::value
    ;
    
    return (bv ? 0 : 1);
    }
