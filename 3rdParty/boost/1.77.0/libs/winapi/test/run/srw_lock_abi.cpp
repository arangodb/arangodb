/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   srw_lock_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for srw_lock.hpp
 */

#include <boost/winapi/srw_lock.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

    BOOST_WINAPI_TEST_TYPE_SIZE(SRWLOCK);

#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

    return boost::report_errors();
}
