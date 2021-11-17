/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   condition_variable_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for condition_variable.hpp
 */

#include <boost/winapi/condition_variable.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

    BOOST_WINAPI_TEST_CONSTANT(CONDITION_VARIABLE_LOCKMODE_SHARED);

    BOOST_WINAPI_TEST_TYPE_SIZE(CONDITION_VARIABLE);

#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

    return boost::report_errors();
}
