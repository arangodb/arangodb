
// Copyright 2018 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

// See library home page at http://www.boost.org/libs/system

// Avoid spurious VC++ warnings
# define _CRT_SECURE_NO_WARNINGS

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>
#include <cstdio>

//

#if defined(BOOST_WINDOWS_API) && defined(BOOST_SYSTEM_USE_UTF8)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE( "Skipping test due to BOOST_WINDOWS_API && BOOST_SYSTEM_USE_UTF8" )

int main() {}

#else

#if defined(BOOST_WINDOWS_API)

#include <windows.h>

std::string sys_strerror( int ev )
{
    void * lpMsgBuf = 0;

    DWORD retval = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        ev,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR) &lpMsgBuf,
        0,
        NULL
    );

    struct local_free
    {
        void * p_;

        ~local_free()
        {
            LocalFree( p_ );
        }
    };

    local_free lf_ = { lpMsgBuf };
    (void)lf_;

    if( retval == 0 )
    {
        char buffer[ 38 ];

        std::sprintf( buffer, "Unknown error (%d)", ev );
        return buffer;
    }

    std::string str( static_cast<char const*>( lpMsgBuf ) );

    while( !str.empty() && (str[str.size()-1] == '\n' || str[str.size()-1] == '\r') )
    {
        str.erase( str.size()-1 );
    }

    if( !str.empty() && str[str.size()-1] == '.' )
    {
        str.erase( str.size()-1 );
    }

    return str;
}

#else

std::string sys_strerror( int ev )
{
    return std::strerror( ev );
}

#endif

//

namespace sys = boost::system;

static void test_message( sys::error_category const & cat, int ev )
{
    BOOST_TEST_EQ( cat.message( ev ), sys_strerror( ev ) );

    char buffer[ 4096 ]; // yes, really
    BOOST_TEST_CSTR_EQ( cat.message( ev, buffer, sizeof( buffer ) ), sys_strerror( ev ).c_str() );
}

int main()
{
    sys::error_category const & cat = sys::system_category();

    // message

    for( int i = -2; i < 16000; ++i )
    {
        test_message( cat, i );
    }

    return boost::report_errors();
}

#endif // #if defined(BOOST_WINDOWS_API) && defined(BOOST_SYSTEM_USE_UTF8)
