
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::system;

namespace lib1
{

error_code get_system_code();
error_code get_generic_code();

} // namespace lib1

namespace lib2
{

error_code get_system_code();
error_code get_generic_code();

} // namespace lib2

int main()
{
    {
        error_code e1 = lib1::get_system_code();
        error_code e2 = lib2::get_system_code();

        BOOST_TEST_EQ( e1, e2 );

        BOOST_TEST_EQ( hash_value( e1 ), hash_value( e2 ) );
    }

    {
        error_code e1 = lib1::get_generic_code();
        error_code e2 = lib2::get_generic_code();

        BOOST_TEST_EQ( e1, e2 );

        BOOST_TEST_EQ( hash_value( e1 ), hash_value( e2 ) );
    }

    return boost::report_errors();
}
