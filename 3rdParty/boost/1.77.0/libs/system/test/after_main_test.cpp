
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/quick_exit.hpp>
#include <cerrno>

using namespace boost::system;

struct Z
{
    ~Z()
    {
        BOOST_TEST_CSTR_EQ( generic_category().name(), "generic" );
        BOOST_TEST_CSTR_EQ( system_category().name(), "system" );

        boost::quick_exit( boost::report_errors() );
    }
};

static Z z;

static error_code e1( 1, system_category() );
static error_code e2( ENOENT, generic_category() );

int main()
{
}
