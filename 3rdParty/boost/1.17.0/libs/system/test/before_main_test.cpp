
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cerrno>

using namespace boost::system;

static error_code e1( 1, system_category() );
static std::string m1 = e1.message();

static error_code e2( ENOENT, generic_category() );
static std::string m2 = e2.message();

int main()
{
    error_code e1_( 1, system_category() );

    BOOST_TEST_EQ( e1, e1_ );
    BOOST_TEST_EQ( m1, e1_.message() );

    error_code e2_( ENOENT, generic_category() );

    BOOST_TEST_EQ( e2, e2_ );
    BOOST_TEST_EQ( m2, e2_.message() );

    return boost::report_errors();
}
