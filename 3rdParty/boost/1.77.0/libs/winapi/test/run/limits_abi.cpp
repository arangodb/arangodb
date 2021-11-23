/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   limits_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for limits.hpp
 */

#include <boost/winapi/limits.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_CONSTANT(MAX_PATH);

#if !defined(BOOST_WINAPI_IS_MINGW)
    BOOST_WINAPI_TEST_CONSTANT(UNICODE_STRING_MAX_BYTES);
    BOOST_WINAPI_TEST_CONSTANT(UNICODE_STRING_MAX_CHARS);
#endif

    return boost::report_errors();
}
