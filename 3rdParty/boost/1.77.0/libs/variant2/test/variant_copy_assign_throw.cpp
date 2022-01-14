
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

struct Y1
{
    Y1() noexcept {} // =default fails on msvc-14.0

    Y1(Y1 const&)
    {
        throw std::runtime_error( "Y1(Y1 const&)" );
    }

    Y1& operator=(Y1 const&) = default;
};

struct Y2
{
    Y2() noexcept {}

    Y2(Y2 const&)
    {
        throw std::runtime_error( "Y2(Y2 const&)" );
    }

    Y2& operator=(Y2 const&) = default;
};

void test()
{
    variant<Y1, Y2> v1( in_place_type_t<Y1>{} );
    variant<Y1, Y2> v2( in_place_type_t<Y2>{} );

    BOOST_TEST_THROWS( v1 = v2, std::runtime_error )
}

int main()
{
    test();
    return boost::report_errors();
}
