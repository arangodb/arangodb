/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   handle_info_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for handle_info.hpp
 */

#include <boost/winapi/handle_info.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_DESKTOP
    BOOST_WINAPI_TEST_CONSTANT(HANDLE_FLAG_INHERIT);
    BOOST_WINAPI_TEST_CONSTANT(HANDLE_FLAG_PROTECT_FROM_CLOSE);

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetHandleInformation);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SetHandleInformation);
#endif // BOOST_WINAPI_PARTITION_DESKTOP

    return boost::report_errors();
}
