////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "iql/query_builder.hpp"
#include "iql/parser_common.hpp"

namespace tests {
  class iql_parser_common_tests: public ::testing::Test {

    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };


  using namespace tests;

  // -----------------------------------------------------------------------------
  // --SECTION--                                                        test suite
  // -----------------------------------------------------------------------------

  TEST_F(iql_parser_common_tests, test_function_arg_wrap) {
    // empty function arg
    {
      irs::iql::proxy_filter branch_buf;
      irs::string_ref locale = "C";
      irs::bstring value_buf;
      bool value_nil;
      irs::iql::function_arg arg;
      auto wrapped = irs::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_FALSE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_TRUE(value_buf.empty());
      ASSERT_FALSE(value_nil);
    }

    // null value
    {
      irs::iql::proxy_filter branch_buf;
      irs::string_ref locale = "C";
      irs::bstring value_buf;
      bool value_nil;
      irs::iql::function_arg arg(irs::bytes_ref::NIL);
      auto wrapped = irs::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_TRUE(value_buf.empty());
      ASSERT_TRUE(value_nil);
    }

    // non-null value
    {
      irs::iql::proxy_filter branch_buf;
      irs::string_ref locale = "C";
      irs::bstring value_buf;
      auto value = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
      bool value_nil;
      irs::iql::function_arg arg(value);
      auto wrapped = irs::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }

    // fnctr value
    {
      irs::iql::proxy_filter branch_buf;
      irs::string_ref locale = "C";
      irs::bstring value_buf;
      auto value = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
      bool value_nil;
      size_t value_call = 0;
      irs::iql::function_arg::fn_value_t value_fn = [&value, &value_call](
          irs::bstring& buf,
          const irs::string_ref& locale,
          void* const& cookie,
          const irs::iql::function_arg::fn_args_t&)->bool {
        buf.append(value);
        ++value_call;
        return true;
      };
      irs::iql::function_arg arg(irs::iql::function_arg::fn_args_t(), value_fn);
      auto wrapped = irs::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }

    // fnctr branch
    {
      irs::iql::proxy_filter branch_buf;
      irs::string_ref locale = "C";
      irs::bstring value_buf;
      bool value_nil;
      size_t branch_call = 0;
      irs::iql::function_arg::fn_branch_t branch_fn = [&branch_call](
          irs::iql::proxy_filter& buf,
          const irs::string_ref& locale,
          void* const& cookie,
          const irs::iql::function_arg::fn_args_t&)->bool {
        ++branch_call;
        return true;
      };
      irs::iql::function_arg arg(irs::iql::function_arg::fn_args_t(), branch_fn);
      auto wrapped = irs::iql::function_arg::wrap(arg);

      ASSERT_TRUE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_EQ(1, branch_call);
      ASSERT_FALSE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_TRUE(value_buf.empty());
      ASSERT_FALSE(value_nil);
    }

    // byte value + fnctr branch
    {
      irs::iql::proxy_filter branch_buf;
      irs::string_ref locale = "C";
      irs::bstring value_buf;
      bool value_nil;
      auto value = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
      size_t branch_call = 0;
      auto branch_fn = [&branch_call](
        irs::iql::proxy_filter& buf,
        const irs::string_ref& locale,
        void* const& cookie,
        const irs::iql::function_arg::fn_args_t&
      )->bool {
        ++branch_call;
        return true;
      };
      irs::iql::function_arg arg(irs::iql::function_arg::fn_args_t(), value, branch_fn);
      auto wrapped = irs::iql::function_arg::wrap(arg);

      ASSERT_TRUE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_EQ(1, branch_call);
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }

    // fnctr value + fnctr branch
    {
      irs::iql::proxy_filter branch_buf;
      irs::string_ref locale = "C";
      irs::bstring value_buf;
      auto value = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
      bool value_nil;
      size_t value_call = 0;
      auto value_fn = [&value, &value_call](
          irs::bstring& buf,
          irs::string_ref const& locale,
          void* const& cookie,
          const irs::iql::function_arg::fn_args_t&)->bool {
        buf.append(value);
        ++value_call;
        return true;
      };
      size_t branch_call = 0;
      auto branch_fn = [&branch_call](
          irs::iql::proxy_filter& buf,
          const irs::string_ref& locale,
          void* const& cookie,
          const irs::iql::function_arg::fn_args_t&)->bool {
        ++branch_call;
        return true;
      };
      irs::iql::function_arg arg(irs::iql::function_arg::fn_args_t(), value_fn, branch_fn);
      auto wrapped = irs::iql::function_arg::wrap(arg);

      ASSERT_TRUE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_EQ(1, branch_call);
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }
  }
}
