
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#if defined(_MSC_VER)
#pragma warning( disable: 4244 ) // 'initializing': conversion from 'int' to 'char', possible loss of data
#endif

#include <boost/mp11/tuple.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <tuple>
#include <memory>
#include <utility>
#include <array>

struct T1
{
    int x, y, z;

    T1( int x = 0, int y = 0, int z = 0 ): x(x), y(y), z(z) {}
};

struct T2
{
    std::unique_ptr<int> x, y, z;

    T2( std::unique_ptr<int> x, std::unique_ptr<int> y, std::unique_ptr<int> z ): x(std::move(x)), y(std::move(y)), z(std::move(z)) {}

#if BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )

    T2( T2&& r ): x( std::move(r.x) ), y( std::move(r.y) ), z( std::move(r.z) ) {}

#endif
};

int main()
{
    using boost::mp11::construct_from_tuple;

    {
        std::tuple<int, short, char> tp{ 1, 2, 3 };

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }
    }

    {
        std::tuple<int, short, char> const tp{ 1, 2, 3 };

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }
    }

#if defined( __clang_major__ ) && __clang_major__ == 3 && __clang_minor__ < 8
#else

    {
        std::tuple<std::unique_ptr<int>, std::unique_ptr<int>, std::unique_ptr<int>> tp{ std::unique_ptr<int>(new int(1)), std::unique_ptr<int>(new int(2)), std::unique_ptr<int>(new int(3)) };

            T2 t2 = construct_from_tuple<T2>( std::move(tp) );

            BOOST_TEST_EQ( *t2.x, 1 );
            BOOST_TEST_EQ( *t2.y, 2 );
            BOOST_TEST_EQ( *t2.z, 3 );
    }

#endif

    {
        std::pair<int, short> tp{ 1, 2 };

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 0 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 0 );
        }
    }

    {
        std::pair<int, short> const tp{ 1, 2 };

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 0 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 0 );
        }
    }

    {
        std::array<int, 3> tp{{ 1, 2, 3 }};

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }
    }

    {
        std::array<int, 3> const tp{{ 1, 2, 3 }};

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 1 );
            BOOST_TEST_EQ( t1.y, 2 );
            BOOST_TEST_EQ( t1.z, 3 );
        }
    }

    {
        std::tuple<> tp;

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 0 );
            BOOST_TEST_EQ( t1.y, 0 );
            BOOST_TEST_EQ( t1.z, 0 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 0 );
            BOOST_TEST_EQ( t1.y, 0 );
            BOOST_TEST_EQ( t1.z, 0 );
        }
    }

    {
        std::array<int, 0> tp;

        {
            T1 t1 = construct_from_tuple<T1>( tp );

            BOOST_TEST_EQ( t1.x, 0 );
            BOOST_TEST_EQ( t1.y, 0 );
            BOOST_TEST_EQ( t1.z, 0 );
        }

        {
            T1 t1 = construct_from_tuple<T1>( std::move(tp) );

            BOOST_TEST_EQ( t1.x, 0 );
            BOOST_TEST_EQ( t1.y, 0 );
            BOOST_TEST_EQ( t1.z, 0 );
        }
    }

    return boost::report_errors();
}
