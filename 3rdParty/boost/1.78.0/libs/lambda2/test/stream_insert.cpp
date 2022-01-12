// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>
#include <string>

template<class T> std::string to_string_1( T const& t )
{
    using namespace boost::lambda2;

    std::ostringstream os;

    ( std::ref( os ) << _1 )( t );

    return os.str();
}

template<class T> std::string to_string_2( T const& t )
{
    using namespace boost::lambda2;

    std::ostringstream os;

    ( os << _1 )( t );

    return os.str();
}

int main()
{
    BOOST_TEST_EQ( to_string_1( 123 ), "123" );
    BOOST_TEST_EQ( to_string_2( 456 ), "456" );

    return boost::report_errors();
}
