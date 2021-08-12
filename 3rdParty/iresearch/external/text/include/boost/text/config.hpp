// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_CONFIG_HPP
#define BOOST_TEXT_CONFIG_HPP

#include <boost/config.hpp>

// Included for definition of __cpp_lib_concepts.
#include <iterator>


/** When you insert into a rope, the incoming sequence may be inserted as a
    new segment, or if it falls within an existing string-segment, it may be
    inserted into the string object used to represent that segment.  This only
    happens if the incoming sequence will fit within the existing segment's
    capacity, or if the segment is smaller than a certain limit.
    BOOST_TEXT_STRING_INSERT_MAX is that limit. */
#ifndef BOOST_TEXT_STRING_INSERT_MAX
#    define BOOST_TEXT_STRING_INSERT_MAX 4096
#endif

#if defined(__cpp_lib_concepts) && !defined(BOOST_TEXT_DISABLE_CONCEPTS)
#    define BOOST_TEXT_USE_CONCEPTS 1
#else
#    define BOOST_TEXT_USE_CONCEPTS 0
#endif

#if defined(__cpp_coroutines) && !defined(BOOST_TEXT_DISABLE_COROUTINES)
#    define BOOST_TEXT_USE_COROUTINES 1
#else
#    define BOOST_TEXT_USE_COROUTINES 0
#endif

#ifndef BOOST_TEXT_DOXYGEN

// The inline namespaces v1 and v2 represent pre- and post-C++20.  v1 is
// inline for standards before C++20, and v2 is inline for C++20 and later.
// Note that this only applies to code for which a v2 namespace alternative
// exists.  Some instances of the v1 namespace may still be inline, if there
// is no v2 version of its contents.
#if BOOST_TEXT_USE_CONCEPTS
#    define BOOST_TEXT_NAMESPACE_V1 namespace v1
#    define BOOST_TEXT_NAMESPACE_V2 inline namespace v2
#else
#    define BOOST_TEXT_NAMESPACE_V1 inline namespace v1
#    define BOOST_TEXT_NAMESPACE_V2 namespace v2
#endif

// Nothing before GCC 6 has proper C++14 constexpr support.
#if defined(__GNUC__) && __GNUC__ < 6 && !defined(__clang__)
#    define BOOST_TEXT_CXX14_CONSTEXPR
#    define BOOST_TEXT_NO_CXX14_CONSTEXPR
#elif defined(_MSC_VER) && _MSC_VER <= 1916
#    define BOOST_TEXT_CXX14_CONSTEXPR
#    define BOOST_TEXT_NO_CXX14_CONSTEXPR
#else
#    define BOOST_TEXT_CXX14_CONSTEXPR BOOST_CXX14_CONSTEXPR
#    if defined(BOOST_NO_CXX14_CONSTEXPR)
#        define BOOST_TEXT_NO_CXX14_CONSTEXPR
#    endif
#endif

// Implements separate compilation features as described in
// http://www.boost.org/more/separate_compilation.html

//  normalize macros

#if !defined(BOOST_TEXT_DYN_LINK) && !defined(BOOST_TEXT_STATIC_LINK) &&       \
    !defined(BOOST_ALL_DYN_LINK) && !defined(BOOST_ALL_STATIC_LINK)
#    define BOOST_TEXT_STATIC_LINK
#endif

#if defined(BOOST_ALL_DYN_LINK) && !defined(BOOST_TEXT_DYN_LINK)
#    define BOOST_TEXT_DYN_LINK
#elif defined(BOOST_ALL_STATIC_LINK) && !defined(BOOST_TEXT_STATIC_LINK)
#    define BOOST_TEXT_STATIC_LINK
#endif

#if defined(BOOST_TEXT_DYN_LINK) && defined(BOOST_TEXT_STATIC_LINK)
#    error Must not define both BOOST_TEXT_DYN_LINK and BOOST_TEXT_STATIC_LINK
#endif

//  enable dynamic or static linking as requested

#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_TEXT_DYN_LINK)
#    if defined(BOOST_TEXT_SOURCE)
#        define BOOST_TEXT_DECL BOOST_SYMBOL_EXPORT
#    else
#        define BOOST_TEXT_DECL BOOST_SYMBOL_IMPORT
#    endif
#else
#    define BOOST_TEXT_DECL
#endif

#if 0 // TODO: Disabled for now.
//  enable automatic library variant selection

#if !defined(BOOST_TEXT_SOURCE) && !defined(BOOST_ALL_NO_LIB) &&               \
    !defined(BOOST_TEXT_NO_LIB)
//
// Set the name of our library, this will get undef'ed by auto_link.hpp
// once it's done with it:
//
#define BOOST_LIB_NAME boost_text
//
// If we're importing code from a dll, then tell auto_link.hpp about it:
//
#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_TEXT_DYN_LINK)
#    define BOOST_DYN_LINK
#endif
//
// And include the header that does the work:
//
#include <boost/config/auto_link.hpp>
#endif  // auto-linking disabled
#endif

#endif // doxygen

#endif
