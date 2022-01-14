
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4244 ) // conversion from float to int, possible loss of data
#endif

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::variant2;

struct F1
{
    template<class T1> T1 operator()( T1 t1 ) const
    {
        return t1;
    }
};

struct F2
{
    template<class T1, class T2> auto operator()( T1 t1, T2 t2 ) const -> decltype( t1 + t2 )
    {
        return t1 + t2;
    }
};

struct F3
{
    template<class T1, class T2, class T3> auto operator()( T1 t1, T2 t2, T3 t3 ) const -> decltype( t1 + t2 + t3 )
    {
        return t1 + t2 + t3;
    }
};

struct F4
{
    template<class T1, class T2, class T3, class T4> auto operator()( T1 t1, T2 t2, T3 t3, T4 t4 ) const -> decltype( t1 + t2 + t3 + t4 )
    {
        return t1 + t2 + t3 + t4;
    }
};

int main()
{
    {
        BOOST_TEST_EQ( (visit<int>( []{ return 3.14f; } )), 3 );
    }

    {
        variant<int, float> v( 1 );

        BOOST_TEST_EQ( visit<int>( F1(), v ), 1 );
        BOOST_TEST_EQ( visit<float>( F1(), v ), 1.0f );
    }

    {
        variant<int, float> const v( 3.14f );

        BOOST_TEST_EQ( visit<int>( F1(), v ), 3 );
        BOOST_TEST_EQ( visit<float>( F1(), v ), 3.14f );
    }

    {
        variant<int, float> v1( 1 );
        variant<int, float> const v2( 3.14f );

        BOOST_TEST_EQ( visit<int>( F2(), v1, v2 ), 4 );
        BOOST_TEST_EQ( visit<float>( F2(), v1, v2 ), 1 + 3.14f );
    }

    {
        variant<int, float, double> v1( 1 );
        variant<int, float, double> const v2( 3.14f );
        variant<int, float, double> v3( 6.28 );

        BOOST_TEST_EQ( visit<int>( F3(), v1, v2, v3 ), 10 );
        BOOST_TEST_EQ( visit<float>( F3(), v1, v2, v3 ), static_cast<float>( 1 + 3.14f + 6.28 ) );
    }

    {
        variant<int, float, double, char> v1( 1 );
        variant<int, float, double, char> const v2( 3.14f );
        variant<int, float, double, char> v3( 6.28 );
        variant<int, float, double, char> const v4( 'A' );

        BOOST_TEST_EQ( visit<int>( F4(), v1, v2, v3, v4 ), 10 + 'A' );
        BOOST_TEST_EQ( visit<float>( F4(), v1, v2, v3, v4 ), static_cast<float>( 1 + 3.14f + 6.28 + 'A' ) );
    }

    return boost::report_errors();
}
