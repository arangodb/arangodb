
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/detail/config.hpp>

#if BOOST_MP11_MSVC
# pragma warning( disable: 4503 ) // decorated name length exceeded
# pragma warning( disable: 4307 ) // '*': integral constant overflow
# pragma warning( disable: 4244 ) // conversion from size_t to uint32_t
# pragma warning( disable: 4267 ) // conversion from size_t to uint32_t
#endif

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test.hpp>
#include <tuple>
#include <cstdint>

#if defined( BOOST_MP11_HAS_CXX14_CONSTEXPR )
# define CONSTEXPR14 constexpr
#else
# define CONSTEXPR14
#endif

struct F
{
    int s;

    CONSTEXPR14 void operator()( int ) { s = s * 10 + 1; }
    CONSTEXPR14 void operator()( short ) { s = s * 10 + 2; }
    CONSTEXPR14 void operator()( char ) { s = s * 10 + 3; }
};

struct G
{
    std::uint32_t s;
    CONSTEXPR14 void operator()( std::uint32_t i ) { s = s * 3 + i; }
};

using boost::mp11::mp_list;
using boost::mp11::mp_for_each;
using boost::mp11::mp_iota_c;

int main()
{
    BOOST_TEST_EQ( (mp_for_each<mp_list<>>( 11 )), 11 );
    BOOST_TEST_EQ( (mp_for_each<mp_list<int>>( F{0} ).s), 1 );
    BOOST_TEST_EQ( (mp_for_each<mp_list<int, short>>( F{0} ).s), 12 );
    BOOST_TEST_EQ( (mp_for_each<mp_list<int, short, char>>( F{0} ).s), 123 );

    BOOST_TEST_EQ( (mp_for_each<std::tuple<>>( 11 )), 11 );
    BOOST_TEST_EQ( (mp_for_each<std::tuple<int>>( F{0} ).s), 1 );
    BOOST_TEST_EQ( (mp_for_each<std::tuple<int, short>>( F{0} ).s), 12 );
    BOOST_TEST_EQ( (mp_for_each<std::tuple<int, short, char>>( F{0} ).s), 123 );

    BOOST_TEST_EQ( (mp_for_each<std::pair<int, short>>( F{0} ).s), 12 );

#if BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
#else

    using L = mp_iota_c<1089>;
    std::uint32_t const R = 598075296;

    BOOST_TEST_EQ( (mp_for_each<L>( G{0} ).s), R );

    {
        G g{0};
        mp_for_each<L>( g );
        BOOST_TEST_EQ( g.s, R );
    }

#endif

#if defined( BOOST_MP11_NO_CONSTEXPR ) || ( !defined(_MSC_VER) && !defined( __GLIBCXX__ ) && __cplusplus < 201400L )
#else

    static_assert( mp_for_each<mp_list<>>( 11 ) == 11, "mp_for_each<mp_list<>>( 11 ) == 11" );
    static_assert( mp_for_each<std::tuple<>>( 12 ) == 12, "mp_for_each<std::tuple<>>( 12 ) == 12" );

#endif

#if defined( BOOST_MP11_HAS_CXX14_CONSTEXPR )

    constexpr auto r1 = mp_for_each<mp_list<int, short, char>>( F{0} );
    static_assert( r1.s == 123, "r1.s == 123" );

    constexpr auto r2 = mp_for_each<std::tuple<int, short, char>>( F{0} );
    static_assert( r2.s == 123, "r2.s == 123" );

    constexpr auto r3 = mp_for_each<std::pair<int, short>>( F{0} );
    static_assert( r3.s == 12, "r3.s == 12" );

    constexpr auto r4 = mp_for_each<L>( G{0} );
    static_assert( r4.s == R, "r4.s == R" );

#endif

    return boost::report_errors();
}
