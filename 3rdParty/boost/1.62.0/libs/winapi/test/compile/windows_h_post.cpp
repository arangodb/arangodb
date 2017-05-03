/*
 *             Copyright Andrey Semashev 2015.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   windows_h_header.cpp
 * \author Andrey Semashev
 * \date   21.06.2015
 *
 * \brief  This file contains a test boilerplate for checking that every public header does not conflict with native windows.h being post-included.
 */

// NOTE: Use Boost.Predef, the most fine graned header to detect Windows. This header should not include anything
//       system- or STL-specific so that the check for Boost.WinAPI header self-sufficiency is not subverted.
//       Boost.Config does not satisfy this requirement.
#include <boost/predef/os/windows.h>

#if BOOST_OS_WINDOWS

#define BOOST_WINAPI_TEST_INCLUDE_HEADER() <boost/detail/BOOST_WINAPI_TEST_HEADER>

#include BOOST_WINAPI_TEST_INCLUDE_HEADER()

#include <windows.h>

#endif // BOOST_OS_WINDOWS

int main(int, char*[])
{
    return 0;
}
