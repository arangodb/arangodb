
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <utility>
#include <string>

using namespace boost::variant2;

struct X
{
    X() = default;
    X( in_place_index_t<0> ) = delete;
};

int main()
{
    {
        variant<int> v( in_place_index_t<0>{} );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<X> v( in_place_index_t<0>{} );

        BOOST_TEST_EQ( v.index(), 0 );
    }

    {
        variant<int> v( in_place_index_t<0>{}, 1 );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 1 );
    }

    {
        variant<int, float> v( in_place_index_t<0>{} );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 0 );
    }

    {
        variant<int, float> v( in_place_index_t<0>{}, 1 );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 1 );
    }

    {
        variant<int, float> v( in_place_index_t<1>{} );

        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 0 );
    }

    {
        variant<int, float> v( in_place_index_t<1>{}, 3.14f );

        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 3.14f );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<0>{}, 1 );

        BOOST_TEST_EQ( v.index(), 0 );
        BOOST_TEST_EQ( get<0>(v), 1 );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<1>{}, 1 );

        BOOST_TEST_EQ( v.index(), 1 );
        BOOST_TEST_EQ( get<1>(v), 1 );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<2>{}, 3.14f );

        BOOST_TEST_EQ( v.index(), 2 );
        BOOST_TEST_EQ( get<2>(v), 3.14f );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<3>{}, 3.14f );

        BOOST_TEST_EQ( v.index(), 3 );
        BOOST_TEST_EQ( get<3>(v), 3.14f );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<4>{}, "text" );

        BOOST_TEST_EQ( v.index(), 4 );
        BOOST_TEST_EQ( get<4>(v), std::string("text") );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<5>{}, "text" );

        BOOST_TEST_EQ( v.index(), 5 );
        BOOST_TEST_EQ( get<5>(v), std::string("text") );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<5>{}, 4, 'a' );

        BOOST_TEST_EQ( v.index(), 5 );
        BOOST_TEST_EQ( get<5>(v), std::string( 4, 'a' ) );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<4>{}, { 'a', 'b', 'c' } );

        BOOST_TEST_EQ( v.index(), 4 );
        BOOST_TEST_EQ( get<4>(v), (std::string{ 'a', 'b', 'c' }) );
    }

    {
        variant<int, int, float, float, std::string, std::string> v( in_place_index_t<5>{}, { 'a', 'b', 'c' }, std::allocator<char>() );

        BOOST_TEST_EQ( v.index(), 5 );
        BOOST_TEST_EQ( get<5>(v), (std::string{ 'a', 'b', 'c' }) );
    }

    return boost::report_errors();
}
