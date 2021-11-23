//
// Copyright 2012-2021 Antony Polukhin.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define TEST_LIB_SOURCE
#include "test_lib_anonymous.hpp"

namespace {
    class user_defined{};
} // anonymous namespace

namespace test_lib {

boost::typeindex::type_index get_anonymous_user_defined_class() {
    return boost::typeindex::type_id<user_defined>();
}

boost::typeindex::type_index get_const_anonymous_user_defined_class() {
    return boost::typeindex::type_id_with_cvr<const user_defined>();
}

}

