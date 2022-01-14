//
// Copyright 2012-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_index.hpp>
#include "test_lib.hpp"

#include <boost/core/lightweight_test.hpp>

namespace user_defined_namespace {
    class user_defined{};
}

void comparing_types_between_modules()
{
    boost::typeindex::type_index t_const_int = boost::typeindex::type_id_with_cvr<const int>();
    boost::typeindex::type_index t_int = boost::typeindex::type_id<int>();

    BOOST_TEST_EQ(t_int, test_lib::get_integer());
    BOOST_TEST_EQ(t_const_int, test_lib::get_const_integer());
    BOOST_TEST_NE(t_const_int, test_lib::get_integer());
    BOOST_TEST_NE(t_int, test_lib::get_const_integer());


    boost::typeindex::type_index t_const_userdef 
        = boost::typeindex::type_id_with_cvr<const user_defined_namespace::user_defined>();
    boost::typeindex::type_index t_userdef 
        = boost::typeindex::type_id<user_defined_namespace::user_defined>();

    BOOST_TEST_EQ(t_userdef, test_lib::get_user_defined_class());
    BOOST_TEST_EQ(t_const_userdef, test_lib::get_const_user_defined_class());
    BOOST_TEST_NE(t_const_userdef, test_lib::get_user_defined_class());
    BOOST_TEST_NE(t_userdef, test_lib::get_const_user_defined_class());


    BOOST_TEST_NE(t_userdef, test_lib::get_integer());
    BOOST_TEST_NE(t_const_userdef, test_lib::get_integer());
    BOOST_TEST_NE(t_int, test_lib::get_user_defined_class());
    BOOST_TEST_NE(t_const_int, test_lib::get_const_user_defined_class());

    // MSVC supports detect_missmatch pragma, but /GR- silently switch disable the link time check.
    // /GR- undefies the _CPPRTTI macro. Using it to detect working detect_missmatch pragma.
    #if !defined(BOOST_HAS_PRAGMA_DETECT_MISMATCH) || !defined(_CPPRTTI)
        test_lib::accept_typeindex(t_int);
    #endif
}


int main() {
    comparing_types_between_modules();

    return boost::report_errors();
}

