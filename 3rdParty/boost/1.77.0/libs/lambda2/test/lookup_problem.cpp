// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>

namespace N
{
    struct Irrelevant {};
    void operator+( Irrelevant, Irrelevant );

    int plus_one( int i )
    {
        using namespace boost::lambda2;
        return (_1 + 1)( i );
    }
}

int main()
{
    BOOST_TEST_EQ( N::plus_one( 4 ), 5 );
    return boost::report_errors();
}
