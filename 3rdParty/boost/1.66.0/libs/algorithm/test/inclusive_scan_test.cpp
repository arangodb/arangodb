/*
   Copyright (c) Marshall Clow 2017.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <vector>
#include <functional>
#include <numeric>
#include <algorithm>

#include <boost/config.hpp>
#include <boost/algorithm/cxx11/iota.hpp>
#include <boost/algorithm/cxx17/inclusive_scan.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

namespace ba = boost::algorithm;

int triangle(int n) { return n*(n+1)/2; }

void basic_tests_op()
{
    {
    std::vector<int> v(10);
    std::fill(v.begin(), v.end(), 3);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>());
    for (size_t i = 0; i < v.size(); ++i)
        assert(v[i] == (int)(i+1) * 3);
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 0);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>());
    for (size_t i = 0; i < v.size(); ++i)
        assert(v[i] == triangle(i));
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 1);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>());
    for (size_t i = 0; i < v.size(); ++i)
        assert(v[i] == triangle(i + 1));
    }

    {
    std::vector<int> v, res;
    ba::inclusive_scan(v.begin(), v.end(), std::back_inserter(res), std::plus<int>());
    assert(res.empty());
    }
}

void test_inclusive_scan_op()
{
	basic_tests_op();
	BOOST_CHECK(true);
}

void basic_tests_init()
{
    {
    std::vector<int> v(10);
    std::fill(v.begin(), v.end(), 3);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>(), 50);
    for (size_t i = 0; i < v.size(); ++i)
        assert(v[i] == 50 + (int)(i+1) * 3);
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 0);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>(), 40);
    for (size_t i = 0; i < v.size(); ++i)
        assert(v[i] == 40 + triangle(i));
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 1);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>(), 30);
    for (size_t i = 0; i < v.size(); ++i)
        assert(v[i] == 30 + triangle(i + 1));
    }

    {
    std::vector<int> v, res;
    ba::inclusive_scan(v.begin(), v.end(), std::back_inserter(res), std::plus<int>(), 40);
    assert(res.empty());
    }
}


void test_inclusive_scan_init()
{
	basic_tests_init();
	BOOST_CHECK(true);
}

void basic_tests_op_init()
{
    {
    std::vector<int> v(10);
    std::fill(v.begin(), v.end(), 3);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>(), 50);
    for (size_t i = 0; i < v.size(); ++i)
        BOOST_CHECK(v[i] == 50 + (int)(i+1) * 3);
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 0);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>(), 40);
    for (size_t i = 0; i < v.size(); ++i)
        BOOST_CHECK(v[i] == 40 + triangle(i));
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 1);
    ba::inclusive_scan(v.begin(), v.end(), v.begin(), std::plus<int>(), 30);
    for (size_t i = 0; i < v.size(); ++i)
        BOOST_CHECK(v[i] == 30 + triangle(i + 1));
    }

    {
    std::vector<int> v, res;
    ba::inclusive_scan(v.begin(), v.end(), std::back_inserter(res), std::plus<int>(), 40);
    BOOST_CHECK(res.empty());
    }
}

void test_inclusive_scan_op_init()
{
    basic_tests_op_init();
	BOOST_CHECK(true);
}



BOOST_AUTO_TEST_CASE( test_main )
{
  test_inclusive_scan_op();
  test_inclusive_scan_init();
  test_inclusive_scan_op_init();
}
