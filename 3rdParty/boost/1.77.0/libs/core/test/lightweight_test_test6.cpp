// Test BOOST_TEST_EQ with character types
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

int main()
{
    BOOST_TEST_EQ( 'A', 'A' );
    BOOST_TEST_EQ( (char)1, (char)1 );
    BOOST_TEST_EQ( (unsigned char)1, (unsigned char)1 );
    BOOST_TEST_EQ( (signed char)-1, (signed char)-1 );

    BOOST_TEST_EQ( L'A', L'A' );
    BOOST_TEST_EQ( (wchar_t)1, (wchar_t)1 );

#if !defined(BOOST_NO_CXX11_CHAR16_T)

    BOOST_TEST_EQ( (char16_t)1, (char16_t)1 );

#endif

#if !defined(BOOST_NO_CXX11_CHAR32_T)

    BOOST_TEST_EQ( (char32_t)1, (char32_t)1 );

#endif

    return boost::report_errors();
}
