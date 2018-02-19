/*
 * Copyright 2016 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_PAGE_PROTECTION_FLAGS_HPP_INCLUDED_
#define BOOST_WINAPI_PAGE_PROTECTION_FLAGS_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace winapi {

#if defined( BOOST_USE_WINDOWS_H )

const DWORD_ PAGE_NOACCESS_ = PAGE_NOACCESS;
const DWORD_ PAGE_READONLY_ = PAGE_READONLY;
const DWORD_ PAGE_READWRITE_ = PAGE_READWRITE;
const DWORD_ PAGE_WRITECOPY_ = PAGE_WRITECOPY;
const DWORD_ PAGE_GUARD_ = PAGE_GUARD;
const DWORD_ PAGE_NOCACHE_ = PAGE_NOCACHE;
const DWORD_ PAGE_WRITECOMBINE_ = PAGE_WRITECOMBINE;

#else // defined( BOOST_USE_WINDOWS_H )

const DWORD_ PAGE_NOACCESS_ = 0x01;
const DWORD_ PAGE_READONLY_ = 0x02;
const DWORD_ PAGE_READWRITE_ = 0x04;
const DWORD_ PAGE_WRITECOPY_ = 0x08;
const DWORD_ PAGE_GUARD_ = 0x100;
const DWORD_ PAGE_NOCACHE_ = 0x200;
const DWORD_ PAGE_WRITECOMBINE_ = 0x400;

#endif // defined( BOOST_USE_WINDOWS_H )

// The PAGE_EXECUTE flags were put into the DESKTOP UWP partition in Windows SDK 8.0 and 8.1 
// and then moved to be globally available again in SDK 10.0.  Due to this, we simply define 
// them unconditionally to work around the issue and consider the 8.x SDKs in error.

const DWORD_ PAGE_EXECUTE_ = 0x10;
const DWORD_ PAGE_EXECUTE_READ_ = 0x20;
const DWORD_ PAGE_EXECUTE_READWRITE_ = 0x40;
const DWORD_ PAGE_EXECUTE_WRITECOPY_ = 0x80;

}
}

#endif // BOOST_WINAPI_PAGE_PROTECTION_FLAGS_HPP_INCLUDED_
