// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/cygwin_error.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(__CYGWIN__)

BOOST_PRAGMA_MESSAGE( "Skipping test, __CYGWIN__ is not defined" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>

int main()
{
    namespace sys = boost::system;

    sys::error_code ec = sys::cygwin_error::no_package;

    BOOST_TEST_EQ( ec, sys::cygwin_error::no_package );
    BOOST_TEST_EQ( ec, sys::error_code( ENOPKG, sys::system_category() ) );

    return boost::report_errors();
}

#endif
