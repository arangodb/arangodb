// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/windows_error.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(BOOST_WINDOWS_API)

BOOST_PRAGMA_MESSAGE( "Skipping test, BOOST_WINDOWS_API is not defined" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>
#include <windows.h>

int main()
{
    namespace sys = boost::system;

    sys::error_code ec = sys::windows_error::invalid_function;

    BOOST_TEST_EQ( ec, sys::windows_error::invalid_function );
    BOOST_TEST_EQ( ec, sys::error_code( ERROR_INVALID_FUNCTION, sys::system_category() ) );
    BOOST_TEST( ec == make_error_condition( sys::errc::function_not_supported ) );

    return boost::report_errors();
}

#endif
