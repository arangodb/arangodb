
// Copyright 2017, 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
using namespace boost::variant2;

#if !defined(BOOST_MP11_HAS_CXX14_CONSTEXPR)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping constexpr op<, op<= test because BOOST_MP11_HAS_CXX14_CONSTEXPR is not defined")

int main() {}

#else

struct X
{
};

inline constexpr bool operator<( X const&, X const& ) { return false; }
inline constexpr bool operator>( X const&, X const& ) { return false; }
inline constexpr bool operator<=( X const&, X const& ) { return false; }
inline constexpr bool operator>=( X const&, X const& ) { return false; }

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#define TEST_EQ( v1, v2 ) \
    STATIC_ASSERT( !(v1 < v2) ); \
    STATIC_ASSERT( !(v1 > v2) ); \
    STATIC_ASSERT( v1 <= v2 ); \
    STATIC_ASSERT( v1 >= v2 );

#define TEST_LE( v1, v3 ) \
    STATIC_ASSERT( v1 < v3 ); \
    STATIC_ASSERT( v3 > v1 ); \
    STATIC_ASSERT( !(v1 > v3) ); \
    STATIC_ASSERT( !(v3 < v1) ); \
    STATIC_ASSERT( v1 <= v3 ); \
    STATIC_ASSERT( v3 >= v1 ); \
    STATIC_ASSERT( !(v1 >= v3) ); \
    STATIC_ASSERT( !(v3 <= v1) );

int main()
{
    {
        constexpr variant<int> v1, v2, v3( 1 ), v4( 1 );

        TEST_EQ( v1, v2 )
        TEST_LE( v1, v3 )
        TEST_EQ( v3, v4 )
    }

    {
        constexpr variant<int, float> v1, v2, v3( 1 ), v4( 1 ), v5( 3.14f ), v6( 3.14f );

        TEST_EQ( v1, v2 )
        TEST_LE( v1, v3 )
        TEST_EQ( v3, v4 )
        TEST_LE( v1, v5 )
        TEST_LE( v3, v5 )
        TEST_EQ( v5, v6 )
    }

    {
        constexpr variant<int, int, float> v1, v2, v3( in_place_index_t<1>{} ), v4( in_place_index_t<1>{} ), v5( 3.14f ), v6( 3.14f );

        TEST_EQ( v1, v2 )
        TEST_LE( v1, v3 )
        TEST_EQ( v3, v4 )
        TEST_LE( v1, v5 )
        TEST_LE( v3, v5 )
        TEST_EQ( v5, v6 )
    }

    {
        constexpr variant<X> v1, v2;

        STATIC_ASSERT( !(v1 < v2) );
        STATIC_ASSERT( !(v1 > v2) );
        STATIC_ASSERT( !(v1 <= v2) );
        STATIC_ASSERT( !(v1 >= v2) );
    }

    {
        constexpr variant<monostate> v1, v2;

        STATIC_ASSERT( !(v1 < v2) );
        STATIC_ASSERT( !(v1 > v2) );
        STATIC_ASSERT( v1 <= v2 );
        STATIC_ASSERT( v1 >= v2 );
    }
}

#endif
