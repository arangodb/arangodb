/*
   Copyright (c) Marshall Clow 2017.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <vector>
#include <functional>
#include <numeric>

#include <boost/config.hpp>
#include <boost/algorithm/cxx11/iota.hpp>
#include <boost/algorithm/cxx17/transform_exclusive_scan.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

namespace ba = boost::algorithm;

int triangle(int n) { return n*(n+1)/2; }

template <class _Tp>
struct identity
{
    const _Tp& operator()(const _Tp& __x) const { return __x;}
};


template <class Iter1, class BOp, class UOp, class T, class Iter2>
void
test(Iter1 first, Iter1 last, BOp bop, UOp uop, T init, Iter2 rFirst, Iter2 rLast)
{
    std::vector<typename std::iterator_traits<Iter1>::value_type> v;
//  Test not in-place
    ba::transform_exclusive_scan(first, last, std::back_inserter(v), init, bop, uop);
    BOOST_CHECK(std::distance(rFirst, rLast) == v.size());
    BOOST_CHECK(std::equal(v.begin(), v.end(), rFirst));

//  Test in-place
    v.clear();
    v.assign(first, last);
    ba::transform_exclusive_scan(v.begin(), v.end(), v.begin(), init, bop, uop);
    BOOST_CHECK(std::distance(rFirst, rLast) == v.size());
    BOOST_CHECK(std::equal(v.begin(), v.end(), rFirst));
}


template <class Iter>
void
test()
{
          int ia[]     = { 1,  3,  5,   7,   9};
    const int pResI0[] = { 0,  1,  4,   9,  16};        // with identity
    const int mResI0[] = { 0,  0,  0,   0,   0};
    const int pResN0[] = { 0, -1, -4,  -9, -16};        // with negate
    const int mResN0[] = { 0,  0,  0,   0,   0};
    const int pResI2[] = { 2,  3,  6,  11,  18};        // with identity
    const int mResI2[] = { 2,  2,  6,  30, 210};
    const int pResN2[] = { 2,  1, -2,  -7, -14};        // with negate
    const int mResN2[] = { 2, -2,  6, -30, 210};
    const unsigned sa = sizeof(ia) / sizeof(ia[0]);
    BOOST_CHECK(sa == sizeof(pResI0) / sizeof(pResI0[0]));       // just to be sure
    BOOST_CHECK(sa == sizeof(mResI0) / sizeof(mResI0[0]));       // just to be sure
    BOOST_CHECK(sa == sizeof(pResN0) / sizeof(pResN0[0]));       // just to be sure
    BOOST_CHECK(sa == sizeof(mResN0) / sizeof(mResN0[0]));       // just to be sure
    BOOST_CHECK(sa == sizeof(pResI2) / sizeof(pResI2[0]));       // just to be sure
    BOOST_CHECK(sa == sizeof(mResI2) / sizeof(mResI2[0]));       // just to be sure
    BOOST_CHECK(sa == sizeof(pResN2) / sizeof(pResN2[0]));       // just to be sure
    BOOST_CHECK(sa == sizeof(mResN2) / sizeof(mResN2[0]));       // just to be sure

    for (unsigned int i = 0; i < sa; ++i ) {
        test(Iter(ia), Iter(ia + i), std::plus<int>(),       identity<int>(),    0, pResI0, pResI0 + i);
        test(Iter(ia), Iter(ia + i), std::multiplies<int>(), identity<int>(),    0, mResI0, mResI0 + i);
        test(Iter(ia), Iter(ia + i), std::plus<int>(),       std::negate<int>(), 0, pResN0, pResN0 + i);
        test(Iter(ia), Iter(ia + i), std::multiplies<int>(), std::negate<int>(), 0, mResN0, mResN0 + i);
        test(Iter(ia), Iter(ia + i), std::plus<int>(),       identity<int>(),    2, pResI2, pResI2 + i);
        test(Iter(ia), Iter(ia + i), std::multiplies<int>(), identity<int>(),    2, mResI2, mResI2 + i);
        test(Iter(ia), Iter(ia + i), std::plus<int>(),       std::negate<int>(), 2, pResN2, pResN2 + i);
        test(Iter(ia), Iter(ia + i), std::multiplies<int>(), std::negate<int>(), 2, mResN2, mResN2 + i);
        }
}

void basic_tests()
{
    {
    std::vector<int> v(10);
    std::fill(v.begin(), v.end(), 3);
    ba::transform_exclusive_scan(v.begin(), v.end(), v.begin(), 50, std::plus<int>(), identity<int>());
    for (size_t i = 0; i < v.size(); ++i)
        BOOST_CHECK(v[i] == 50 + (int) i * 3);
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 0);
    ba::transform_exclusive_scan(v.begin(), v.end(), v.begin(), 30, std::plus<int>(), identity<int>());
    for (size_t i = 0; i < v.size(); ++i)
        BOOST_CHECK(v[i] == 30 + triangle(i-1));
    }

    {
    std::vector<int> v(10);
    ba::iota(v.begin(), v.end(), 1);
    ba::transform_exclusive_scan(v.begin(), v.end(), v.begin(), 40, std::plus<int>(), identity<int>());
    for (size_t i = 0; i < v.size(); ++i)
        BOOST_CHECK(v[i] == 40 + triangle(i));
    }

    {
    std::vector<int> v, res;
    ba::transform_exclusive_scan(v.begin(), v.end(), std::back_inserter(res), 40, std::plus<int>(), identity<int>());
    BOOST_CHECK(res.empty());
    }
}


void test_transform_exclusive_scan_init()
{
	basic_tests();
//  All the iterator categories
    test<input_iterator        <const int*> >();
    test<forward_iterator      <const int*> >();
    test<bidirectional_iterator<const int*> >();
    test<random_access_iterator<const int*> >();
    test<const int*>();
    test<      int*>();
}

BOOST_AUTO_TEST_CASE( test_main )
{
  test_transform_exclusive_scan_init();
}
