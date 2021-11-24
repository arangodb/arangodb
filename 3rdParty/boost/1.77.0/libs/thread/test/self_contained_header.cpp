/*
 *             Copyright Andrey Semashev 2019.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   self_contained_header.cpp
 * \author Andrey Semashev
 * \date   2019-01-19
 *
 * \brief  This file contains a test boilerplate for checking that every public header is self-contained and does not have any missing #includes.
 */

#if defined(BOOST_THREAD_TEST_POST_WINDOWS_H)
#include <windows.h>
#endif

#define BOOST_THREAD_TEST_INCLUDE_HEADER() <boost/thread/BOOST_THREAD_TEST_HEADER>

#include BOOST_THREAD_TEST_INCLUDE_HEADER()

int main(int, char*[])
{
    return 0;
}
