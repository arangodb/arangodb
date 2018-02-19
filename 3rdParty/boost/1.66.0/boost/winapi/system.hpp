/*
 * Copyright 2010 Vicente J. Botet Escriba
 * Copyright (c) Microsoft Corporation 2014
 * Copyright 2015 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_SYSTEM_HPP_INCLUDED_
#define BOOST_WINAPI_SYSTEM_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(BOOST_MSVC)
#pragma warning(push)
// nonstandard extension used : nameless struct/union
#pragma warning(disable: 4201)
#endif

#if !defined( BOOST_USE_WINDOWS_H )
extern "C" {
struct _SYSTEM_INFO;

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
GetSystemInfo(::_SYSTEM_INFO* lpSystemInfo);
#endif

#if BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WINXP
BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
GetNativeSystemInfo(::_SYSTEM_INFO* lpSystemInfo);
#endif
#endif
}
#endif

namespace boost {
namespace winapi {

typedef struct BOOST_MAY_ALIAS _SYSTEM_INFO {
    BOOST_WINAPI_DETAIL_EXTENSION union {
        DWORD_ dwOemId;
        BOOST_WINAPI_DETAIL_EXTENSION struct {
            WORD_ wProcessorArchitecture;
            WORD_ wReserved;
        };
    };
    DWORD_ dwPageSize;
    LPVOID_ lpMinimumApplicationAddress;
    LPVOID_ lpMaximumApplicationAddress;
    DWORD_PTR_ dwActiveProcessorMask;
    DWORD_ dwNumberOfProcessors;
    DWORD_ dwProcessorType;
    DWORD_ dwAllocationGranularity;
    WORD_ wProcessorLevel;
    WORD_ wProcessorRevision;
} SYSTEM_INFO_, *LPSYSTEM_INFO_;

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
BOOST_FORCEINLINE VOID_ GetSystemInfo(LPSYSTEM_INFO_ lpSystemInfo)
{
    ::GetSystemInfo(reinterpret_cast< ::_SYSTEM_INFO* >(lpSystemInfo));
}
#endif

#if BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WINXP
BOOST_FORCEINLINE VOID_ GetNativeSystemInfo(LPSYSTEM_INFO_ lpSystemInfo)
{
    ::GetNativeSystemInfo(reinterpret_cast< ::_SYSTEM_INFO* >(lpSystemInfo));
}
#endif
#endif
}
}

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

#endif // BOOST_WINAPI_SYSTEM_HPP_INCLUDED_
