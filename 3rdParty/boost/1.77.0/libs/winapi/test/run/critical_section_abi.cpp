/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   critical_section_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for critical_section.hpp
 */

#include <boost/winapi/critical_section.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_STRUCT(CRITICAL_SECTION, (DebugInfo)(LockCount)(RecursionCount)(OwningThread)(LockSemaphore)(SpinCount));

    return boost::report_errors();
}
