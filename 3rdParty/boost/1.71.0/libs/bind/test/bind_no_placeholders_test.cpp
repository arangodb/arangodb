//
//  bind_no_placeholders_test.cpp - test for BOOST_BIND_NO_PLACEHOLDERS
//
//  Copyright (c) 2001, 2002 Peter Dimov and Multi Media Ltd.
//  Copyright (c) 2001 David Abrahams
//  Copyright (c) 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/bind.hpp>
#include <boost/core/lightweight_test.hpp>

//

long f_0()
{
    return 17041L;
}

long f_1(long a)
{
    return a;
}

long f_2(long a, long b)
{
    return a + 10 * b;
}

long f_3(long a, long b, long c)
{
    return a + 10 * b + 100 * c;
}

long f_4(long a, long b, long c, long d)
{
    return a + 10 * b + 100 * c + 1000 * d;
}

long f_5(long a, long b, long c, long d, long e)
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e;
}

long f_6(long a, long b, long c, long d, long e, long f)
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f;
}

long f_7(long a, long b, long c, long d, long e, long f, long g)
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g;
}

long f_8(long a, long b, long c, long d, long e, long f, long g, long h)
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g + 10000000 * h;
}

long f_9(long a, long b, long c, long d, long e, long f, long g, long h, long i)
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g + 10000000 * h + 100000000 * i;
}

void function_test()
{
    using namespace boost;

    arg<1> _1;
    arg<2> _2;
    arg<3> _3;
    arg<4> _4;
    arg<5> _5;
    arg<6> _6;
    arg<7> _7;
    arg<8> _8;
    arg<9> _9;

    BOOST_TEST( bind(f_0)() == 17041L );
    BOOST_TEST( bind(f_1, _1)(1) == 1L );
    BOOST_TEST( bind(f_2, _1, _2)(1, 2) == 21L );
    BOOST_TEST( bind(f_3, _1, _2, _3)(1, 2, 3) == 321L );
    BOOST_TEST( bind(f_4, _1, _2, _3, _4)(1, 2, 3, 4) == 4321L );
    BOOST_TEST( bind(f_5, _1, _2, _3, _4, _5)(1, 2, 3, 4, 5) == 54321L );
    BOOST_TEST( bind(f_6, _1, _2, _3, _4, _5, _6)(1, 2, 3, 4, 5, 6) == 654321L );
    BOOST_TEST( bind(f_7, _1, _2, _3, _4, _5, _6, _7)(1, 2, 3, 4, 5, 6, 7) == 7654321L );
    BOOST_TEST( bind(f_8, _1, _2, _3, _4, _5, _6, _7, _8)(1, 2, 3, 4, 5, 6, 7, 8) == 87654321L );
    BOOST_TEST( bind(f_9, _1, _2, _3, _4, _5, _6, _7, _8, _9)(1, 2, 3, 4, 5, 6, 7, 8, 9) == 987654321L );
}

int main()
{
    function_test();
    return boost::report_errors();
}
