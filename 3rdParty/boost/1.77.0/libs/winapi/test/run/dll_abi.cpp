/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   dll_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for dll.hpp
 */

#include <boost/winapi/dll.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM

    BOOST_WINAPI_TEST_TYPE_SAME(FARPROC);
    BOOST_WINAPI_TEST_TYPE_SAME(NEARPROC);
    BOOST_WINAPI_TEST_TYPE_SAME(PROC);

    BOOST_WINAPI_TEST_CONSTANT(DONT_RESOLVE_DLL_REFERENCES);
    BOOST_WINAPI_TEST_CONSTANT(LOAD_WITH_ALTERED_SEARCH_PATH);

#if defined(LOAD_IGNORE_CODE_AUTHZ_LEVEL)
    // This one is not defined by MinGW
    BOOST_WINAPI_TEST_CONSTANT(LOAD_IGNORE_CODE_AUTHZ_LEVEL);
#endif

#if !defined(BOOST_NO_ANSI_APIS)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(LoadLibraryA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(LoadLibraryExA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetModuleHandleA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetModuleFileNameA);
#endif // !defined(BOOST_NO_ANSI_APIS)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(LoadLibraryW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(LoadLibraryExW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetModuleHandleW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetModuleFileNameW);

#if BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(FreeLibrary);
#endif

#if !defined(UNDER_CE)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetProcAddress);
#else
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetProcAddressA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetProcAddressW);
#endif

    BOOST_WINAPI_TEST_STRUCT(MEMORY_BASIC_INFORMATION, (BaseAddress)(AllocationBase)(AllocationProtect)(RegionSize)(State)(Protect)(Type));

#endif // BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM

    return boost::report_errors();
}
