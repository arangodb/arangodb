// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>

using namespace boost::system;

struct X
{
};

int main()
{
    {
        result<int> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.value(), 0 );
    }

    {
        result<X> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );
    }

    {
        result<void> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );
    }

    return boost::report_errors();
}
