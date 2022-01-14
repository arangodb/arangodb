//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/error.hpp>

#include <memory>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class error_test
{
public:
    void check(error e)
    {
        auto const ec = make_error_code(e);
        BOOST_TEST(ec.category().name() != nullptr);
        BOOST_TEST(! ec.message().empty());
        BOOST_TEST(ec.category().default_error_condition(
            static_cast<int>(e)).category() == ec.category());
    }

    void check(condition c, error e)
    {
        {
            auto const ec = make_error_code(e);
            BOOST_TEST(ec.category().name() != nullptr);
            BOOST_TEST(! ec.message().empty());
            BOOST_TEST(ec == c);
        }
        {
            auto ec = make_error_condition(c);
            BOOST_TEST(ec.category().name() != nullptr);
            BOOST_TEST(! ec.message().empty());
            BOOST_TEST(ec == c);
        }
    }

    void
    run()
    {
        check(condition::parse_error, error::syntax);
        check(condition::parse_error, error::extra_data);
        check(condition::parse_error, error::incomplete);
        check(condition::parse_error, error::exponent_overflow);
        check(condition::parse_error, error::too_deep);
        check(condition::parse_error, error::illegal_leading_surrogate);
        check(condition::parse_error, error::illegal_trailing_surrogate);
        check(condition::parse_error, error::expected_hex_digit);
        check(condition::parse_error, error::expected_utf16_escape);
        check(condition::parse_error, error::object_too_large);
        check(condition::parse_error, error::array_too_large);
        check(condition::parse_error, error::key_too_large);
        check(condition::parse_error, error::string_too_large);
        check(condition::parse_error, error::exception);

        check(condition::assign_error, error::not_number);
        check(condition::assign_error, error::not_exact);

        check(error::test_failure);
    }
};

TEST_SUITE(error_test, "boost.json.error");

BOOST_JSON_NS_END
