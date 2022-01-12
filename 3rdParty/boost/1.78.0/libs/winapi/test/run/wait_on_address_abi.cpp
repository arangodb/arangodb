/*
 *             Copyright Andrey Semashev 2020.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   wait_on_address_abi.cpp
 * \author Andrey Semashev
 * \date   07.06.2020
 *
 * \brief  This file contains ABI test for wait_on_address.hpp
 */

#include <boost/winapi/wait_on_address.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN8 && (BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WaitOnAddress);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WakeByAddressSingle);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WakeByAddressAll);
#endif

    return boost::report_errors();
}
