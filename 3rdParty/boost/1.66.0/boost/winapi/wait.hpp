/*
 * Copyright 2010 Vicente J. Botet Escriba
 * Copyright 2015 Andrey Semashev
 * Copyright 2017 James E. King, III
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_WAIT_HPP_INCLUDED_
#define BOOST_WINAPI_WAIT_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if !defined( BOOST_USE_WINDOWS_H )
extern "C" {

#if BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM
BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
WaitForSingleObjectEx(
    boost::winapi::HANDLE_ hHandle,
    boost::winapi::DWORD_ dwMilliseconds,
    boost::winapi::BOOL_ bAlertable);
#endif

#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4
BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
SignalObjectAndWait(
    boost::winapi::HANDLE_ hObjectToSignal,
    boost::winapi::HANDLE_ hObjectToWaitOn,
    boost::winapi::DWORD_ dwMilliseconds,
    boost::winapi::BOOL_ bAlertable);
#endif
#endif

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
WaitForSingleObject(
    boost::winapi::HANDLE_ hHandle,
    boost::winapi::DWORD_ dwMilliseconds);

BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
WaitForMultipleObjects(
    boost::winapi::DWORD_ nCount,
    boost::winapi::HANDLE_ const* lpHandles,
    boost::winapi::BOOL_ bWaitAll,
    boost::winapi::DWORD_ dwMilliseconds);

BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI
WaitForMultipleObjectsEx(
    boost::winapi::DWORD_ nCount,
    boost::winapi::HANDLE_ const* lpHandles,
    boost::winapi::BOOL_ bWaitAll,
    boost::winapi::DWORD_ dwMilliseconds,
    boost::winapi::BOOL_ bAlertable);
#endif // BOOST_WINAPI_PARTITION_APP_SYSTEM

} // extern "C"
#endif

namespace boost {
namespace winapi {

#if BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM
using ::WaitForSingleObjectEx;
#endif
#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4
using ::SignalObjectAndWait;
#endif
#endif

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
using ::WaitForMultipleObjects;
using ::WaitForMultipleObjectsEx;
using ::WaitForSingleObject;
#endif

#if defined( BOOST_USE_WINDOWS_H )

const DWORD_ INFINITE_ = INFINITE;
const DWORD_ WAIT_ABANDONED_ = WAIT_ABANDONED;
const DWORD_ WAIT_OBJECT_0_ = WAIT_OBJECT_0;
const DWORD_ WAIT_TIMEOUT_ = WAIT_TIMEOUT;
const DWORD_ WAIT_FAILED_ = WAIT_FAILED;

#else // defined( BOOST_USE_WINDOWS_H )

const DWORD_ INFINITE_ = (DWORD_)0xFFFFFFFF;
const DWORD_ WAIT_ABANDONED_ = 0x00000080L;
const DWORD_ WAIT_OBJECT_0_ = 0x00000000L;
const DWORD_ WAIT_TIMEOUT_ = 0x00000102L;
const DWORD_ WAIT_FAILED_ = (DWORD_)0xFFFFFFFF;

#endif // defined( BOOST_USE_WINDOWS_H )

const DWORD_ infinite = INFINITE_;
const DWORD_ wait_abandoned = WAIT_ABANDONED_;
const DWORD_ wait_object_0 = WAIT_OBJECT_0_;
const DWORD_ wait_timeout = WAIT_TIMEOUT_;
const DWORD_ wait_failed = WAIT_FAILED_;

const DWORD_ max_non_infinite_wait = (DWORD_)0xFFFFFFFE;

}
}

#endif // BOOST_WINAPI_WAIT_HPP_INCLUDED_
