/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          https://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   uncaught_exceptions.cpp
 * \author Andrey Semashev
 * \date   2018-11-10
 *
 * \brief  This file contains tests for the uncaught_exceptions function.
 *
 * This file only contains the very basic checks of functionality that can be portably achieved
 * through C++03 std::uncaught_exception.
 */

#include <boost/core/uncaught_exceptions.hpp>
#include <boost/core/lightweight_test.hpp>

struct my_exception {};

class exception_watcher
{
    unsigned int& m_count;

public:
    explicit exception_watcher(unsigned int& count) : m_count(count) {}
    ~exception_watcher() { m_count = boost::core::uncaught_exceptions(); }
};

// Tests for uncaught_exceptions when used in a destructor while an exception propagates
void test_in_destructor()
{
    const unsigned int root_count = boost::core::uncaught_exceptions();

    unsigned int level1_count = root_count;
    try
    {
        exception_watcher watcher(level1_count);
        throw my_exception();
    }
    catch (...)
    {
    }

    BOOST_TEST_NE(root_count, level1_count);
}

int main()
{
    test_in_destructor();

    return boost::report_errors();
}
