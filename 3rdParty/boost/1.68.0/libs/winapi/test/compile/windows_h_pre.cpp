/*
 *             Copyright Andrey Semashev 2015.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   windows_h_header.cpp
 * \author Andrey Semashev
 * \date   21.06.2015
 *
 * \brief  This file contains a test boilerplate for checking that every public header does not conflict with native windows.h pre-included.
 */

#include <windows.h>

#define BOOST_WINAPI_TEST_INCLUDE_HEADER() <boost/BOOST_WINAPI_TEST_HEADER>

#include BOOST_WINAPI_TEST_INCLUDE_HEADER()

int main(int, char*[])
{
    return 0;
}
