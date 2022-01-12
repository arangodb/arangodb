//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json.hpp>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

struct json_test
{
    ::test_suite::log_type log;

    void
    run()
    {
        log <<
            "sizeof(alignof)\n"
            "  object        == " << sizeof(object) << " (" << alignof(object) << ")\n"
            "    value_type  == " << sizeof(object::value_type) << " (" << alignof(object::value_type) << ")\n"
            "  array         == " << sizeof(array) << " (" << alignof(array) << ")\n"
            "  string        == " << sizeof(string) << " (" << alignof(string) << ")\n"
            "  value         == " << sizeof(value) << " (" << alignof(value) << ")\n"
            "  serializer    == " << sizeof(serializer) << "\n"
            "  stream_parser == " << sizeof(stream_parser)
            ;
        BOOST_TEST_PASS();
    }
};

TEST_SUITE(json_test, "boost.json.zsizes");

BOOST_JSON_NS_END
