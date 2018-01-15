/*
 * Copyright 2016 Klemens D. Morgenstern
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_OVERLAPPED_HPP_INCLUDED_
#define BOOST_WINAPI_OVERLAPPED_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if !defined( BOOST_USE_WINDOWS_H )
extern "C" {
struct _OVERLAPPED;
}
#endif

namespace boost {
namespace winapi {

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#endif

typedef struct BOOST_MAY_ALIAS _OVERLAPPED {
    ULONG_PTR_ Internal;
    ULONG_PTR_ InternalHigh;
    union {
        struct {
            DWORD_ Offset;
            DWORD_ OffsetHigh;
        };
        PVOID_  Pointer;
    };
    HANDLE_    hEvent;
} OVERLAPPED_, *LPOVERLAPPED_;

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

} // namespace winapi
} // namespace boost

#endif // BOOST_WINAPI_OVERLAPPED_HPP_INCLUDED_
