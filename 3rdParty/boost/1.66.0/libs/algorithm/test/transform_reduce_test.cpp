/*
   Copyright (c) Marshall Clow 2013.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <boost/config.hpp>
#include <boost/algorithm/cxx17/transform_reduce.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

namespace ba = boost::algorithm;

template <class _Tp>
struct identity
{
    const _Tp& operator()(const _Tp& __x) const { return __x;}
};

template <class _Tp>
struct twice
{
  	const _Tp operator()(const _Tp& __x) const { return 2 * __x; }
};


template <class Iter1, class T, class BOp, class UOp>
void
test_init_bop_uop(Iter1 first1, Iter1 last1, T init, BOp bOp, UOp uOp, T x)
{
    BOOST_CHECK(ba::transform_reduce(first1, last1, init, bOp, uOp) == x);
}

template <class Iter>
void
test_init_bop_uop()
{
    int ia[]          = {1, 2, 3, 4, 5, 6};
    unsigned sa = sizeof(ia) / sizeof(ia[0]);

    test_init_bop_uop(Iter(ia), Iter(ia),    0, std::plus<int>(),       identity<int>(),       0);
    test_init_bop_uop(Iter(ia), Iter(ia),    1, std::multiplies<int>(), identity<int>(),       1);
    test_init_bop_uop(Iter(ia), Iter(ia+1),  0, std::multiplies<int>(), identity<int>(),       0);
    test_init_bop_uop(Iter(ia), Iter(ia+1),  2, std::plus<int>(),       identity<int>(),       3);
    test_init_bop_uop(Iter(ia), Iter(ia+2),  0, std::plus<int>(),       identity<int>(),       3);
    test_init_bop_uop(Iter(ia), Iter(ia+2),  3, std::multiplies<int>(), identity<int>(),       6);
    test_init_bop_uop(Iter(ia), Iter(ia+sa), 4, std::multiplies<int>(), identity<int>(),    2880);
    test_init_bop_uop(Iter(ia), Iter(ia+sa), 4, std::plus<int>(),       identity<int>(),      25);

    test_init_bop_uop(Iter(ia), Iter(ia),    0, std::plus<int>(),       twice<int>(),       0);
    test_init_bop_uop(Iter(ia), Iter(ia),    1, std::multiplies<int>(), twice<int>(),       1);
    test_init_bop_uop(Iter(ia), Iter(ia+1),  0, std::multiplies<int>(), twice<int>(),       0);
    test_init_bop_uop(Iter(ia), Iter(ia+1),  2, std::plus<int>(),       twice<int>(),       4);
    test_init_bop_uop(Iter(ia), Iter(ia+2),  0, std::plus<int>(),       twice<int>(),       6);
    test_init_bop_uop(Iter(ia), Iter(ia+2),  3, std::multiplies<int>(), twice<int>(),      24);
    test_init_bop_uop(Iter(ia), Iter(ia+sa), 4, std::multiplies<int>(), twice<int>(),  184320); // 64 * 2880
    test_init_bop_uop(Iter(ia), Iter(ia+sa), 4, std::plus<int>(),       twice<int>(),      46);
}

void test_transform_reduce_init_bop_uop()
{
	BOOST_CHECK ( true );
}

template <class Iter1, class Iter2, class T, class Op1, class Op2>
void
test_init_bop_bop(Iter1 first1, Iter1 last1, Iter2 first2, T init, Op1 op1, Op2 op2, T x)
{
    BOOST_CHECK(ba::transform_reduce(first1, last1, first2, init, op1, op2) == x);
}

template <class SIter, class UIter>
void
test_init_bop_bop()
{
    int ia[]          = {1, 2, 3, 4, 5, 6};
    unsigned int ua[] = {2, 4, 6, 8, 10,12};
    unsigned sa = sizeof(ia) / sizeof(ia[0]);
    BOOST_CHECK(sa == sizeof(ua) / sizeof(ua[0]));       // just to be sure

    test_init_bop_bop(SIter(ia), SIter(ia),    UIter(ua), 0, std::plus<int>(), std::multiplies<int>(),       0);
    test_init_bop_bop(UIter(ua), UIter(ua),    SIter(ia), 1, std::multiplies<int>(), std::plus<int>(),       1);
    test_init_bop_bop(SIter(ia), SIter(ia+1),  UIter(ua), 0, std::multiplies<int>(), std::plus<int>(),       0);
    test_init_bop_bop(UIter(ua), UIter(ua+1),  SIter(ia), 2, std::plus<int>(), std::multiplies<int>(),       4);
    test_init_bop_bop(SIter(ia), SIter(ia+2),  UIter(ua), 0, std::plus<int>(), std::multiplies<int>(),      10);
    test_init_bop_bop(UIter(ua), UIter(ua+2),  SIter(ia), 3, std::multiplies<int>(), std::plus<int>(),      54);
    test_init_bop_bop(SIter(ia), SIter(ia+sa), UIter(ua), 4, std::multiplies<int>(), std::plus<int>(), 2099520);
    test_init_bop_bop(UIter(ua), UIter(ua+sa), SIter(ia), 4, std::plus<int>(), std::multiplies<int>(),     186);
}

void test_transform_reduce_init_bop_bop()
{
//  All the iterator categories
    test_init_bop_bop<input_iterator        <const int*>, input_iterator        <const unsigned int*> >();
    test_init_bop_bop<input_iterator        <const int*>, forward_iterator      <const unsigned int*> >();
    test_init_bop_bop<input_iterator        <const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init_bop_bop<input_iterator        <const int*>, random_access_iterator<const unsigned int*> >();

    test_init_bop_bop<forward_iterator      <const int*>, input_iterator        <const unsigned int*> >();
    test_init_bop_bop<forward_iterator      <const int*>, forward_iterator      <const unsigned int*> >();
    test_init_bop_bop<forward_iterator      <const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init_bop_bop<forward_iterator      <const int*>, random_access_iterator<const unsigned int*> >();

    test_init_bop_bop<bidirectional_iterator<const int*>, input_iterator        <const unsigned int*> >();
    test_init_bop_bop<bidirectional_iterator<const int*>, forward_iterator      <const unsigned int*> >();
    test_init_bop_bop<bidirectional_iterator<const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init_bop_bop<bidirectional_iterator<const int*>, random_access_iterator<const unsigned int*> >();

    test_init_bop_bop<random_access_iterator<const int*>, input_iterator        <const unsigned int*> >();
    test_init_bop_bop<random_access_iterator<const int*>, forward_iterator      <const unsigned int*> >();
    test_init_bop_bop<random_access_iterator<const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init_bop_bop<random_access_iterator<const int*>, random_access_iterator<const unsigned int*> >();

//  just plain pointers (const vs. non-const, too)
    test_init_bop_bop<const int*, const unsigned int *>();
    test_init_bop_bop<const int*,       unsigned int *>();
    test_init_bop_bop<      int*, const unsigned int *>();
    test_init_bop_bop<      int*,       unsigned int *>();
}

template <class Iter1, class Iter2, class T>
void
test_init(Iter1 first1, Iter1 last1, Iter2 first2, T init, T x)
{
    BOOST_CHECK(ba::transform_reduce(first1, last1, first2, init) == x);
}

template <class SIter, class UIter>
void
test_init()
{
    int ia[]          = {1, 2, 3, 4, 5, 6};
    unsigned int ua[] = {2, 4, 6, 8, 10,12};
    unsigned sa = sizeof(ia) / sizeof(ia[0]);
    BOOST_CHECK(sa == sizeof(ua) / sizeof(ua[0]));       // just to be sure

    test_init(SIter(ia), SIter(ia),    UIter(ua), 0,   0);
    test_init(UIter(ua), UIter(ua),    SIter(ia), 1,   1);
    test_init(SIter(ia), SIter(ia+1),  UIter(ua), 0,   2);
    test_init(UIter(ua), UIter(ua+1),  SIter(ia), 2,   4);
    test_init(SIter(ia), SIter(ia+2),  UIter(ua), 0,  10);
    test_init(UIter(ua), UIter(ua+2),  SIter(ia), 3,  13);
    test_init(SIter(ia), SIter(ia+sa), UIter(ua), 0, 182);
    test_init(UIter(ua), UIter(ua+sa), SIter(ia), 4, 186);
}

void test_transform_reduce_init()
{
//  All the iterator categories
    test_init<input_iterator        <const int*>, input_iterator        <const unsigned int*> >();
    test_init<input_iterator        <const int*>, forward_iterator      <const unsigned int*> >();
    test_init<input_iterator        <const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init<input_iterator        <const int*>, random_access_iterator<const unsigned int*> >();

    test_init<forward_iterator      <const int*>, input_iterator        <const unsigned int*> >();
    test_init<forward_iterator      <const int*>, forward_iterator      <const unsigned int*> >();
    test_init<forward_iterator      <const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init<forward_iterator      <const int*>, random_access_iterator<const unsigned int*> >();

    test_init<bidirectional_iterator<const int*>, input_iterator        <const unsigned int*> >();
    test_init<bidirectional_iterator<const int*>, forward_iterator      <const unsigned int*> >();
    test_init<bidirectional_iterator<const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init<bidirectional_iterator<const int*>, random_access_iterator<const unsigned int*> >();

    test_init<random_access_iterator<const int*>, input_iterator        <const unsigned int*> >();
    test_init<random_access_iterator<const int*>, forward_iterator      <const unsigned int*> >();
    test_init<random_access_iterator<const int*>, bidirectional_iterator<const unsigned int*> >();
    test_init<random_access_iterator<const int*>, random_access_iterator<const unsigned int*> >();

//  just plain pointers (const vs. non-const, too)
    test_init<const int*, const unsigned int *>();
    test_init<const int*,       unsigned int *>();
    test_init<      int*, const unsigned int *>();
    test_init<      int*,       unsigned int *>();
}

BOOST_AUTO_TEST_CASE( test_main )
{
  test_transform_reduce_init();
  test_transform_reduce_init_bop_uop();
  test_transform_reduce_init_bop_bop();
}
