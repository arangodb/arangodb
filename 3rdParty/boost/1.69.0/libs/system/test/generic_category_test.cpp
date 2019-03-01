
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

//

namespace sys = boost::system;

int main()
{
    sys::error_category const & cat = sys::generic_category();

    // message

    for( int i = -2; i < 1024; ++i )
    {
        {
            BOOST_TEST_CSTR_EQ( cat.message( i ).c_str(), std::strerror( i ) );
        }

        {
            char buffer[ 256 ];
            BOOST_TEST_CSTR_EQ( cat.message( i, buffer, sizeof( buffer ) ), std::strerror( i ) );
        }
    }

    return boost::report_errors();
}
