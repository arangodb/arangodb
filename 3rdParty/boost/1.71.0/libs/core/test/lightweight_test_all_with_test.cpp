//
// Test for BOOST_TEST_ALL_WITH
//
// Copyright (c) 2017 Bjorn Reese
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <cmath>
#include <functional>
#include <vector>
#include <boost/core/lightweight_test.hpp>

void test_vector()
{
    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 1 ); y.push_back( 2 ); y.push_back( 3 ); y.push_back( 4 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::equal_to<int>() );
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 2 ); y.push_back( 3 ); y.push_back( 4 ); y.push_back( 5 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::not_equal_to<int>() );
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 2 ); y.push_back( 3 ); y.push_back( 4 ); y.push_back( 5 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::less<int>() );
    }
}

template <typename T>
struct with_tolerance
{
    with_tolerance(T tolerance) : tolerance(tolerance) {}
    bool operator()(T lhs, T rhs)
    {
        return (std::abs(lhs - rhs) <= tolerance);
    }

private:
    T tolerance;
};

void test_tolerance_predicate()
{
    {
        std::vector<double> x, y;
        x.push_back( 1.0 ); x.push_back( 2.0 ); x.push_back( 3.0 ); x.push_back( 4.0 );
        y.push_back( 1.0 ); y.push_back( 2.0 ); y.push_back( 3.0 ); y.push_back( 4.0 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), with_tolerance<double>(1e-5) );
    }

    {
        std::vector<double> x, y;
        x.push_back( 1.0 ); x.push_back( 1.0 );
        y.push_back( 1.0 - 1e-6 ); y.push_back( 1.0 + 1e-6 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), with_tolerance<double>(1e-5) );
    }
}

int main()
{
    test_vector();
    test_tolerance_predicate();

    return boost::report_errors();
}
