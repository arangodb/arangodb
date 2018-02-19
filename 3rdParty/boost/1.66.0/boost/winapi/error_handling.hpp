/*
 * Copyright 2010 Vicente J. Botet Escriba
 * Copyright 2015 Andrey Semashev
 * Copyright 2016 Jorge Lodos
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_ERROR_HANDLING_HPP_INCLUDED_
#define BOOST_WINAPI_ERROR_HANDLING_HPP_INCLUDED_

#include <stdarg.h>
#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/get_last_error.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if !defined( BOOST_USE_WINDOWS_H )
extern "C" {
#if !defined( BOOST_NO_ANSI_APIS )
BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
FormatMessageA(
    boost::winapi::DWORD_ dwFlags,
    boost::winapi::LPCVOID_ lpSource,
    boost::winapi::DWORD_ dwMessageId,
    boost::winapi::DWORD_ dwLanguageId,
    boost::winapi::LPSTR_ lpBuffer,
    boost::winapi::DWORD_ nSize,
    va_list *Arguments);
#endif

BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
FormatMessageW(
    boost::winapi::DWORD_ dwFlags,
    boost::winapi::LPCVOID_ lpSource,
    boost::winapi::DWORD_ dwMessageId,
    boost::winapi::DWORD_ dwLanguageId,
    boost::winapi::LPWSTR_ lpBuffer,
    boost::winapi::DWORD_ nSize,
    va_list *Arguments);

#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM
BOOST_SYMBOL_IMPORT boost::winapi::UINT_ WINAPI
SetErrorMode(boost::winapi::UINT_ uMode);
#endif
} // extern "C"
#endif

namespace boost {
namespace winapi {

#if defined( BOOST_USE_WINDOWS_H )

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
const DWORD_ FORMAT_MESSAGE_ALLOCATE_BUFFER_= FORMAT_MESSAGE_ALLOCATE_BUFFER;
#endif

const DWORD_ FORMAT_MESSAGE_IGNORE_INSERTS_=  FORMAT_MESSAGE_IGNORE_INSERTS;
const DWORD_ FORMAT_MESSAGE_FROM_STRING_=     FORMAT_MESSAGE_FROM_STRING;
const DWORD_ FORMAT_MESSAGE_FROM_HMODULE_=    FORMAT_MESSAGE_FROM_HMODULE;
const DWORD_ FORMAT_MESSAGE_FROM_SYSTEM_=     FORMAT_MESSAGE_FROM_SYSTEM;
const DWORD_ FORMAT_MESSAGE_ARGUMENT_ARRAY_=  FORMAT_MESSAGE_ARGUMENT_ARRAY;
const DWORD_ FORMAT_MESSAGE_MAX_WIDTH_MASK_=  FORMAT_MESSAGE_MAX_WIDTH_MASK;

const WORD_ LANG_NEUTRAL_=                  LANG_NEUTRAL;
const WORD_ LANG_INVARIANT_=                LANG_INVARIANT;

const WORD_ SUBLANG_DEFAULT_=               SUBLANG_DEFAULT;    // user default

BOOST_FORCEINLINE BOOST_CONSTEXPR WORD_ MAKELANGID_(WORD_ p, WORD_ s) BOOST_NOEXCEPT
{
    return MAKELANGID(p,s);
}

#if BOOST_WINAPI_PARTITION_DESKTOP
const DWORD_ SEM_FAILCRITICALERRORS_ =     SEM_FAILCRITICALERRORS;
const DWORD_ SEM_NOGPFAULTERRORBOX_ =      SEM_NOGPFAULTERRORBOX;
const DWORD_ SEM_NOALIGNMENTFAULTEXCEPT_ = SEM_NOALIGNMENTFAULTEXCEPT;
const DWORD_ SEM_NOOPENFILEERRORBOX_ =     SEM_NOOPENFILEERRORBOX;
#endif

#else

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
const DWORD_ FORMAT_MESSAGE_ALLOCATE_BUFFER_= 0x00000100;
#endif
const DWORD_ FORMAT_MESSAGE_IGNORE_INSERTS_=  0x00000200;
const DWORD_ FORMAT_MESSAGE_FROM_STRING_=     0x00000400;
const DWORD_ FORMAT_MESSAGE_FROM_HMODULE_=    0x00000800;
const DWORD_ FORMAT_MESSAGE_FROM_SYSTEM_=     0x00001000;
const DWORD_ FORMAT_MESSAGE_ARGUMENT_ARRAY_=  0x00002000;
const DWORD_ FORMAT_MESSAGE_MAX_WIDTH_MASK_=  0x000000FF;

const WORD_ LANG_NEUTRAL_=                  0x00;
const WORD_ LANG_INVARIANT_=                0x7f;

const WORD_ SUBLANG_DEFAULT_=               0x01;    // user default

BOOST_FORCEINLINE BOOST_CONSTEXPR WORD_ MAKELANGID_(WORD_ p, WORD_ s) BOOST_NOEXCEPT
{
    return (WORD_)((((WORD_)(s)) << 10) | (WORD_)(p));
}

#if BOOST_WINAPI_PARTITION_DESKTOP
const DWORD_ SEM_FAILCRITICALERRORS_ =     0x0001;
const DWORD_ SEM_NOGPFAULTERRORBOX_ =      0x0002;
const DWORD_ SEM_NOALIGNMENTFAULTEXCEPT_ = 0x0004;
const DWORD_ SEM_NOOPENFILEERRORBOX_ =     0x8000;
#endif

#endif

#if !defined( BOOST_NO_ANSI_APIS )
using ::FormatMessageA;
#endif
using ::FormatMessageW;
#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM
using ::SetErrorMode;
#endif

#if !defined( BOOST_NO_ANSI_APIS )
BOOST_FORCEINLINE DWORD_ format_message(
    DWORD_ dwFlags,
    LPCVOID_ lpSource,
    DWORD_ dwMessageId,
    DWORD_ dwLanguageId,
    LPSTR_ lpBuffer,
    DWORD_ nSize,
    va_list *Arguments)
{
    return ::FormatMessageA(dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize, Arguments);
}
#endif

BOOST_FORCEINLINE DWORD_ format_message(
    DWORD_ dwFlags,
    LPCVOID_ lpSource,
    DWORD_ dwMessageId,
    DWORD_ dwLanguageId,
    LPWSTR_ lpBuffer,
    DWORD_ nSize,
    va_list *Arguments)
{
    return ::FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize, Arguments);
}

}
}

#endif // BOOST_WINAPI_ERROR_HANDLING_HPP_INCLUDED_
