//
// Copyright 2012-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>

#include <boost/type_index.hpp>
#include "test_lib_anonymous.hpp"

#include <iostream>

#define BOOST_CHECK_NE(x, y) BOOST_CHECK(x != y)

namespace {
    class user_defined{};
}

void comparing_anonymous_types_between_modules()
{
    boost::typeindex::type_index t_const_userdef = boost::typeindex::type_id_with_cvr<const user_defined>();
    boost::typeindex::type_index t_userdef = boost::typeindex::type_id<user_defined>();

    // Known to fail on Clang and old versions of GCC.
    //BOOST_TEST_NE(t_userdef, test_lib::get_anonymous_user_defined_class());
    //BOOST_TEST_NE(t_const_userdef, test_lib::get_const_anonymous_user_defined_class());

    std::cout
        << "t_userdef == " << t_userdef
        << ", test_lib::get_anonymous_user_defined_class() == " << test_lib::get_anonymous_user_defined_class()
        << '\n';
    std::cout
        << "t_const_userdef == " << t_const_userdef
        << ", test_lib::get_const_anonymous_user_defined_class() == " << test_lib::get_const_anonymous_user_defined_class()
        << '\n';

    BOOST_TEST_NE(t_const_userdef, test_lib::get_anonymous_user_defined_class());
    BOOST_TEST_NE(t_userdef, test_lib::get_const_anonymous_user_defined_class());
}

int main() {
    comparing_anonymous_types_between_modules();

    return boost::report_errors();
}

