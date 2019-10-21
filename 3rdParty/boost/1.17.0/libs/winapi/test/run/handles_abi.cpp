/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   handles_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for handles.hpp
 */

#include <boost/winapi/handles.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_CONSTANT(DUPLICATE_CLOSE_SOURCE);
    BOOST_WINAPI_TEST_CONSTANT(DUPLICATE_SAME_ACCESS);
    BOOST_WINAPI_TEST_CONSTANT(INVALID_HANDLE_VALUE);

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CloseHandle);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(DuplicateHandle);

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN10
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CompareObjectHandles);
#endif

    return boost::report_errors();
}
