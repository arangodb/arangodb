
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <utility>
#include <string>

using namespace boost::variant2;

struct X1
{
    static int instances;

    int v;

    X1(): v(0) { ++instances; }
    explicit X1(int v): v(v) { ++instances; }
    X1(X1 const& r): v(r.v) { ++instances; }
    X1(X1&& r): v(r.v) { ++instances; }

    ~X1() noexcept { --instances; }

    X1& operator=( X1 const& r ) { v = r.v; return *this; }
    X1& operator=( X1&& r ) { v = r.v; return *this; }
};

int X1::instances = 0;

struct X2
{
    static int instances;

    int v;

    X2(): v(0) { ++instances; }
    explicit X2(int v): v(v) { ++instances; }
    X2(X2 const& r): v(r.v) { ++instances; }
    X2(X2&& r): v(r.v) { ++instances; }

    ~X2() noexcept { --instances; }

    X2& operator=( X2 const& r ) { v = r.v; return *this; }
    X2& operator=( X2&& r ) { v = r.v; return *this; }
};

int X2::instances = 0;

struct Y1
{
    static int instances;

    int v;

    Y1() noexcept: v(0) { ++instances; }
    explicit Y1(int v) noexcept: v(v) { ++instances; }
    Y1(Y1 const& r) noexcept: v(r.v) { ++instances; }
    Y1(Y1&& r) noexcept: v(r.v) { ++instances; }

    ~Y1() noexcept { --instances; }

    Y1& operator=( Y1 const& r ) noexcept { v = r.v; return *this; }
    Y1& operator=( Y1&& r ) noexcept { v = r.v; return *this; }
};

int Y1::instances = 0;

struct Y2
{
    static int instances;

    int v;

    Y2() noexcept: v(0) { ++instances; }
    explicit Y2(int v) noexcept: v(v) { ++instances; }
    Y2(Y2 const& r) noexcept: v(r.v) { ++instances; }
    Y2(Y2&& r) noexcept: v(r.v) { ++instances; }

    ~Y2() noexcept { --instances; }

    Y2& operator=( Y2 const& r ) noexcept { v = r.v; return *this; }
    Y2& operator=( Y2&& r ) noexcept { v = r.v; return *this; }
};

int Y2::instances = 0;

int main()
{
    BOOST_TEST_EQ( Y1::instances, 0 );

    {
        variant<Y1> v;
        BOOST_TEST_EQ( Y1::instances, 1 );

        {
            variant<Y1> v2;
            BOOST_TEST_EQ( Y1::instances, 2 );

            v = v2;
            BOOST_TEST_EQ( Y1::instances, 2 );
        }

        BOOST_TEST_EQ( Y1::instances, 1 );

        v = Y1{1};
        BOOST_TEST_EQ( Y1::instances, 1 );
    }

    BOOST_TEST_EQ( Y1::instances, 0 );
    BOOST_TEST_EQ( Y2::instances, 0 );

    {
        variant<Y1, Y2> v;

        BOOST_TEST_EQ( Y1::instances, 1 );
        BOOST_TEST_EQ( Y2::instances, 0 );

        {
            variant<Y1, Y2> v2;
            BOOST_TEST_EQ( Y1::instances, 2 );
            BOOST_TEST_EQ( Y2::instances, 0 );

            v = v2;
            BOOST_TEST_EQ( Y1::instances, 2 );
            BOOST_TEST_EQ( Y2::instances, 0 );

            v2 = Y2{1};
            BOOST_TEST_EQ( Y1::instances, 1 );
            BOOST_TEST_EQ( Y2::instances, 1 );

            v = v2;
            BOOST_TEST_EQ( Y1::instances, 0 );
            BOOST_TEST_EQ( Y2::instances, 2 );
        }

        BOOST_TEST_EQ( Y1::instances, 0 );
        BOOST_TEST_EQ( Y2::instances, 1 );

        v.emplace<0>();

        BOOST_TEST_EQ( Y1::instances, 1 );
        BOOST_TEST_EQ( Y2::instances, 0 );

        v.emplace<Y2>();

        BOOST_TEST_EQ( Y1::instances, 0 );
        BOOST_TEST_EQ( Y2::instances, 1 );
    }

    BOOST_TEST_EQ( Y1::instances, 0 );
    BOOST_TEST_EQ( Y2::instances, 0 );

    BOOST_TEST_EQ( X1::instances, 0 );
    BOOST_TEST_EQ( X2::instances, 0 );

    {
        variant<X1, X2> v;

        BOOST_TEST_EQ( X1::instances, 1 );
        BOOST_TEST_EQ( X2::instances, 0 );

        {
            variant<X1, X2> v2;
            BOOST_TEST_EQ( X1::instances, 2 );
            BOOST_TEST_EQ( X2::instances, 0 );

            v = v2;
            BOOST_TEST_EQ( X1::instances, 2 );
            BOOST_TEST_EQ( X2::instances, 0 );

            v2 = X2{1};
            BOOST_TEST_EQ( X1::instances, 1 );
            BOOST_TEST_EQ( X2::instances, 1 );

            v = v2;
            BOOST_TEST_EQ( X1::instances, 0 );
            BOOST_TEST_EQ( X2::instances, 2 );
        }

        BOOST_TEST_EQ( X1::instances, 0 );
        BOOST_TEST_EQ( X2::instances, 1 );

        v.emplace<0>();

        BOOST_TEST_EQ( X1::instances, 1 );
        BOOST_TEST_EQ( X2::instances, 0 );

        v.emplace<X2>();

        BOOST_TEST_EQ( X1::instances, 0 );
        BOOST_TEST_EQ( X2::instances, 1 );
    }

    BOOST_TEST_EQ( X1::instances, 0 );
    BOOST_TEST_EQ( X2::instances, 0 );

    return boost::report_errors();
}
