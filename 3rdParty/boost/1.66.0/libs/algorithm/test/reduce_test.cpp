/*
   Copyright (c) Marshall Clow 2013.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <vector>
#include <functional>

#include <boost/config.hpp>
#include <boost/algorithm/cxx17/reduce.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

namespace ba = boost::algorithm;

template <class Iter, class T, class Op>
void
test_reduce(Iter first, Iter last, T init, Op op, T x)
{
    BOOST_CHECK(ba::reduce(first, last, init, op) == x);
}

template <class Iter, class T, class Op>
void
test_reduce(Iter first, Iter last, Op op, T x)
{
    BOOST_CHECK(ba::reduce(first, last, op) == x);
}

template <class Iter, class T>
void
test_reduce(Iter first, Iter last, T x)
{
    BOOST_CHECK(ba::reduce(first, last) == x);
}

template <class Iter>
void
test_init_op()
{
    int ia[] = {1, 2, 3, 4, 5, 6};
    unsigned sa = sizeof(ia) / sizeof(ia[0]);
    test_reduce(Iter(ia), Iter(ia), 0, std::plus<int>(), 0);
    test_reduce(Iter(ia), Iter(ia), 1, std::multiplies<int>(), 1);
    test_reduce(Iter(ia), Iter(ia+1), 0, std::plus<int>(), 1);
    test_reduce(Iter(ia), Iter(ia+1), 2, std::multiplies<int>(), 2);
    test_reduce(Iter(ia), Iter(ia+2), 0, std::plus<int>(), 3);
    test_reduce(Iter(ia), Iter(ia+2), 3, std::multiplies<int>(), 6);
    test_reduce(Iter(ia), Iter(ia+sa), 0, std::plus<int>(), 21);
    test_reduce(Iter(ia), Iter(ia+sa), 4, std::multiplies<int>(), 2880);
}

void test_reduce_init_op()
{
    test_init_op<input_iterator<const int*> >();
    test_init_op<forward_iterator<const int*> >();
    test_init_op<bidirectional_iterator<const int*> >();
    test_init_op<random_access_iterator<const int*> >();
    test_init_op<const int*>();

    {
    char ia[] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned sa = sizeof(ia) / sizeof(ia[0]);
    unsigned res = boost::algorithm::reduce(ia, ia+sa, 1U, std::multiplies<unsigned>());
    BOOST_CHECK(res == 40320);		// 8! will not fit into a char
    }
}

template <class Iter>
void
test_init()
{
    int ia[] = {1, 2, 3, 4, 5, 6};
    unsigned sa = sizeof(ia) / sizeof(ia[0]);
    test_reduce(Iter(ia), Iter(ia), 0, 0);
    test_reduce(Iter(ia), Iter(ia), 1, 1);
    test_reduce(Iter(ia), Iter(ia+1), 0, 1);
    test_reduce(Iter(ia), Iter(ia+1), 2, 3);
    test_reduce(Iter(ia), Iter(ia+2), 0, 3);
    test_reduce(Iter(ia), Iter(ia+2), 3, 6);
    test_reduce(Iter(ia), Iter(ia+sa), 0, 21);
    test_reduce(Iter(ia), Iter(ia+sa), 4, 25);
}

void test_reduce_init()
{
    test_init<input_iterator<const int*> >();
    test_init<forward_iterator<const int*> >();
    test_init<bidirectional_iterator<const int*> >();
    test_init<random_access_iterator<const int*> >();
    test_init<const int*>();
}


template <class Iter>
void
test()
{
    int ia[] = {1, 2, 3, 4, 5, 6};
    unsigned sa = sizeof(ia) / sizeof(ia[0]);
    test_reduce(Iter(ia), Iter(ia), 0);
    test_reduce(Iter(ia), Iter(ia+1), 1);
    test_reduce(Iter(ia), Iter(ia+2), 3);
    test_reduce(Iter(ia), Iter(ia+sa), 21);
}

void test_reduce()
{
    test<input_iterator<const int*> >();
    test<forward_iterator<const int*> >();
    test<bidirectional_iterator<const int*> >();
    test<random_access_iterator<const int*> >();
    test<const int*>();
}

BOOST_AUTO_TEST_CASE( test_main )
{
  test_reduce();
  test_reduce_init();
  test_reduce_init_op();
}
