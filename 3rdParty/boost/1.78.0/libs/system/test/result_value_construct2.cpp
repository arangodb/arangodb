// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <vector>
#include <string>

using namespace boost::system;

struct X
{
    int a;
};

struct Y
{
    int a, b;
};

struct E
{
    std::string v;
};

result<X> fx0()
{
    return {};
}

result<X> fx1()
{
    return {{ 1 }};
}

result<Y> fy0()
{
    return {};
}

result<Y> fy2()
{
    return {{ 1, 2 }};
}

result<X, E> fxe0()
{
    return {};
}

result<X, E> fxe1()
{
    return {{ 1 }};
}

result<std::vector<int>> fv0()
{
    return {};
}

result<std::vector<int>> fv1()
{
    return {{ 1 }};
}

result<std::vector<int>> fv2()
{
    return {{ 1, 2 }};
}

int main()
{
    {
        result<X> r = fx0();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->a, 0 );
    }

    {
        result<X> r = fx1();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->a, 1 );
    }

    {
        result<Y> r = fy0();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->a, 0 );
        BOOST_TEST_EQ( r->b, 0 );
    }

    {
        result<Y> r = fy2();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->a, 1 );
        BOOST_TEST_EQ( r->b, 2 );
    }

    {
        result<X, E> r = fxe0();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->a, 0 );
    }

    {
        result<X, E> r = fxe1();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->a, 1 );
    }

    {
        result<std::vector<int>> r = fv0();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->size(), 0 );
    }

    {
        result<std::vector<int>> r = fv1();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->size(), 1 );
        BOOST_TEST_EQ( r->at(0), 1 );
    }

    {
        result<std::vector<int>> r = fv2();

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r->size(), 2 );
        BOOST_TEST_EQ( r->at(0), 1 );
        BOOST_TEST_EQ( r->at(1), 2 );
    }

    return boost::report_errors();
}
