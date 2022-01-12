//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef GTEST_HPP
#define GTEST_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/string_view.hpp>

#include "test_suite.hpp"

#define TEST(s1,s2) \
struct s1 ## _ ## s2 ## _test; \
TEST_SUITE(s1 ## _ ## s2 ## _test, "boost.Ryu." #s1 "." #s2); \
struct s1 ## _ ## s2 ## _test { \
  void run(); }; void s1 ## _ ## s2 ## _test::run()

#define EXPECT_STREQ(s1, s2) \
    BOOST_TEST(::boost::json::string_view(s1) == \
               ::boost::json::string_view(s2))

#define ASSERT_STREQ(s1, s2) \
    { auto const s1_ = (s1); auto const s2_ = (s2); \
    EXPECT_STREQ(s1_, s2_); }

#define ASSERT_EQ(e1, e2) BOOST_TEST((e1)==(e2))

#endif
