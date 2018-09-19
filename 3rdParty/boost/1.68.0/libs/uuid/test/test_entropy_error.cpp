//
// Copyright (c) 2017 James E. King III
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//   http://www.boost.org/LICENCE_1_0.txt)
//
// Entropy error class test
//

#include <boost/detail/lightweight_test.hpp>
#include <boost/uuid/entropy_error.hpp>
#include <string>

int main(int, char*[])
{
    using namespace boost::uuids;

    entropy_error err(6, "foo");
    BOOST_TEST_EQ(6, err.errcode());
    BOOST_TEST_CSTR_EQ("foo", err.what());

    return boost::report_errors();
}
