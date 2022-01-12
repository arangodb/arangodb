// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

using namespace boost::system;

struct X
{
    int a, b;
};

result<X> fx1()
{
    return {{ EINVAL, generic_category() }};
}

struct Y
{
    std::string v;
};

struct E
{
    int v;
};

result<Y, E> fy1()
{
    return {{ 42 }};
}

int main()
{
    {
        result<X> r = fx1();

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), std::error_code( EINVAL, generic_category() ) );
    }

    {
        result<Y, E> r = fy1();

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v, 42 );
    }

    return boost::report_errors();
}
