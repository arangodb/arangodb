
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
    BOOST_TEST_EQ( lib1::get_system_code(), lib2::get_system_code() );
    BOOST_TEST_EQ( lib1::get_generic_code(), lib2::get_generic_code() );

    return boost::report_errors();
}
