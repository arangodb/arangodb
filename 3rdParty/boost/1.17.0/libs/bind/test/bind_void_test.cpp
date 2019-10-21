#include <boost/config.hpp>

#if defined(BOOST_MSVC)
#pragma warning(disable: 4786)  // identifier truncated in debug info
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4711)  // function selected for automatic inline expansion
#pragma warning(disable: 4514)  // unreferenced inline removed
#endif

//
//  bind_void_test.cpp - test for bind<void>
//
//  Copyright (c) 2008 Peter Dimov
//  Copyright (c) 2014 Agustin Berge
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/bind.hpp>
#include <boost/ref.hpp>

#if defined(BOOST_MSVC) && (BOOST_MSVC < 1300)
#pragma warning(push, 3)
#endif

#include <iostream>

#if defined(BOOST_MSVC) && (BOOST_MSVC < 1300)
#pragma warning(pop)
#endif

#include <boost/detail/lightweight_test.hpp>

//

long global_result;

long f_0()
{
    return global_result = 17041L;
}

long f_1(long a)
{
    return global_result = a;
}

long f_2(long a, long b)
{
    return global_result = a + 10 * b;
}

long f_3(long a, long b, long c)
{
    return global_result = a + 10 * b + 100 * c;
}

long f_4(long a, long b, long c, long d)
{
    return global_result = a + 10 * b + 100 * c + 1000 * d;
}

long f_5(long a, long b, long c, long d, long e)
{
    return global_result = a + 10 * b + 100 * c + 1000 * d + 10000 * e;
}

long f_6(long a, long b, long c, long d, long e, long f)
{
    return global_result = a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f;
}

long f_7(long a, long b, long c, long d, long e, long f, long g)
{
    return global_result = a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g;
}

long f_8(long a, long b, long c, long d, long e, long f, long g, long h)
{
    return global_result = a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g + 10000000 * h;
}

long f_9(long a, long b, long c, long d, long e, long f, long g, long h, long i)
{
    return global_result = a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g + 10000000 * h + 100000000 * i;
}

void function_test()
{
    using namespace boost;

    int const i = 1;

    BOOST_TEST( (bind<void>(f_0)(i), (global_result == 17041L)) );
    BOOST_TEST( (bind<void>(f_1, _1)(i), (global_result == 1L)) );
    BOOST_TEST( (bind<void>(f_2, _1, 2)(i), (global_result == 21L)) );
    BOOST_TEST( (bind<void>(f_3, _1, 2, 3)(i), (global_result == 321L)) );
    BOOST_TEST( (bind<void>(f_4, _1, 2, 3, 4)(i), (global_result == 4321L)) );
    BOOST_TEST( (bind<void>(f_5, _1, 2, 3, 4, 5)(i), (global_result == 54321L)) );
    BOOST_TEST( (bind<void>(f_6, _1, 2, 3, 4, 5, 6)(i), (global_result == 654321L)) );
    BOOST_TEST( (bind<void>(f_7, _1, 2, 3, 4, 5, 6, 7)(i), (global_result == 7654321L)) );
    BOOST_TEST( (bind<void>(f_8, _1, 2, 3, 4, 5, 6, 7, 8)(i), (global_result == 87654321L)) );
    BOOST_TEST( (bind<void>(f_9, _1, 2, 3, 4, 5, 6, 7, 8, 9)(i), (global_result == 987654321L)) );
}

//

struct Y
{
    short operator()(short & r) const { return global_result = ++r; }
    int operator()(int a, int b) const { return global_result = a + 10 * b; }
    long operator() (long a, long b, long c) const { return global_result = a + 10 * b + 100 * c; }
    void operator() (long a, long b, long c, long d) const { global_result = a + 10 * b + 100 * c + 1000 * d; }
};

void function_object_test()
{
    using namespace boost;

    short i(6);

    int const k = 3;

    BOOST_TEST( (bind<void>(Y(), ref(i))(), (global_result == 7)) );
    BOOST_TEST( (bind<void>(Y(), ref(i))(), (global_result == 8)) );
    BOOST_TEST( (bind<void>(Y(), i, _1)(k), (global_result == 38)) );
    BOOST_TEST( (bind<void>(Y(), i, _1, 9)(k), (global_result == 938)) );
    BOOST_TEST( (bind<void>(Y(), i, _1, 9, 4)(k), (global_result == 4938)) );
}

int main()
{
    function_test();
    function_object_test();

    return boost::report_errors();
}
