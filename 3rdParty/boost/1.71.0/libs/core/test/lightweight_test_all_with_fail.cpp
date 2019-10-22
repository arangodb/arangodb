//
// Negative est for BOOST_TEST_ALL_WITH
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

int fail_vector()
{
    int test_cases = 0;

    {
        std::vector<int> x, y;
        x.push_back( 1 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::equal_to<int>() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        y.push_back( 1 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::equal_to<int>() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 1 ); y.push_back( 2 ); y.push_back( 3 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::equal_to<int>() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 );
        y.push_back( 1 ); y.push_back( 2 ); y.push_back( 3 ); y.push_back( 4 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::equal_to<int>() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 1 ); y.push_back( 3 ); y.push_back( 2 ); y.push_back( 4 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::equal_to<int>() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 1 ); y.push_back( 3 ); y.push_back( 2 ); y.push_back( 4 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), std::less<int>() );
        ++test_cases;
    }

    return test_cases;
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

int fail_tolerance_predicate()
{
    int test_cases = 0;

    {
        std::vector<double> x, y;
        x.push_back( 1.0 ); x.push_back( 1.0 );
        y.push_back( 1.0 - 1e-4 ); y.push_back( 1.0 + 1e-4 );
        BOOST_TEST_ALL_WITH( x.begin(), x.end(), y.begin(), y.end(), with_tolerance<double>(1e-5) );
        ++test_cases;
    }

    return test_cases;
}

int main()
{
    int test_cases = 0;

    test_cases += fail_vector();
    test_cases += fail_tolerance_predicate();

    return boost::report_errors() == test_cases;
}
