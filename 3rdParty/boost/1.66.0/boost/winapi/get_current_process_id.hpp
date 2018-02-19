/*
 * Copyright 2010 Vicente J. Botet Escriba
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_GET_CURRENT_PROCESS_ID_HPP_INCLUDED_
#define BOOST_WINAPI_GET_CURRENT_PROCESS_ID_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

// Windows CE define GetCurrentProcessId as an inline function in kfuncs.h
#if !defined( BOOST_USE_WINDOWS_H ) && !defined( UNDER_CE )
extern "C" {
BOOST_SYMBOL_IMPORT boost::winapi::DWORD_ WINAPI GetCurrentProcessId(BOOST_WINAPI_DETAIL_VOID);
}
#endif

namespace boost {
namespace winapi {
using ::GetCurrentProcessId;
}
}

#endif // BOOST_WINAPI_GET_CURRENT_PROCESS_ID_HPP_INCLUDED_
