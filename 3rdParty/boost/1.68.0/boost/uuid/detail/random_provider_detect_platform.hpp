//
// Copyright (c) 2017 James E. King III
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//   http://www.boost.org/LICENCE_1_0.txt)
//
// Platform-specific random entropy provider platform detection
//

#ifndef BOOST_UUID_DETAIL_RANDOM_PROVIDER_PLATFORM_DETECTION_HPP
#define BOOST_UUID_DETAIL_RANDOM_PROVIDER_PLATFORM_DETECTION_HPP

#include <boost/predef/library/c/cloudabi.h>
#include <boost/predef/library/c/gnu.h>
#include <boost/predef/os/bsd/open.h>
#include <boost/predef/os/linux.h>
#include <boost/predef/os/windows.h>
#if BOOST_OS_LINUX
#include <sys/syscall.h>
#endif

//
// Platform Detection - will load in the correct header and
// will define the class <tt>random_provider_base</tt>.
//

#if BOOST_OS_BSD_OPEN >= BOOST_VERSION_NUMBER(2, 1, 0) || BOOST_LIB_C_CLOUDABI
# define BOOST_UUID_RANDOM_PROVIDER_ARC4RANDOM
# define BOOST_UUID_RANDOM_PROVIDER_NAME arc4random

#elif BOOST_OS_WINDOWS
# include <boost/winapi/config.hpp>
# if BOOST_WINAPI_PARTITION_APP_SYSTEM && \
     !defined(BOOST_UUID_RANDOM_PROVIDER_FORCE_WINCRYPT) && \
     !defined(_WIN32_WCE) && \
     (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6)
#  define BOOST_UUID_RANDOM_PROVIDER_BCRYPT
#  define BOOST_UUID_RANDOM_PROVIDER_NAME bcrypt

# elif BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM
#  define BOOST_UUID_RANDOM_PROVIDER_WINCRYPT
#  define BOOST_UUID_RANDOM_PROVIDER_NAME wincrypt
# else
#  error Unable to find a suitable windows entropy provider
# endif

#elif BOOST_OS_LINUX && defined(SYS_getrandom) && !defined(BOOST_UUID_RANDOM_PROVIDER_FORCE_POSIX) && !defined(BOOST_UUID_RANDOM_PROVIDER_DISABLE_GETRANDOM)
# define BOOST_UUID_RANDOM_PROVIDER_GETRANDOM
# define BOOST_UUID_RANDOM_PROVIDER_NAME getrandom

#elif BOOST_LIB_C_GNU >= BOOST_VERSION_NUMBER(2, 25, 0) && !defined(BOOST_UUID_RANDOM_PROVIDER_FORCE_POSIX)
# define BOOST_UUID_RANDOM_PROVIDER_GETENTROPY
# define BOOST_UUID_RANDOM_PROVIDER_NAME getentropy

#else
# define BOOST_UUID_RANDOM_PROVIDER_POSIX
# define BOOST_UUID_RANDOM_PROVIDER_NAME posix

#endif

#define BOOST_UUID_RANDOM_PROVIDER_STRINGIFY2(X) #X
#define BOOST_UUID_RANDOM_PROVIDER_STRINGIFY(X) BOOST_UUID_RANDOM_PROVIDER_STRINGIFY2(X)

#if defined(BOOST_UUID_RANDOM_PROVIDER_SHOW)
#pragma message("BOOST_UUID_RANDOM_PROVIDER_NAME " BOOST_UUID_RANDOM_PROVIDER_STRINGIFY(BOOST_UUID_RANDOM_PROVIDER_NAME))
#endif

#endif // BOOST_UUID_DETAIL_RANDOM_PROVIDER_PLATFORM_DETECTION_HPP
