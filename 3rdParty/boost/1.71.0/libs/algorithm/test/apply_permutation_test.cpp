/*
  Copyright (c) Alexander Zaitsev <zamazan4ik@gmail.com>, 2017

  Distributed under the Boost Software License, Version 1.0. (See
  accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt)

  See http://www.boost.org/ for latest version.
*/

#include <vector>

#include <boost/algorithm/apply_permutation.hpp>

#define BOOST_TEST_MAIN

#include <boost/test/included/unit_test.hpp>

namespace ba = boost::algorithm;


BOOST_AUTO_TEST_CASE(test_apply_permutation)
{
    //Empty
    {
        std::vector<int> vec, order, result;
        
        ba::apply_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //1 element
    {
        std::vector<int> vec, order, result;
        vec.push_back(1);
        order.push_back(0);
        result = vec;
        
        ba::apply_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //2 elements, no changes
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2);
        order.push_back(0); order.push_back(1);
        result = vec;
        
        ba::apply_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //2 elements, changed
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2);
        order.push_back(1); order.push_back(0);
        result.push_back(2); result.push_back(1);
        
        ba::apply_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //Multiple elements, no changes
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2); vec.push_back(3); vec.push_back(4); vec.push_back(5);
        order.push_back(0); order.push_back(1); order.push_back(2); order.push_back(3); order.push_back(4);
        result = vec;
        
        ba::apply_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //Multiple elements, changed
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2); vec.push_back(3); vec.push_back(4); vec.push_back(5);
        order.push_back(4); order.push_back(3); order.push_back(2); order.push_back(1); order.push_back(0);
        result.push_back(5); result.push_back(4); result.push_back(3); result.push_back(2); result.push_back(1);
        
        ba::apply_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //Just test range interface
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2); vec.push_back(3); vec.push_back(4); vec.push_back(5);
        order.push_back(0); order.push_back(1); order.push_back(2); order.push_back(3); order.push_back(4);
        result = vec;
        
        ba::apply_permutation(vec, order);
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
}

BOOST_AUTO_TEST_CASE(test_apply_reverse_permutation)
{
    //Empty
    {
        std::vector<int> vec, order, result;
        
        ba::apply_reverse_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //1 element
    {
        std::vector<int> vec, order, result;
        vec.push_back(1);
        order.push_back(0);
        result = vec;
        
        ba::apply_reverse_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //2 elements, no changes
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2);
        order.push_back(0); order.push_back(1);
        result = vec;
        
        ba::apply_reverse_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //2 elements, changed
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2);
        order.push_back(1); order.push_back(0);
        result.push_back(2); result.push_back(1);
        
        ba::apply_reverse_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //Multiple elements, no changes
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2); vec.push_back(3); vec.push_back(4); vec.push_back(5);
        order.push_back(0); order.push_back(1); order.push_back(2); order.push_back(3); order.push_back(4);
        result = vec;
        
        ba::apply_reverse_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //Multiple elements, changed
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2); vec.push_back(3); vec.push_back(4); vec.push_back(5);
        order.push_back(4); order.push_back(3); order.push_back(2); order.push_back(1); order.push_back(0);
        result.push_back(5); result.push_back(4); result.push_back(3); result.push_back(2); result.push_back(1);
        
        ba::apply_reverse_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
    //Just test range interface
    {
        std::vector<int> vec, order, result;
        vec.push_back(1); vec.push_back(2); vec.push_back(3); vec.push_back(4); vec.push_back(5);
        order.push_back(0); order.push_back(1); order.push_back(2); order.push_back(3); order.push_back(4);
        result = vec;
        
        ba::apply_reverse_permutation(vec, order);
        BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), result.begin(), result.end());
    }
}
