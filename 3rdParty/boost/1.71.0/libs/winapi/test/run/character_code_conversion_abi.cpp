/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   character_code_conversion_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for character_code_conversion.hpp
 */

#include <boost/winapi/character_code_conversion.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_CONSTANT(CP_ACP);
    BOOST_WINAPI_TEST_CONSTANT(CP_OEMCP);
    BOOST_WINAPI_TEST_CONSTANT(CP_MACCP);
    BOOST_WINAPI_TEST_CONSTANT(CP_THREAD_ACP);
    BOOST_WINAPI_TEST_CONSTANT(CP_SYMBOL);
    BOOST_WINAPI_TEST_CONSTANT(CP_UTF7);
    BOOST_WINAPI_TEST_CONSTANT(CP_UTF8);

    BOOST_WINAPI_TEST_CONSTANT(MB_PRECOMPOSED);
    BOOST_WINAPI_TEST_CONSTANT(MB_COMPOSITE);
    BOOST_WINAPI_TEST_CONSTANT(MB_USEGLYPHCHARS);
    BOOST_WINAPI_TEST_CONSTANT(MB_ERR_INVALID_CHARS);

    BOOST_WINAPI_TEST_CONSTANT(WC_COMPOSITECHECK);
    BOOST_WINAPI_TEST_CONSTANT(WC_DISCARDNS);
    BOOST_WINAPI_TEST_CONSTANT(WC_SEPCHARS);
    BOOST_WINAPI_TEST_CONSTANT(WC_DEFAULTCHAR);
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN2K
    BOOST_WINAPI_TEST_CONSTANT(WC_NO_BEST_FIT_CHARS);
#endif

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
    BOOST_WINAPI_TEST_CONSTANT(WC_ERR_INVALID_CHARS);
#endif

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(MultiByteToWideChar);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WideCharToMultiByte);

    return boost::report_errors();
}
