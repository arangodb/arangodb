
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
};

inline bool operator<( X const&, X const& ) { return false; }
inline bool operator>( X const&, X const& ) { return false; }
inline bool operator<=( X const&, X const& ) { return false; }
inline bool operator>=( X const&, X const& ) { return false; }

#define TEST_EQ( v1, v2 ) \
    BOOST_TEST_NOT( v1 < v2 ); \
    BOOST_TEST_NOT( v1 > v2 ); \
    BOOST_TEST( v1 <= v2 ); \
    BOOST_TEST( v1 >= v2 );

#define TEST_LE( v1, v3 ) \
    BOOST_TEST( v1 < v3 ); \
    BOOST_TEST( v3 > v1 ); \
    BOOST_TEST_NOT( v1 > v3 ); \
    BOOST_TEST_NOT( v3 < v1 ); \
    BOOST_TEST( v1 <= v3 ); \
    BOOST_TEST( v3 >= v1 ); \
    BOOST_TEST_NOT( v1 >= v3 ); \
    BOOST_TEST_NOT( v3 <= v1 );

int main()
{
    {
        variant<int> v1, v2, v3( 1 ), v4( 1 );

        TEST_EQ( v1, v2 )
        TEST_LE( v1, v3 )
        TEST_EQ( v3, v4 )
    }

    {
        variant<int, float> v1, v2, v3( 1 ), v4( 1 ), v5( 3.14f ), v6( 3.14f );

        TEST_EQ( v1, v2 )
        TEST_LE( v1, v3 )
        TEST_EQ( v3, v4 )
        TEST_LE( v1, v5 )
        TEST_LE( v3, v5 )
        TEST_EQ( v5, v6 )
    }

    {
        variant<int, int, float> v1, v2, v3( in_place_index_t<1>{} ), v4( in_place_index_t<1>{} ), v5( 3.14f ), v6( 3.14f );

        TEST_EQ( v1, v2 )
        TEST_LE( v1, v3 )
        TEST_EQ( v3, v4 )
        TEST_LE( v1, v5 )
        TEST_LE( v3, v5 )
        TEST_EQ( v5, v6 )
    }

    {
        variant<X> v1, v2;

        BOOST_TEST_NOT( v1 < v2 );
        BOOST_TEST_NOT( v1 > v2 );
        BOOST_TEST_NOT( v1 <= v2 );
        BOOST_TEST_NOT( v1 >= v2 );
    }

    return boost::report_errors();
}
