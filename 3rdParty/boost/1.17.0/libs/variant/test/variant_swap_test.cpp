//-----------------------------------------------------------------------------
// boost-libs variant/test/variant_swap_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2009 ArtVPS Ltd.
// Copyright (c) 2013-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/core/lightweight_test.hpp"
#include "boost/variant.hpp"

#include <vector>
#include <algorithm>
 
void run1()
{
    using boost::variant;
    typedef variant< int, std::vector<int>* > t_var;

    std::vector<int> vec;
    t_var v0(23), v1(&vec);

    BOOST_TEST(v0.which() == 0);
    BOOST_TEST(v1.which() == 1);

    swap(v0, v1);

    BOOST_TEST(v0.which() == 1);
    BOOST_TEST(v1.which() == 0);
}

void run2()
{
    using boost::variant;
    using std::swap;
    typedef variant< std::vector<int>, std::vector<double> > t_var;
 
    std::vector<int> vec1;
    std::vector<double> vec2;
    t_var v0(vec1), v1(vec2);

    BOOST_TEST(v0.which() == 0);
    BOOST_TEST(v1.which() == 1);

    swap(v0, v1);

    BOOST_TEST(v0.which() == 1);
    BOOST_TEST(v1.which() == 0);

    v0.swap(v1);

    BOOST_TEST(v0.which() == 0);
    BOOST_TEST(v1.which() == 1);
}

void run3()
{
    using boost::variant;
    using std::swap;
    typedef variant< int, double > t_var;
 
    t_var v0(1), v1(2.0);

    BOOST_TEST(v0.which() == 0);
    BOOST_TEST(v1.which() == 1);

    swap(v0, v1);

    BOOST_TEST(v0.which() == 1);
    BOOST_TEST(v1.which() == 0);

    v0.swap(v1);

    BOOST_TEST(v0.which() == 0);
    BOOST_TEST(v1.which() == 1);
}

int main() 
{ 
   run1();
   run2();
   run3();

   return boost::report_errors();
}

