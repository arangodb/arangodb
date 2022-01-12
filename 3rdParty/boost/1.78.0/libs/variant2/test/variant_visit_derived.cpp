// Copyright 2017, 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4244 ) // conversion from float to int, possible loss of data
#endif

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <utility>

struct X: boost::variant2::variant<int, float>
{
#if BOOST_WORKAROUND( BOOST_MSVC, < 1940 )

    template<class T> explicit X( T&& t ): variant( std::forward<T>( t ) ) {};

#else

    using variant::variant;

#endif
};

template<class... T> struct Y: boost::variant2::variant<T...>
{
    using boost::variant2::variant<T...>::variant;
};

int main()
{
    {
        X v1( 1 );
        X const v2( 3.14f );

        BOOST_TEST_EQ( (visit( []( int x1, float x2 ){ return (int)(x1 * 1000) + (int)(x2 * 100); }, v1, v2 )), 1314 );

        visit( []( int x1, float x2 ){ BOOST_TEST_EQ( x1, 1 ); BOOST_TEST_EQ( x2, 3.14f ); }, v1, v2 );
        visit( []( int x1, float x2 ){ BOOST_TEST_EQ( x1, 1 ); BOOST_TEST_EQ( x2, 3.14f ); }, std::move(v1), std::move(v2) );
    }

    {
        Y<int, float> v1( 1 );
        Y<int, float> const v2( 3.14f );

        BOOST_TEST_EQ( (visit( []( int x1, float x2 ){ return (int)(x1 * 1000) + (int)(x2 * 100); }, v1, v2 )), 1314 );

        visit( []( int x1, float x2 ){ BOOST_TEST_EQ( x1, 1 ); BOOST_TEST_EQ( x2, 3.14f ); }, v1, v2 );
        visit( []( int x1, float x2 ){ BOOST_TEST_EQ( x1, 1 ); BOOST_TEST_EQ( x2, 3.14f ); }, std::move(v1), std::move(v2) );
    }

    return boost::report_errors();
}
