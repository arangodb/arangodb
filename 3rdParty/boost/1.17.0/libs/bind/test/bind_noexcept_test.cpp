//
// bind_noexcept_test.cpp
//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/bind.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

#if defined(BOOST_NO_CXX11_NOEXCEPT)

int main()
{
}

#else

//

long f_0() noexcept
{
    return 17041L;
}

long f_1(long a) noexcept
{
    return a;
}

long f_2(long a, long b) noexcept
{
    return a + 10 * b;
}

long f_3(long a, long b, long c) noexcept
{
    return a + 10 * b + 100 * c;
}

long f_4(long a, long b, long c, long d) noexcept
{
    return a + 10 * b + 100 * c + 1000 * d;
}

long f_5(long a, long b, long c, long d, long e) noexcept
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e;
}

long f_6(long a, long b, long c, long d, long e, long f) noexcept
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f;
}

long f_7(long a, long b, long c, long d, long e, long f, long g) noexcept
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g;
}

long f_8(long a, long b, long c, long d, long e, long f, long g, long h) noexcept
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g + 10000000 * h;
}

long f_9(long a, long b, long c, long d, long e, long f, long g, long h, long i) noexcept
{
    return a + 10 * b + 100 * c + 1000 * d + 10000 * e + 100000 * f + 1000000 * g + 10000000 * h + 100000000 * i;
}

void function_test()
{
    int const i = 1;

    BOOST_TEST( boost::bind(f_0)(i) == 17041L );
    BOOST_TEST( boost::bind(f_1, _1)(i) == 1L );
    BOOST_TEST( boost::bind(f_2, _1, 2)(i) == 21L );
    BOOST_TEST( boost::bind(f_3, _1, 2, 3)(i) == 321L );
    BOOST_TEST( boost::bind(f_4, _1, 2, 3, 4)(i) == 4321L );
    BOOST_TEST( boost::bind(f_5, _1, 2, 3, 4, 5)(i) == 54321L );
    BOOST_TEST( boost::bind(f_6, _1, 2, 3, 4, 5, 6)(i) == 654321L );
    BOOST_TEST( boost::bind(f_7, _1, 2, 3, 4, 5, 6, 7)(i) == 7654321L );
    BOOST_TEST( boost::bind(f_8, _1, 2, 3, 4, 5, 6, 7, 8)(i) == 87654321L );
    BOOST_TEST( boost::bind(f_9, _1, 2, 3, 4, 5, 6, 7, 8, 9)(i) == 987654321L );
}

int main()
{
    function_test();
    return boost::report_errors();
}

#endif
