//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/kind.hpp>

#include <boost/json/string_view.hpp>

#include <type_traits>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class kind_test
{
public:
    BOOST_STATIC_ASSERT(
        std::is_enum<kind>::value);

    void
    check(kind k, string_view s)
    {
        BOOST_TEST(
            to_string(k) == s);
        std::stringstream ss;
        ss << k;
        BOOST_TEST(ss.str() == s);
    }

    void
    run()
    {
        check(kind::array,   "array");
        check(kind::object,  "object");
        check(kind::string,  "string");
        check(kind::int64,   "int64");
        check(kind::uint64,  "uint64");
        check(kind::double_, "double");
        check(kind::bool_,   "bool");
        check(kind::null,    "null");
    }
};

TEST_SUITE(kind_test, "boost.json.kind");

BOOST_JSON_NS_END
