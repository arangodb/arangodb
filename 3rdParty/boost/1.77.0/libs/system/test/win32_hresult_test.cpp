
// Copyright 2018 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

// See library home page at http://www.boost.org/libs/system

#include <boost/system/error_code.hpp>
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

    HRESULT r = HRESULT_FROM_WIN32( ERROR_ACCESS_DENIED );

    sys::error_code ec( r, sys::system_category() );
    sys::error_condition en = make_error_condition( sys::errc::permission_denied );

    BOOST_TEST( ec == en );
    BOOST_TEST( ec.default_error_condition() == en );

    BOOST_TEST_EQ( ec.default_error_condition().value(), en.value() );

    return boost::report_errors();
}

#endif
