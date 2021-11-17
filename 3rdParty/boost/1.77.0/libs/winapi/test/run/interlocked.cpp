/*
 *             Copyright Andrey Semashev 2019.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   interlocked.cpp
 * \author Andrey Semashev
 * \date   10.07.2019
 *
 * \brief  This file contains test for interlocked.hpp
 */

#include <boost/detail/interlocked.hpp>
#include <windows.h> // include to test there are no conflicts with Windows SDK
#include <boost/cstdint.hpp>
#include <boost/core/lightweight_test.hpp>

void test_int32()
{
    boost::int32_t n = 0, r = 0;

    r = BOOST_INTERLOCKED_INCREMENT(&n);
    BOOST_TEST_EQ(n, 1);
    BOOST_TEST_EQ(r, 1);
    r = BOOST_INTERLOCKED_DECREMENT(&n);
    BOOST_TEST_EQ(n, 0);
    BOOST_TEST_EQ(r, 0);
    r = BOOST_INTERLOCKED_COMPARE_EXCHANGE(&n, 2, 0);
    BOOST_TEST_EQ(n, 2);
    BOOST_TEST_EQ(r, 0);
    r = BOOST_INTERLOCKED_EXCHANGE(&n, 3);
    BOOST_TEST_EQ(n, 3);
    BOOST_TEST_EQ(r, 2);
    r = BOOST_INTERLOCKED_EXCHANGE_ADD(&n, 10);
    BOOST_TEST_EQ(n, 13);
    BOOST_TEST_EQ(r, 3);
}

void test_pointer()
{
    void* p = 0;
    void* q = 0;

    q = BOOST_INTERLOCKED_COMPARE_EXCHANGE_POINTER(&p, (void*)&q, (void*)0);
    BOOST_TEST_EQ(p, (void*)&q);
    BOOST_TEST_EQ(q, (void*)0);
    q = BOOST_INTERLOCKED_EXCHANGE_POINTER(&p, (void*)0);
    BOOST_TEST_EQ(p, (void*)0);
    BOOST_TEST_EQ(q, (void*)&q);
}

int main()
{
    test_int32();
    test_pointer();

    return boost::report_errors();
}
