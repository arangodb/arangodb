// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/linux_error.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(__linux__)

BOOST_PRAGMA_MESSAGE( "Skipping test, __linux__ is not defined" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>

int main()
{
    namespace sys = boost::system;

    sys::error_code ec = sys::linux_error::dot_dot_error;

    BOOST_TEST_EQ( ec, sys::linux_error::dot_dot_error );
    BOOST_TEST_EQ( ec, sys::error_code( EDOTDOT, sys::system_category() ) );

    return boost::report_errors();
}

#endif
