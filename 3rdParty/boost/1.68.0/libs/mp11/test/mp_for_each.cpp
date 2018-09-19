
//  Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <tuple>

#if !defined( BOOST_NO_CXX14_CONSTEXPR )
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

using boost::mp11::mp_list;
using boost::mp11::mp_for_each;

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

#if defined( BOOST_NO_CXX11_CONSTEXPR ) || ( !defined( __GLIBCXX__ ) && __cplusplus < 201400L )
#else

    static_assert( mp_for_each<mp_list<>>( 11 ) == 11, "mp_for_each<mp_list<>>( 11 ) == 11" );
    static_assert( mp_for_each<std::tuple<>>( 12 ) == 12, "mp_for_each<std::tuple<>>( 12 ) == 12" );

#endif

#if !defined( BOOST_NO_CXX14_CONSTEXPR ) && !BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

    constexpr auto r1 = mp_for_each<mp_list<int, short, char>>( F{0} );
    static_assert( r1.s == 123, "r1.s == 123" );

    constexpr auto r2 = mp_for_each<std::tuple<int, short, char>>( F{0} );
    static_assert( r2.s == 123, "r2.s == 123" );

    constexpr auto r3 = mp_for_each<std::pair<int, short>>( F{0} );
    static_assert( r3.s == 12, "r3.s == 12" );

#endif

    return boost::report_errors();
}
