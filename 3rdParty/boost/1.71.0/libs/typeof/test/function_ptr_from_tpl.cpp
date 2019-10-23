// Copyright (C) 2006 Arkadiy Vertleyb
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (http://www.boost.org/LICENSE_1_0.txt)

#include <boost/typeof/typeof.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/static_assert.hpp>

void f1() {}
void f2(...) {}

template<class T> 
struct tpl1
{
    typedef BOOST_TYPEOF_TPL(&f1) type;
};

template<class T> 
struct tpl2
{
    typedef BOOST_TYPEOF_TPL(&f2) type;
};

typedef void(*fun1_type)();
typedef void(*fun2_type)(...);
 
BOOST_STATIC_ASSERT((boost::is_same<tpl1<void>::type, fun1_type>::value));
BOOST_STATIC_ASSERT((boost::is_same<tpl2<void>::type, fun2_type>::value));
