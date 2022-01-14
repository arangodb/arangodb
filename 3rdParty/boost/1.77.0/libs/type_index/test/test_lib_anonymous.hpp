//
// Copyright 2012-2021 Antony Polukhin.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_TYPE_INDEX_TESTS_TEST_LIB_ANONYMOUS_HPP
#define BOOST_TYPE_INDEX_TESTS_TEST_LIB_ANONYMOUS_HPP

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

TEST_LIB_DECL boost::typeindex::type_index get_anonymous_user_defined_class();
TEST_LIB_DECL boost::typeindex::type_index get_const_anonymous_user_defined_class();

}

#endif // BOOST_TYPE_INDEX_TESTS_TEST_LIB_ANONYMOUS_HPP

