// Copyright (C) 2006 Arkadiy Vertleyb
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (http://www.boost.org/LICENSE_1_0.txt)

#include <boost/typeof/typeof.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/static_assert.hpp>

int foo1(double);
int foo2(...);
int foo3(int, ...);
typedef int(&FREF1)(double);
typedef int(&FREF2)(...);
typedef int(&FREF3)(int, ...);
FREF1 fref1 = *foo1;
FREF2 fref2 = *foo2;
FREF3 fref3 = *foo3;

BOOST_STATIC_ASSERT((boost::is_same<BOOST_TYPEOF(fref1), int(double)>::value));
BOOST_STATIC_ASSERT((boost::is_same<BOOST_TYPEOF(fref2), int(...)>::value));
BOOST_STATIC_ASSERT((boost::is_same<BOOST_TYPEOF(fref3), int(int,...)>::value));
