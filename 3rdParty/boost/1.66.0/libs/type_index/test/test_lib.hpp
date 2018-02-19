//
// Copyright (c) Antony Polukhin, 2012-2015.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_TYPE_INDEX_TESTS_TEST_LIB_HPP
#define BOOST_TYPE_INDEX_TESTS_TEST_LIB_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER)
# pragma once
#endif

#include <boost/type_index.hpp>

// This is ALWAYS a dynamic library
#if defined(TEST_LIB_SOURCE)
#   define TEST_LIB_DECL BOOST_SYMBOL_EXPORT
# else
#   define TEST_LIB_DECL BOOST_SYMBOL_IMPORT
# endif

namespace test_lib {

TEST_LIB_DECL boost::typeindex::type_index get_integer();
TEST_LIB_DECL boost::typeindex::type_index get_user_defined_class();

TEST_LIB_DECL boost::typeindex::type_index get_const_integer();
TEST_LIB_DECL boost::typeindex::type_index get_const_user_defined_class();

#ifndef BOOST_HAS_PRAGMA_DETECT_MISMATCH
// This is required for checking RTTI on/off linkage
TEST_LIB_DECL void accept_typeindex(const boost::typeindex::type_index&);
#endif

}

#endif // BOOST_TYPE_INDEX_TESTS_LIB1_HPP

