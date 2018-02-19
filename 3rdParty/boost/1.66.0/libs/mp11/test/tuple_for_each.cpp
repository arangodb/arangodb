
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/tuple.hpp>
#include <boost/core/lightweight_test.hpp>
#include <tuple>
#include <memory>
#include <utility>
#include <array>

int main()
{
    using boost::mp11::tuple_for_each;

    {
        std::tuple<int, short, char> tp{ 1, 2, 3 };

        {
            int s = 0;

            tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }

        {
            int s = 0;

            tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }
    }

    {
        std::tuple<int, short, char> const tp{ 1, 2, 3 };

        {
            int s = 0;

            tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }

        {
            int s = 0;

            tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }
    }

#if defined( __clang_major__ ) && __clang_major__ == 3 && __clang_minor__ < 8
#else

    {
        std::tuple<std::unique_ptr<int>, std::unique_ptr<int>, std::unique_ptr<int>> tp{ std::unique_ptr<int>(new int(1)), std::unique_ptr<int>(new int(2)), std::unique_ptr<int>(new int(3)) };

        int s = 0;

        tuple_for_each( std::move(tp), [&]( std::unique_ptr<int> p ){ s = s * 10 + *p; } );

        BOOST_TEST_EQ( s, 123 );
    }

#endif

    {
        std::pair<int, short> tp{ 1, 2 };

        {
            int s = 0;

            tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 12 );
        }

        {
            int s = 0;

            tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 12 );
        }
    }

    {
        std::pair<int, short> const tp{ 1, 2 };

        {
            int s = 0;

            tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 12 );
        }

        {
            int s = 0;

            tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 12 );
        }
    }

    {
        std::array<int, 3> tp{{ 1, 2, 3 }};

        {
            int s = 0;

            tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }

        {
            int s = 0;

            tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }
    }

    {
        std::array<int, 3> const tp{{ 1, 2, 3 }};

        {
            int s = 0;

            tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }

        {
            int s = 0;

            tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

            BOOST_TEST_EQ( s, 123 );
        }
    }

    {
        std::tuple<> tp;

        BOOST_TEST_EQ( tuple_for_each( tp, 11 ), 11 );
        BOOST_TEST_EQ( tuple_for_each( std::move( tp ), 12 ), 12 );
    }

    {
        std::array<int, 0> tp;

        BOOST_TEST_EQ( tuple_for_each( tp, 11 ), 11 );
        BOOST_TEST_EQ( tuple_for_each( std::move( tp ), 12 ), 12 );
    }

    return boost::report_errors();
}
