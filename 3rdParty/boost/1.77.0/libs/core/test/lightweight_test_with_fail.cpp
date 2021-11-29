// Negative test for BOOST_TEST_WITH
//
// Copyright 2020 Bjorn Reese
// Copyright 2020 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <cmath>

template <typename T>
struct with_tolerance
{
    with_tolerance( T tolerance ): tolerance( tolerance )
    {
    }

    bool operator()( T lhs, T rhs ) const
    {
        return std::abs( lhs - rhs ) <= tolerance;
    }

private:

    T tolerance;
};

void test_tolerance_predicate()
{
    BOOST_TEST_WITH( 1.0, 1.0 - 1e-6, with_tolerance<double>(1e-7) );
    BOOST_TEST_WITH( 1.0, 1.0 + 1e-6, with_tolerance<double>(1e-7) );
}

int main()
{
    test_tolerance_predicate();
    return boost::report_errors() == 2;
}
