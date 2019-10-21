/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   init_once_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for init_once.hpp
 */

#include <boost/winapi/init_once.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
    BOOST_WINAPI_TEST_TYPE_SIZE(INIT_ONCE);
    BOOST_WINAPI_TEST_TYPE_SIZE(PINIT_ONCE_FN); // cannot be compared is_same because has INIT_ONCE in arguments

    BOOST_WINAPI_TEST_CONSTANT(INIT_ONCE_ASYNC);
    BOOST_WINAPI_TEST_CONSTANT(INIT_ONCE_CHECK_ONLY);
    BOOST_WINAPI_TEST_CONSTANT(INIT_ONCE_INIT_FAILED);
    BOOST_WINAPI_TEST_CONSTANT(INIT_ONCE_CTX_RESERVED_BITS);
#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

    return boost::report_errors();
}
