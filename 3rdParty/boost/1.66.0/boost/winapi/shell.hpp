/*
 * Copyright 2016 Klemens D. Morgenstern
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_SHELL_HPP_INCLUDED_
#define BOOST_WINAPI_SHELL_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/limits.hpp>
#if defined( BOOST_USE_WINDOWS_H )
#include <shellapi.h>
#endif

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if BOOST_WINAPI_PARTITION_DESKTOP

#if !defined( BOOST_USE_WINDOWS_H )
extern "C" {

BOOST_WINAPI_DETAIL_DECLARE_HANDLE(HICON);

#if !defined( BOOST_NO_ANSI_APIS )
struct _SHFILEINFOA;
#endif
struct _SHFILEINFOW;

#if !defined( BOOST_NO_ANSI_APIS )
BOOST_SYMBOL_IMPORT boost::winapi::DWORD_PTR_ WINAPI SHGetFileInfoA(
    boost::winapi::LPCSTR_ pszPath,
    boost::winapi::DWORD_ dwFileAttributes,
    ::_SHFILEINFOA *psfinsigned,
    boost::winapi::UINT_ cbFileInfons,
    boost::winapi::UINT_ uFlags);
#endif

BOOST_SYMBOL_IMPORT boost::winapi::DWORD_PTR_ WINAPI SHGetFileInfoW(
    boost::winapi::LPCWSTR_ pszPath,
    boost::winapi::DWORD_ dwFileAttributes,
    ::_SHFILEINFOW *psfinsigned,
    boost::winapi::UINT_ cbFileInfons,
    boost::winapi::UINT_ uFlags);

} // extern "C"
#endif // !defined( BOOST_USE_WINDOWS_H )

namespace boost {
namespace winapi {

typedef ::HICON HICON_;

#if defined( BOOST_USE_WINDOWS_H )

const DWORD_ SHGFI_ICON_              = SHGFI_ICON;
const DWORD_ SHGFI_DISPLAYNAME_       = SHGFI_DISPLAYNAME;
const DWORD_ SHGFI_TYPENAME_          = SHGFI_TYPENAME;
const DWORD_ SHGFI_ATTRIBUTES_        = SHGFI_ATTRIBUTES;
const DWORD_ SHGFI_ICONLOCATION_      = SHGFI_ICONLOCATION;
const DWORD_ SHGFI_EXETYPE_           = SHGFI_EXETYPE;
const DWORD_ SHGFI_SYSICONINDEX_      = SHGFI_SYSICONINDEX;
const DWORD_ SHGFI_LINKOVERLAY_       = SHGFI_LINKOVERLAY;
const DWORD_ SHGFI_SELECTED_          = SHGFI_SELECTED;
#if (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN2K)
const DWORD_ SHGFI_ATTR_SPECIFIED_    = SHGFI_ATTR_SPECIFIED;
#endif
const DWORD_ SHGFI_LARGEICON_         = SHGFI_LARGEICON;
const DWORD_ SHGFI_SMALLICON_         = SHGFI_SMALLICON;
const DWORD_ SHGFI_OPENICON_          = SHGFI_OPENICON;
const DWORD_ SHGFI_SHELLICONSIZE_     = SHGFI_SHELLICONSIZE;
const DWORD_ SHGFI_PIDL_              = SHGFI_PIDL;
const DWORD_ SHGFI_USEFILEATTRIBUTES_ = SHGFI_USEFILEATTRIBUTES;

#else // defined( BOOST_USE_WINDOWS_H )

const DWORD_ SHGFI_ICON_              = 0x000000100;
const DWORD_ SHGFI_DISPLAYNAME_       = 0x000000200;
const DWORD_ SHGFI_TYPENAME_          = 0x000000400;
const DWORD_ SHGFI_ATTRIBUTES_        = 0x000000800;
const DWORD_ SHGFI_ICONLOCATION_      = 0x000001000;
const DWORD_ SHGFI_EXETYPE_           = 0x000002000;
const DWORD_ SHGFI_SYSICONINDEX_      = 0x000004000;
const DWORD_ SHGFI_LINKOVERLAY_       = 0x000008000;
const DWORD_ SHGFI_SELECTED_          = 0x000010000;
#if (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN2K)
const DWORD_ SHGFI_ATTR_SPECIFIED_    = 0x000020000;
#endif
const DWORD_ SHGFI_LARGEICON_         = 0x000000000;
const DWORD_ SHGFI_SMALLICON_         = 0x000000001;
const DWORD_ SHGFI_OPENICON_          = 0x000000002;
const DWORD_ SHGFI_SHELLICONSIZE_     = 0x000000004;
const DWORD_ SHGFI_PIDL_              = 0x000000008;
const DWORD_ SHGFI_USEFILEATTRIBUTES_ = 0x000000010;

#endif // defined( BOOST_USE_WINDOWS_H )

// These constants are only declared for _WIN32_IE >= 0x0500. We don't set IE version
// and 5.0 is the default version since NT4 SP6, so just define the constants unconditionally.
// They are also missing from MinGW.
const DWORD_ SHGFI_ADDOVERLAYS_       = 0x000000020;
const DWORD_ SHGFI_OVERLAYINDEX_      = 0x000000040;

typedef struct BOOST_MAY_ALIAS _SHFILEINFOA {
    HICON_ hIcon;
    int iIcon;
    DWORD_ dwAttributes;
    CHAR_ szDisplayName[MAX_PATH_];
    CHAR_ szTypeName[80];
} SHFILEINFOA_;

typedef struct BOOST_MAY_ALIAS _SHFILEINFOW {
    HICON_ hIcon;
    int iIcon;
    DWORD_ dwAttributes;
    WCHAR_ szDisplayName[MAX_PATH_];
    WCHAR_ szTypeName[80];
} SHFILEINFOW_;

#if !defined( BOOST_NO_ANSI_APIS )

BOOST_FORCEINLINE DWORD_PTR_ SHGetFileInfoA(LPCSTR_ pszPath, DWORD_ dwFileAttributes, SHFILEINFOA_* psfinsigned, UINT_ cbFileInfons, UINT_ uFlags)
{
    return ::SHGetFileInfoA(pszPath, dwFileAttributes, reinterpret_cast< ::_SHFILEINFOA* >(psfinsigned), cbFileInfons, uFlags);
}

BOOST_FORCEINLINE DWORD_PTR_ sh_get_file_info(LPCSTR_ pszPath, DWORD_ dwFileAttributes, SHFILEINFOA_* psfinsigned, UINT_ cbFileInfons, UINT_ uFlags)
{
    return ::SHGetFileInfoA(pszPath, dwFileAttributes, reinterpret_cast< ::_SHFILEINFOA* >(psfinsigned), cbFileInfons, uFlags);
}

#endif // BOOST_NO_ANSI_APIS

BOOST_FORCEINLINE DWORD_PTR_ SHGetFileInfoW(LPCWSTR_ pszPath, DWORD_ dwFileAttributes, SHFILEINFOW_* psfinsigned, UINT_ cbFileInfons, UINT_ uFlags)
{
    return ::SHGetFileInfoW(pszPath, dwFileAttributes, reinterpret_cast< ::_SHFILEINFOW* >(psfinsigned), cbFileInfons, uFlags);
}

BOOST_FORCEINLINE DWORD_PTR_ sh_get_file_info(LPCWSTR_ pszPath, DWORD_ dwFileAttributes, SHFILEINFOW_* psfinsigned, UINT_ cbFileInfons, UINT_ uFlags)
{
    return ::SHGetFileInfoW(pszPath, dwFileAttributes, reinterpret_cast< ::_SHFILEINFOW* >(psfinsigned), cbFileInfons, uFlags);
}

}
}

#endif // BOOST_WINAPI_PARTITION_DESKTOP
#endif // BOOST_WINAPI_SHELL_HPP_INCLUDED_
