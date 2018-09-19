/*
    Copyright 2005-2007 Adobe Systems Incorporated
    Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>

    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).

    See http://opensource.adobe.com/gil for most recent version including documentation.
*/

/*************************************************************************************************/

#ifndef GIL_CONFIG_HPP
#define GIL_CONFIG_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief GIL configuration file
/// \author Lubomir Bourdev and Hailin Jin \n
///         Adobe Systems Incorporated
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_GIL_DOXYGEN_ONLY)
/// \def BOOST_GIL_CONFIG_HAS_UNALIGNED_ACCESS
/// \brief Define to allow unaligned memory access
/// Theoretically (or historically?) on platforms which support dereferencing on
/// non-word memory boundary, unaligned access may result in performance improvement.
/// \warning Unfortunately, this optimization may be a C/C++ strict aliasing rules
/// violation, if accessed data buffer has effective type that cannot be aliased
/// without leading to undefined behaviour.
#define BOOST_GIL_CONFIG_HAS_UNALIGNED_ACCESS
#endif

#if defined(BOOST_GIL_CONFIG_HAS_UNALIGNED_ACCESS)
#if defined(sun) || defined(__sun) || \             // SunOS
    defined(__osf__) || defined(__osf) || \         // Tru64
    defined(_hpux) || defined(hpux) || \            // HP-UX
    defined(__arm__) || defined(__ARM_ARCH) || \    // ARM
    defined(_AIX)                                   // AIX
#error Unaligned access strictly disabled for some UNIX platforms or ARM architecture
#elif defined(__i386__) || defined(__x86_64__) || defined(__vax__)
    // The check for little-endian architectures that tolerate unaligned memory
    // accesses is just an optimization. Nothing will break if it fails to detect
    // a suitable architecture.
    //
    // Unfortunately, this optimization may be a C/C++ strict aliasing rules violation
    // if accessed data buffer has effective type that cannot be aliased
    // without leading to undefined behaviour.
BOOST_PRAGMA_MESSAGE("CAUTION: Unaligned access tolerated on little-endian may cause undefined behaviour")
#else
#error Unaligned access disabled for unknown platforms and architectures
#endif
#endif // defined(BOOST_GIL_CONFIG_HAS_UNALIGNED_ACCESS)

#endif
