//
// Negative test for BOOST_TEST_ALL_EQ
//
// Copyright (c) 2017 Bjorn Reese
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <vector>
#include <set>
#include <boost/core/lightweight_test.hpp>

int main()
{
    int test_cases = 0;

    // Array

    {
        int x[] = { 1 };
        int y[] = { 1, 2 };
        BOOST_TEST_ALL_EQ( x, x + sizeof(x)/sizeof(x[0]), y, y + sizeof(y)/sizeof(y[0]) );
        ++test_cases;
    }

    {
        int x[] = { 1, 2 };
        int y[] = { 1 };
        BOOST_TEST_ALL_EQ( x, x + sizeof(x)/sizeof(x[0]), y, y + sizeof(y)/sizeof(y[0]) );
        ++test_cases;
    }

    {
        int x[] = { 2 };
        int y[] = { 1, 2 };
        BOOST_TEST_ALL_EQ( x, x + sizeof(x)/sizeof(x[0]), y, y + sizeof(y)/sizeof(y[0]) );
        ++test_cases;
    }

    {
        int x[] = { 1, 2, 3, 4 };
        int y[] = { 1, 3, 2, 4 };
        BOOST_TEST_ALL_EQ( x, x + sizeof(x)/sizeof(x[0]), y, y + sizeof(y)/sizeof(y[0]) );
        ++test_cases;
    }

    // Vector

    {
        std::vector<int> x, y;
        x.push_back( 1 );
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        y.push_back( 1 );
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 1 ); y.push_back( 3 ); y.push_back( 2 ); y.push_back( 4 );
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    {
        std::vector<float> x, y;
        x.push_back( 1.0f ); x.push_back( 2.0f ); x.push_back( 3.0f ); x.push_back( 4.0f );
        y.push_back( 4.0f ); y.push_back( 2.0f ); y.push_back( 3.0f ); y.push_back( 1.0f );
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 );
        y.push_back( 1 ); y.push_back( 3 ); y.push_back( 2 ); y.push_back( 4 );
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    {
        std::vector<int> x, y;
        x.push_back( 1 ); x.push_back( 2 ); x.push_back( 3 ); x.push_back( 4 );
        y.push_back( 1 ); y.push_back( 3 ); y.push_back( 2 );;
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    // Set

    {
        std::set<int> x, y;
        x.insert(1);
        y.insert(1); y.insert(3);
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    {
        std::set<int> x, y;
        x.insert(1); x.insert(2);
        y.insert(1);
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    {
        std::set<int> x, y;
        x.insert(1); x.insert(2);
        y.insert(1); y.insert(3);
        BOOST_TEST_ALL_EQ( x.begin(), x.end(), y.begin(), y.end() );
        ++test_cases;
    }

    return boost::report_errors() == test_cases;
}
