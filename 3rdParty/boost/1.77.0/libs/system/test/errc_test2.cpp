// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/errc.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    BOOST_TEST_EQ( make_error_code( boost::system::errc::success ).value(), 0 );
    BOOST_TEST_EQ( make_error_condition( boost::system::errc::success ).value(), 0 );

    BOOST_TEST_EQ( make_error_code( boost::system::errc::no_such_file_or_directory ).value(), ENOENT );
    BOOST_TEST_EQ( make_error_condition( boost::system::errc::no_such_file_or_directory ).value(), ENOENT );

    BOOST_TEST_EQ( make_error_code( boost::system::errc::address_family_not_supported ).value(), EAFNOSUPPORT );
    BOOST_TEST_EQ( make_error_condition( boost::system::errc::address_family_not_supported ).value(), EAFNOSUPPORT );

    return boost::report_errors();
}
