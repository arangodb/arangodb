/* Boost.Flyweight test of flyweight forwarding and initializer_list ctors.
 *
 * Copyright 2006-2015 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/flyweight for library home page.
 */

#include "test_multictor.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/detail/lightweight_test.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/flyweight.hpp> 
#include <boost/functional/hash.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

using boost::flyweight;

#if BOOST_WORKAROUND(BOOST_MSVC,<1300)
#define NONCONST const
#else
#define NONCONST
#endif

struct multictor
{
  typedef multictor type;

  multictor():
    t(0,0,0.0,"",false){}
  multictor(NONCONST int& x0):
    t(x0,0,0.0,"",false){}
  multictor(int x0,NONCONST char& x1):
    t(x0,x1,0.0,"",false){}
  multictor(int x0,char x1,NONCONST double& x2):
    t(x0,x1,x2,"",false){}
  multictor(int x0,char x1,double x2,NONCONST std::string& x3):
    t(x0,x1,x2,x3,false){}
  multictor(int x0,char x1,double x2,const std::string& x3,NONCONST bool& x4):
    t(x0,x1,x2,x3,x4){}

  friend bool operator==(const type& x,const type& y){return x.t==y.t;}
  friend bool operator< (const type& x,const type& y){return x.t< y.t;}
  friend bool operator!=(const type& x,const type& y){return x.t!=y.t;}
  friend bool operator> (const type& x,const type& y){return x.t> y.t;}
  friend bool operator>=(const type& x,const type& y){return x.t>=y.t;}
  friend bool operator<=(const type& x,const type& y){return x.t<=y.t;}

  boost::tuples::tuple<int,char,double,std::string,bool> t;
};

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
namespace boost{
#endif

inline std::size_t hash_value(const multictor& x)
{
  std::size_t res=0;
  boost::hash_combine(res,boost::tuples::get<0>(x.t));
  boost::hash_combine(res,boost::tuples::get<1>(x.t));
  boost::hash_combine(res,boost::tuples::get<2>(x.t));
  boost::hash_combine(res,boost::tuples::get<3>(x.t));
  boost::hash_combine(res,boost::tuples::get<4>(x.t));
  return res;
}

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
} /* namespace boost */
#endif

#if !defined(BOOST_NO_SFINAE)&&!defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX)
#define INIT0(_) {}
#define INIT1(a) {a}
#define INIT2(a,b) {a,b}
#define INIT_LIST1(a) {a}
#define INIT_LIST2(a,b) {a,b}
#else
#define INIT0(_)
#define INIT1(a) ((a))
#define INIT2(a,b) ((a),(b))
#define INIT_LIST1(a) ({a})
#define INIT_LIST2(a,b) ({a,b})
#endif

struct initctor
{
  struct arg{arg(int= 0){}};
  
  initctor():res(-1){}
  initctor(arg,arg):res(-2){}
  initctor(int,unsigned int):res(-3){}
  
  initctor(std::initializer_list<int> list):res(0)
  {
    typedef const int* iterator;
    for(iterator it=list.begin(),it_end=list.end();it!=it_end;++it){
      res+=*it;
    }
  }
  
  initctor(std::initializer_list<unsigned int> list):res(0)
  {
    typedef const unsigned int* iterator;
    for(iterator it=list.begin(),it_end=list.end();it!=it_end;++it){
      res+=(int)(*it)*2;
    }
  }

  friend bool operator==(const initctor& x,const initctor& y)
  {
    return x.res==y.res;
  }

  int res;
};

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
namespace boost{
#endif

inline std::size_t hash_value(const initctor& x)
{
  return (std::size_t)(x.res);
}

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
} /* namespace boost */
#endif

#endif

void test_multictor()
{
  flyweight<multictor> f;
  multictor            m;
  BOOST_TEST(f==m);

  int x0=1;
  flyweight<multictor> f0(x0);
  multictor            m0(x0);
  BOOST_TEST(f0==m0);

  char x1='a';
  flyweight<multictor> f1(1,x1);
  multictor            m1(1,x1);
  BOOST_TEST(f1==m1);

  double x2=3.1416;
  flyweight<multictor> f2(1,'a',x2);
  multictor            m2(1,'a',x2);
  BOOST_TEST(f2==m2);

  std::string x3("boost");
  flyweight<multictor> f3(1,'a',3.1416,x3);
  multictor            m3(1,'a',3.1416,x3);
  BOOST_TEST(f3==m3);

  bool x4=true;
  flyweight<multictor> f4(1,'a',3.1416,"boost",x4);
  multictor            m4(1,'a',3.1416,"boost",x4);
  BOOST_TEST(f4==m4);

#if !defined(BOOST_NO_SFINAE)&&!defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
  flyweight<initctor> ff INIT0(~);
  BOOST_TEST(ff.get().res==-1);

  ff=flyweight<initctor> INIT2(initctor::arg(),1);
  BOOST_TEST(ff.get().res==-2);

  flyweight<initctor> ff0 INIT2(initctor::arg(),initctor::arg());
  BOOST_TEST(ff0.get().res==-2);
  
  ff0={1};
  BOOST_TEST(ff0.get().res==1);
  
  flyweight<initctor> ff1 INIT_LIST2(1,2);
  BOOST_TEST(ff1.get().res==3);
  
  ff1={1u,2u,3u};
  BOOST_TEST(ff1.get().res==12);

  flyweight<initctor> ff2 INIT_LIST1(1u);
  BOOST_TEST(ff2.get().res==2);
#endif
}
