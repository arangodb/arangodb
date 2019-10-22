/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   stack_backtrace_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for stack_backtrace.hpp
 */

#include <boost/winapi/stack_backtrace.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
// MinGW does not provide RtlCaptureStackBackTrace
#if !defined(BOOST_WINAPI_IS_MINGW)
// Note: RtlCaptureStackBackTrace is available in WinXP SP1 and later
#if (BOOST_USE_NTDDI_VERSION > BOOST_WINAPI_NTDDI_WINXP)
#if BOOST_WINAPI_PARTITION_APP_SYSTEM

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(RtlCaptureStackBackTrace);

#endif // BOOST_WINAPI_PARTITION_APP_SYSTEM
#endif // (BOOST_USE_NTDDI_VERSION > BOOST_WINAPI_NTDDI_WINXP)
#endif // !defined(BOOST_WINAPI_IS_MINGW)

    return boost::report_errors();
}
