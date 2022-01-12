/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          https://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   uncaught_exceptions_np.cpp
 * \author Andrey Semashev
 * \date   2018-11-10
 *
 * \brief  This file contains tests for the uncaught_exceptions function.
 *
 * This file contains checks that are compiler specific and not quite portable or require C++17.
 */

#include <boost/core/uncaught_exceptions.hpp>

#if !defined(BOOST_CORE_UNCAUGHT_EXCEPTIONS_EMULATED)

#include <boost/core/lightweight_test.hpp>

#if defined(_MSC_VER)
# pragma warning(disable: 4512) // assignment operator could not be generated
#endif

struct my_exception1 {};
struct my_exception2 {};

class exception_watcher2
{
    unsigned int& m_count;

public:
    explicit exception_watcher2(unsigned int& count) : m_count(count) {}
    ~exception_watcher2() { m_count = boost::core::uncaught_exceptions(); }
};

class exception_watcher1
{
    unsigned int& m_count1;
    unsigned int& m_count2;

public:
    exception_watcher1(unsigned int& count1, unsigned int& count2) : m_count1(count1), m_count2(count2) {}
    ~exception_watcher1()
    {
        m_count1 = boost::core::uncaught_exceptions();
        try
        {
            exception_watcher2 watcher2(m_count2);
            throw my_exception2();
        }
        catch (...)
        {
        }
    }
};

// Tests for uncaught_exceptions when used in nested destructors while an exception propagates
void test_in_nested_destructors()
{
    const unsigned int root_count = boost::core::uncaught_exceptions();

    unsigned int level1_count = root_count, level2_count = root_count;
    try
    {
        exception_watcher1 watcher1(level1_count, level2_count);
        throw my_exception1();
    }
    catch (...)
    {
    }

    BOOST_TEST_NE(root_count, level1_count);
    BOOST_TEST_NE(root_count, level2_count);
    BOOST_TEST_NE(level1_count, level2_count);
}

int main()
{
    test_in_nested_destructors();

    return boost::report_errors();
}

#else

int main()
{
    return 0;
}

#endif
