/*
 * Copyright 2010 Vicente J. Botet Escriba
 * Copyright 2015 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_CRITICAL_SECTION_HPP_INCLUDED_
#define BOOST_WINAPI_CRITICAL_SECTION_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/detail/cast_ptr.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if !defined( BOOST_USE_WINDOWS_H )

extern "C" {
#if !defined( BOOST_WINAPI_IS_MINGW )
struct _RTL_CRITICAL_SECTION;

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
InitializeCriticalSection(::_RTL_CRITICAL_SECTION* lpCriticalSection);
#endif

BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
EnterCriticalSection(::_RTL_CRITICAL_SECTION* lpCriticalSection);

BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
LeaveCriticalSection(::_RTL_CRITICAL_SECTION* lpCriticalSection);

#if BOOST_USE_WINAPI_VERSION >= 0x0403
#if BOOST_WINAPI_PARTITION_APP_SYSTEM
BOOST_SYMBOL_IMPORT boost::winapi::BOOL_ WINAPI
InitializeCriticalSectionAndSpinCount(
    ::_RTL_CRITICAL_SECTION* lpCriticalSection,
    boost::winapi::DWORD_ dwSpinCount);

BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
SetCriticalSectionSpinCount(
    ::_RTL_CRITICAL_SECTION* lpCriticalSection,
    boost::winapi::DWORD_ dwSpinCount);
#endif

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
BOOST_SYMBOL_IMPORT boost::winapi::BOOL_ WINAPI
InitializeCriticalSectionEx(
    ::_RTL_CRITICAL_SECTION* lpCriticalSection,
    boost::winapi::DWORD_ dwSpinCount,
    boost::winapi::DWORD_ Flags);
#endif
#endif

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4
BOOST_SYMBOL_IMPORT boost::winapi::BOOL_ WINAPI
TryEnterCriticalSection(::_RTL_CRITICAL_SECTION* lpCriticalSection);
#endif

BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
DeleteCriticalSection(::_RTL_CRITICAL_SECTION* lpCriticalSection);

#else // defined( BOOST_WINAPI_IS_MINGW )

// MinGW uses a different name for the structure
struct _CRITICAL_SECTION;

BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
InitializeCriticalSection(::_CRITICAL_SECTION* lpCriticalSection);

BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
EnterCriticalSection(::_CRITICAL_SECTION* lpCriticalSection);

BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
LeaveCriticalSection(::_CRITICAL_SECTION* lpCriticalSection);

#if BOOST_USE_WINAPI_VERSION >= 0x0403
BOOST_SYMBOL_IMPORT boost::winapi::BOOL_ WINAPI
InitializeCriticalSectionAndSpinCount(
    ::_CRITICAL_SECTION* lpCriticalSection,
    boost::winapi::DWORD_ dwSpinCount);

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
BOOST_SYMBOL_IMPORT boost::winapi::BOOL_ WINAPI
InitializeCriticalSectionEx(
    ::_CRITICAL_SECTION* lpCriticalSection,
    boost::winapi::DWORD_ dwSpinCount,
    boost::winapi::DWORD_ Flags);
#endif

BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
SetCriticalSectionSpinCount(
    ::_CRITICAL_SECTION* lpCriticalSection,
    boost::winapi::DWORD_ dwSpinCount);
#endif

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4
BOOST_SYMBOL_IMPORT boost::winapi::BOOL_ WINAPI
TryEnterCriticalSection(::_CRITICAL_SECTION* lpCriticalSection);
#endif

BOOST_SYMBOL_IMPORT boost::winapi::VOID_ WINAPI
DeleteCriticalSection(::_CRITICAL_SECTION* lpCriticalSection);

#endif // defined( BOOST_WINAPI_IS_MINGW )
} // extern "C"
#endif

namespace boost {
namespace winapi {

struct _RTL_CRITICAL_SECTION_DEBUG;

#pragma pack(push, 8)

typedef struct BOOST_MAY_ALIAS _RTL_CRITICAL_SECTION {
    _RTL_CRITICAL_SECTION_DEBUG* DebugInfo;
    LONG_ LockCount;
    LONG_ RecursionCount;
    HANDLE_ OwningThread;
    HANDLE_ LockSemaphore;
    ULONG_PTR_ SpinCount;
} CRITICAL_SECTION_, *PCRITICAL_SECTION_;

#pragma pack(pop)

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
BOOST_FORCEINLINE VOID_ InitializeCriticalSection(CRITICAL_SECTION_* lpCriticalSection)
{
    ::InitializeCriticalSection(winapi::detail::cast_ptr(lpCriticalSection));
}
#endif

BOOST_FORCEINLINE VOID_ EnterCriticalSection(CRITICAL_SECTION_* lpCriticalSection)
{
    ::EnterCriticalSection(winapi::detail::cast_ptr(lpCriticalSection));
}

BOOST_FORCEINLINE VOID_ LeaveCriticalSection(CRITICAL_SECTION_* lpCriticalSection)
{
    ::LeaveCriticalSection(winapi::detail::cast_ptr(lpCriticalSection));
}

#if BOOST_USE_WINAPI_VERSION >= 0x0403
#if BOOST_WINAPI_PARTITION_APP_SYSTEM
BOOST_FORCEINLINE BOOL_ InitializeCriticalSectionAndSpinCount(CRITICAL_SECTION_* lpCriticalSection, DWORD_ dwSpinCount)
{
    return ::InitializeCriticalSectionAndSpinCount(winapi::detail::cast_ptr(lpCriticalSection), dwSpinCount);
}

BOOST_FORCEINLINE DWORD_ SetCriticalSectionSpinCount(CRITICAL_SECTION_* lpCriticalSection, DWORD_ dwSpinCount)
{
    return ::SetCriticalSectionSpinCount(winapi::detail::cast_ptr(lpCriticalSection), dwSpinCount);
}
#endif

// CRITICAL_SECTION_NO_DEBUG_INFO is defined for WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
const DWORD_ CRITICAL_SECTION_NO_DEBUG_INFO_ = 0x01000000;
const DWORD_ CRITICAL_SECTION_FLAG_NO_DEBUG_INFO_ = CRITICAL_SECTION_NO_DEBUG_INFO_;
const DWORD_ CRITICAL_SECTION_FLAG_DYNAMIC_SPIN_ = 0x02000000; // undocumented
const DWORD_ CRITICAL_SECTION_FLAG_STATIC_INIT_ = 0x04000000; // undocumented

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
BOOST_FORCEINLINE BOOL_ InitializeCriticalSectionEx(CRITICAL_SECTION_* lpCriticalSection, DWORD_ dwSpinCount, DWORD_ Flags)
{
    return ::InitializeCriticalSectionEx(winapi::detail::cast_ptr(lpCriticalSection), dwSpinCount, Flags);
}
#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
#endif // BOOST_USE_WINAPI_VERSION >= 0x0403

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4
BOOST_FORCEINLINE BOOL_ TryEnterCriticalSection(CRITICAL_SECTION_* lpCriticalSection)
{
    return ::TryEnterCriticalSection(winapi::detail::cast_ptr(lpCriticalSection));
}
#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4

BOOST_FORCEINLINE VOID_ DeleteCriticalSection(CRITICAL_SECTION_* lpCriticalSection)
{
    ::DeleteCriticalSection(winapi::detail::cast_ptr(lpCriticalSection));
}

}
}

#endif // BOOST_WINAPI_CRITICAL_SECTION_HPP_INCLUDED_
