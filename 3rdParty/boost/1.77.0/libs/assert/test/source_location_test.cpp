// Copyright 2019 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    {
        boost::source_location loc;

        BOOST_TEST_CSTR_EQ( loc.file_name(), "(unknown)" );
        BOOST_TEST_CSTR_EQ( loc.function_name(), "(unknown)" );
        BOOST_TEST_EQ( loc.line(), 0 );
        BOOST_TEST_EQ( loc.column(), 0 );
    }

    {
        boost::source_location loc = BOOST_CURRENT_LOCATION;


        BOOST_TEST_CSTR_EQ( loc.file_name(), __FILE__ );
        BOOST_TEST_CSTR_EQ( loc.function_name(), BOOST_CURRENT_FUNCTION );
        BOOST_TEST_EQ( loc.line(), 20 );
        BOOST_TEST_EQ( loc.column(), 0 );
    }

    return boost::report_errors();
}
