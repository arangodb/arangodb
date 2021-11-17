
// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4702 ) // unreachable code
#endif

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <stdexcept>

using namespace boost::variant2;

struct X
{
    static int instances;

    X()
    {
        ++instances;
    }

    X( X const& )
    {
        throw std::runtime_error( "X(X const&)" );
    }

    ~X()
    {
        --instances;
    }
};

int X::instances = 0;

void test()
{
    X::instances = 0;

    {
        variant<X> v1;

        BOOST_TEST_EQ( X::instances, 1 );

        try
        {
            variant<X> v2( v1 );
            BOOST_TEST_EQ( X::instances, 2 );
        }
        catch( std::exception const& )
        {
        }

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );
}

int main()
{
    test();
    return boost::report_errors();
}
