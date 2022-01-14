// Test BOOST_TEST_NE with character types
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

int main()
{
    BOOST_TEST_NE( 'A', 'A' );
    BOOST_TEST_NE( (char)1, (char)1 );
    BOOST_TEST_NE( (unsigned char)1, (unsigned char)1 );
    BOOST_TEST_NE( (signed char)-1, (signed char)-1 );

    BOOST_TEST_NE( L'A', L'A' );
    BOOST_TEST_NE( (wchar_t)1, (wchar_t)1 );

    int exp = 6;

#if !defined(BOOST_NO_CXX11_CHAR16_T)

    BOOST_TEST_NE( (char16_t)1, (char16_t)1 );
    ++exp;

#endif

#if !defined(BOOST_NO_CXX11_CHAR32_T)

    BOOST_TEST_NE( (char32_t)1, (char32_t)1 );
    ++exp;

#endif

    return boost::report_errors() == exp;
}
