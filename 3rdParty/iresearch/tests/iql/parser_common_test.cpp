//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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
      iresearch::iql::proxy_filter branch_buf;
      std::locale locale;
      iresearch::bstring value_buf;
      bool value_nil;
      iresearch::iql::function_arg arg;
      auto wrapped = iresearch::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_FALSE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_TRUE(value_buf.empty());
      ASSERT_FALSE(value_nil);
    }

    // null value
    {
      iresearch::iql::proxy_filter branch_buf;
      std::locale locale;
      iresearch::bstring value_buf;
      bool value_nil;
      iresearch::iql::function_arg arg(iresearch::bytes_ref::nil);
      auto wrapped = iresearch::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_TRUE(value_buf.empty());
      ASSERT_TRUE(value_nil);
    }

    // non-null value
    {
      iresearch::iql::proxy_filter branch_buf;
      std::locale locale;
      iresearch::bstring value_buf;
      auto value = iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("abc"));
      bool value_nil;
      iresearch::iql::function_arg arg(value);
      auto wrapped = iresearch::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }

    // fnctr value
    {
      iresearch::iql::proxy_filter branch_buf;
      std::locale locale;
      iresearch::bstring value_buf;
      auto value = iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("abc"));
      bool value_nil;
      size_t value_call = 0;
      iresearch::iql::function_arg::fn_value_t value_fn = [&value, &value_call](
        iresearch::bstring& buf,
        std::locale const& locale,
        void* const& cookie,
        const iresearch::iql::function_arg::fn_args_t&
      )->bool {
        buf.append(value);
        ++value_call;
        return true;
      };
      iresearch::iql::function_arg arg(iresearch::iql::function_arg::fn_args_t(), value_fn);
      auto wrapped = iresearch::iql::function_arg::wrap(arg);

      ASSERT_FALSE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }

    // fnctr branch
    {
      iresearch::iql::proxy_filter branch_buf;
      std::locale locale;
      iresearch::bstring value_buf;
      bool value_nil;
      size_t branch_call = 0;
      iresearch::iql::function_arg::fn_branch_t branch_fn = [&branch_call](
        iresearch::iql::proxy_filter& buf,
        const std::locale& locale,
        void* const& cookie,
        const iresearch::iql::function_arg::fn_args_t&
      )->bool {
        ++branch_call;
        return true;
      };
      iresearch::iql::function_arg arg(iresearch::iql::function_arg::fn_args_t(), branch_fn);
      auto wrapped = iresearch::iql::function_arg::wrap(arg);

      ASSERT_TRUE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_EQ(1, branch_call);
      ASSERT_FALSE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_TRUE(value_buf.empty());
      ASSERT_FALSE(value_nil);
    }

    // byte value + fnctr branch
    {
      iresearch::iql::proxy_filter branch_buf;
      std::locale locale;
      iresearch::bstring value_buf;
      bool value_nil;
      auto value = iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("abc"));
      size_t branch_call = 0;
      auto branch_fn = [&branch_call](
        iresearch::iql::proxy_filter& buf,
        const std::locale& locale,
        void* const& cookie,
        const iresearch::iql::function_arg::fn_args_t&
      )->bool {
        ++branch_call;
        return true;
      };
      iresearch::iql::function_arg arg(iresearch::iql::function_arg::fn_args_t(), value, branch_fn);
      auto wrapped = iresearch::iql::function_arg::wrap(arg);

      ASSERT_TRUE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_EQ(1, branch_call);
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }

    // fnctr value + fnctr branch
    {
      iresearch::iql::proxy_filter branch_buf;
      std::locale locale;
      iresearch::bstring value_buf;
      auto value = iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("abc"));
      bool value_nil;
      size_t value_call = 0;
      auto value_fn = [&value, &value_call](
        iresearch::bstring& buf,
        std::locale const& locale,
        void* const& cookie,
        const iresearch::iql::function_arg::fn_args_t&
      )->bool {
        buf.append(value);
        ++value_call;
        return true;
      };
      size_t branch_call = 0;
      auto branch_fn = [&branch_call](
        iresearch::iql::proxy_filter& buf,
        const std::locale& locale,
        void* const& cookie,
        const iresearch::iql::function_arg::fn_args_t&
      )->bool {
        ++branch_call;
        return true;
      };
      iresearch::iql::function_arg arg(iresearch::iql::function_arg::fn_args_t(), value_fn, branch_fn);
      auto wrapped = iresearch::iql::function_arg::wrap(arg);

      ASSERT_TRUE(wrapped.branch(branch_buf, locale, nullptr));
      ASSERT_EQ(1, branch_call);
      ASSERT_TRUE(wrapped.value(value_buf, value_nil, locale, nullptr));
      ASSERT_EQ(value_buf, value);
      ASSERT_FALSE(value_nil);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
