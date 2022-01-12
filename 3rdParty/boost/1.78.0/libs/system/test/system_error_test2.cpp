// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/system_error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cerrno>

namespace sys = boost::system;

int main()
{
    {
        sys::error_code ec( 5, sys::generic_category() );
        sys::system_error x1( ec );
        (void)x1;
    }

    {
        sys::error_code ec( 5, sys::system_category() );
        sys::system_error x1( ec );
        (void)x1;
    }

    {
        sys::system_error x1( make_error_code( sys::errc::invalid_argument ) );
        (void)x1;
    }

    {
        sys::system_error x1( 5, sys::generic_category() );
        (void)x1;
    }

    {
        sys::system_error x1( 5, sys::system_category() );
        (void)x1;
    }

    return boost::report_errors();
}
