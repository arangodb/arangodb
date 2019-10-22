// Copyright Antony Polukhin, 2016-2019.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <boost/stacktrace/stacktrace.hpp>

#if defined(BOOST_LEXICAL_CAST_TRY_LEXICAL_CONVERT_HPP) || defined(BOOST_LEXICAL_CAST_BAD_LEXICAL_CAST_HPP)
#error "LexicalCast headers leaked into the boost/stacktrace/stacktrace.hpp"
#endif

#if !defined(BOOST_USE_WINDOWS_H) && defined(_WINDOWS_H)
#error "windows.h header leaked into the boost/stacktrace/stacktrace.hpp"
#endif

#include <stdexcept>

using namespace boost::stacktrace;

#ifdef BOOST_STACKTRACE_DYN_LINK
# ifdef BOOST_STACKTRACE_TEST_IMPL_LIB
#   define BOOST_ST_API BOOST_SYMBOL_EXPORT
# else
#   define BOOST_ST_API BOOST_SYMBOL_IMPORT
# endif
#else
# ifdef BOOST_STACKTRACE_TEST_EXPORTS_TABLE_USAGE
#   define BOOST_ST_API BOOST_SYMBOL_VISIBLE
# else
#   define BOOST_ST_API
# endif
#endif

typedef std::pair<boost::stacktrace::stacktrace, boost::stacktrace::stacktrace> st_pair;
typedef st_pair (*foo1_t)(int i);
BOOST_ST_API st_pair function_from_library(int i, foo1_t foo1);
BOOST_ST_API boost::stacktrace::stacktrace return_from_nested_namespaces();
BOOST_ST_API boost::stacktrace::stacktrace make_some_stacktrace1();
BOOST_ST_API boost::stacktrace::stacktrace make_some_stacktrace2();

#ifdef BOOST_STACKTRACE_TEST_EXPORTS_TABLE_USAGE
  BOOST_SYMBOL_VISIBLE
#endif
inline st_pair function_from_main_translation_unit(int i) {
    if (i) {
        return function_from_library(i - 1, function_from_main_translation_unit);
    }

    std::pair<stacktrace, stacktrace> ret;
    try {
        throw std::logic_error("test");
    } catch (const std::logic_error& /*e*/) {
        ret.second = stacktrace();
        return ret;
    }
}
