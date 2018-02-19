// Copyright (C) 2006 Arkadiy Vertleyb
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (http://www.boost.org/LICENSE_1_0.txt)

#include "test.hpp"

BOOST_STATIC_ASSERT(boost::type_of::test<double(*)()>::value);
BOOST_STATIC_ASSERT(boost::type_of::test<double(*)(int, double, short, char*, bool, char, float, long, unsigned short)>::value);
BOOST_STATIC_ASSERT(boost::type_of::test<void(*)()>::value);
BOOST_STATIC_ASSERT(boost::type_of::test<void(*)(int, double, short, char*, bool, char, float, long, unsigned short)>::value);
BOOST_STATIC_ASSERT(boost::type_of::test<void(*)(...)>::value);
BOOST_STATIC_ASSERT(boost::type_of::test<void(*)(int, double, short, char*, bool, char, float, long, unsigned short, ...)>::value);

// check that const gets stripped from function pointer

int foo1(double);
int foo2(...);
typedef int(*PTR1)(double);
typedef int(*PTR2)(...);
typedef const PTR1 CPTR1;
typedef const PTR2 CPTR2;
CPTR1 cptr1 = foo1;
CPTR2 cptr2 = foo2;

BOOST_STATIC_ASSERT((boost::is_same<BOOST_TYPEOF(cptr1), PTR1>::value));
BOOST_STATIC_ASSERT((boost::is_same<BOOST_TYPEOF(cptr2), PTR2>::value));
