//---------------------------------------------------------------------------//
// Copyright (c) 2014 Mageswaran.D <mageswaran1989@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestLexicographicalCompare
#include <boost/test/unit_test.hpp>

#include <boost/compute/container/string.hpp>
#include <boost/compute/container/basic_string.hpp>
#include <boost/compute/algorithm/lexicographical_compare.hpp>

#include "context_setup.hpp"
#include "check_macros.hpp"

BOOST_AUTO_TEST_CASE(lexicographical_compare_string)
{
    boost::compute::string a = "abcdefghijk";
    boost::compute::string b = "abcdefghijk";
    boost::compute::string c = "abcdefghija";
    boost::compute::string d = "zabcdefghij";

    BOOST_CHECK(boost::compute::lexicographical_compare(a.begin(), 
                                                        a.end(), 
                                                        b.begin(), 
                                                        b.end()) == false);

    BOOST_CHECK(boost::compute::lexicographical_compare(c.begin(), 
                                                        c.end(), 
                                                        a.begin(), 
                                                        a.end()) == true);

    BOOST_CHECK(boost::compute::lexicographical_compare(c.begin(),
                                                        c.end(),
                                                        d.begin(),
                                                        d.end()) == true);
}

BOOST_AUTO_TEST_CASE(lexicographical_compare_number)
{
    int data1[] = { 1, 2, 3, 4, 5, 6 };
    int data2[] = { 9, 2, 3, 4, 5, 6 };
    int data3[] = { 1, 2, 3, 4, 5 };
    int data4[] = { 9, 2, 3, 4, 5, 100 };

    boost::compute::vector<int> vector1(data1, data1 + 6, queue);
    boost::compute::vector<int> vector2(data2, data2 + 6, queue);
    boost::compute::vector<int> vector3(data3, data3 + 5, queue);
    boost::compute::vector<int> vector4(data4, data4 + 6, queue);

    BOOST_CHECK(boost::compute::lexicographical_compare(vector1.begin(),
                                                        vector1.end(),
                                                        vector2.begin(),
                                                        vector2.end(),
                                                        queue) == true);

    BOOST_CHECK(boost::compute::lexicographical_compare(vector1.begin(),
                                                        vector1.end(),
                                                        vector3.begin(),
                                                        vector3.end(),
                                                        queue) == false);

    BOOST_CHECK(boost::compute::lexicographical_compare(vector3.begin(),
                                                        vector3.end(),
                                                        vector4.begin(),
                                                        vector4.end(),
                                                        queue) == true);
}

BOOST_AUTO_TEST_SUITE_END()
