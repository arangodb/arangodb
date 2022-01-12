// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>
#include <string>

using namespace boost::variant2;

template<class T> std::string to_string( T const& t )
{
    std::ostringstream os;

    os << t;

    return os.str();
}

int main()
{
    variant<int, float, std::string> v( 1 );

    BOOST_TEST_EQ( to_string( v ), to_string( 1 ) );

    v = 3.14f;

    BOOST_TEST_EQ( to_string( v ), to_string( 3.14f ) );

    v = "test";

    BOOST_TEST_EQ( to_string( v ), to_string( "test" ) );

    return boost::report_errors();
}
