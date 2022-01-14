/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   crypt_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for crypt.hpp
 */

#include <boost/winapi/crypt.hpp>
#include <windows.h>
#include <wincrypt.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_TYPE_SAME(HCRYPTPROV);

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
    BOOST_WINAPI_TEST_CONSTANT(PROV_RSA_FULL);

    BOOST_WINAPI_TEST_CONSTANT(CRYPT_VERIFYCONTEXT);
    BOOST_WINAPI_TEST_CONSTANT(CRYPT_NEWKEYSET);
    BOOST_WINAPI_TEST_CONSTANT(CRYPT_DELETEKEYSET);
    BOOST_WINAPI_TEST_CONSTANT(CRYPT_MACHINE_KEYSET);
    BOOST_WINAPI_TEST_CONSTANT(CRYPT_SILENT);
#endif

#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM

#if !defined(BOOST_NO_ANSI_APIS)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CryptEnumProvidersA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CryptAcquireContextA);
#endif
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CryptEnumProvidersW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CryptAcquireContextW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CryptGenRandom);

#endif // BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM

    return boost::report_errors();
}
