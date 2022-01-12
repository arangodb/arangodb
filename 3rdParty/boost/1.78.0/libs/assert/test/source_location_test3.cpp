// Copyright 2020, 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>

int main()
{
    {
        boost::source_location loc;
        BOOST_TEST_EQ( loc.to_string(), std::string( "(unknown source location)" ) );
    }

    {
        boost::source_location loc;

        std::ostringstream os;
        os << loc;

        BOOST_TEST_EQ( os.str(), std::string( "(unknown source location)" ) );
    }

    {
        boost::source_location loc = BOOST_CURRENT_LOCATION;
        BOOST_TEST_EQ( loc.to_string(), std::string( __FILE__ ) + ":26 in function '" + BOOST_CURRENT_FUNCTION + "'" );
    }

    {
        boost::source_location loc = BOOST_CURRENT_LOCATION;

        std::ostringstream os;
        os << loc;

        BOOST_TEST_EQ( os.str(), std::string( __FILE__ ) + ":31 in function '" + BOOST_CURRENT_FUNCTION + "'" );
    }

    return boost::report_errors();
}
