// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <utility>

struct X
{
    int m1;
    int m2;

    int f1() const
    {
        return m1;
    }

    int& f2()
    {
        return m2;
    }
};

int main()
{
    using namespace boost::lambda2;

    {
        X const x{ 1, 2 };

        BOOST_TEST_EQ( (_1->*&X::m1)( x ), 1 );
        BOOST_TEST_EQ( (_1->*&X::m2)( x ), 2 );
        BOOST_TEST_EQ( (_1->*&X::f1)( x ), 1 );
    }

    {
        X x{ 0, 0 };

        BOOST_TEST_EQ( (_1->*&X::m1)( x ), 0 );
        BOOST_TEST_EQ( (_1->*&X::m2)( x ), 0 );
        BOOST_TEST_EQ( (_1->*&X::f1)( x ), 0 );
        BOOST_TEST_EQ( (_1->*&X::f2)( x ), 0 );

#if defined(_LIBCPP_VERSION)

        // https://bugs.llvm.org/show_bug.cgi?id=51753
        using std::placeholders::_1;

#endif

        BOOST_TEST_EQ( (_1->*&X::m1 += 1)( x ), 1 );
        BOOST_TEST_EQ( (_1->*&X::m2 += 2)( x ), 2 );

        BOOST_TEST_EQ( (_1->*&X::f1)( x ), 1 );
        BOOST_TEST_EQ( (_1->*&X::f2)( x ), 2 );

        BOOST_TEST_EQ( (_1->*&X::f2 += 3)( x ), 5 );
    }

    {
        std::pair<int, int> const x( 1, 2 );

        BOOST_TEST_EQ( (_1->*&std::pair<int, int>::first)( x ), 1 );
        BOOST_TEST_EQ( (_1->*&std::pair<int, int>::second)( x ), 2 );

#if defined(_LIBCPP_VERSION)

        // https://bugs.llvm.org/show_bug.cgi?id=51753
        using std::placeholders::_1;

#endif

        BOOST_TEST_EQ( (_1->*first)( x ), 1 );
        BOOST_TEST_EQ( (_1->*second)( x ), 2 );
    }

    return boost::report_errors();
}
