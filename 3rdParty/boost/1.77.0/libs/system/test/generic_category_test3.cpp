// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/generic_category.hpp>
#include <boost/system/error_condition.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cerrno>

namespace sys = boost::system;

int main()
{
    sys::error_category const & cat = sys::generic_category();

    // name
    BOOST_TEST_CSTR_EQ( cat.name(), "generic" );

    // default_error_condition
    BOOST_TEST( cat.default_error_condition( 0 ) == sys::error_condition( 0, cat ) );
    BOOST_TEST( cat.default_error_condition( ENOENT ) == sys::error_condition( ENOENT, cat ) );
    BOOST_TEST( cat.default_error_condition( -1 ) == sys::error_condition( -1, cat ) );

    // failed
    BOOST_TEST( !cat.failed( 0 ) );
    BOOST_TEST( cat.failed( ENOENT ) );
    BOOST_TEST( cat.failed( -1 ) );

    return boost::report_errors();
}
