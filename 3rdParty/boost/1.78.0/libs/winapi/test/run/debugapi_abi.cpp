/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   debugapi_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for debugapi.hpp
 */

#include <boost/winapi/debugapi.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(IsDebuggerPresent);
#endif

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OutputDebugStringA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OutputDebugStringW);

    return boost::report_errors();
}
