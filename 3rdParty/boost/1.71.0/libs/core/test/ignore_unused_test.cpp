// Copyright (c) 2014 Adam Wulkiewicz, Lodz, Poland.
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/ignore_unused.hpp>

BOOST_CXX14_CONSTEXPR int test_fun(int a)
{
    boost::ignore_unused(a);
    return 0;
}

int main()
{
    {
        int a;
        boost::ignore_unused(a);
    }
    {
        int a, b;
        boost::ignore_unused(a, b);
    }
    {
        int a, b, c;
        boost::ignore_unused(a, b, c);
    }
    {
        int a, b, c, d;
        boost::ignore_unused(a, b, c, d);
    }
    {
        int a, b, c, d, e;
        boost::ignore_unused(a, b, c, d, e);
    }

    {
        typedef int a;
        boost::ignore_unused<a>();
    }
    {
        typedef int a;
        typedef int b;
        boost::ignore_unused<a, b>();
    }
    {
        typedef int a;
        typedef int b;
        typedef int c;
        boost::ignore_unused<a, b, c>();
    }
    {
        typedef int a;
        typedef int b;
        typedef int c;
        typedef int d;
        boost::ignore_unused<a, b, c, d>();
    }
    {
        typedef int a;
        typedef int b;
        typedef int c;
        typedef int d;
        typedef int e;
        boost::ignore_unused<a, b, c, d, e>();
    }

    {
        BOOST_CXX14_CONSTEXPR const int a = test_fun(0);
        boost::ignore_unused(a);
    }

    return 0;
}
