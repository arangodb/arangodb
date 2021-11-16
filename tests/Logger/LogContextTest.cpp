////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <stdexcept>
#include <string_view>
#include <type_traits>
#include "gtest/gtest.h"

#include "Basics/overload.h"
#include "Logger/LogContext.h"

constexpr char LogKey1[] = "key1";
constexpr char LogKey2[] = "key2";
constexpr char LogKey3[] = "key3";
constexpr char LogKey4[] = "key4";
constexpr char LogKey5[] = "key5";
constexpr char LogKey6[] = "key6";

namespace arangodb {

struct LogContextTest : ::testing::Test {
  using ScopedValue = LogContext::Accessor::ScopedValue;
};

TEST_F(LogContextTest, visit_visits_values_in_order_they_are_added) {
  ScopedValue v1(LogContext::makeValue().with<LogKey1>(1).with<LogKey2>(2u).with<LogKey3>(std::int8_t(3)));
  ScopedValue v2(LogContext::makeValue().with<LogKey4>(std::uint8_t(4)).with<LogKey5>(5.0f).with<LogKey6>("blubb"));
  unsigned cnt = 0;
  LogContext::OverloadVisitor visitor(overload{
    [&cnt](std::string_view key, std::int64_t value) {
      if (cnt == 0) {
        EXPECT_EQ(1, value);
      } else if (cnt == 2) {
        EXPECT_EQ(3, value);
      } else {
        EXPECT_TRUE(false) << "unexpected cnt " << cnt << " with value " << value;
      }
      ++cnt;
    },
    [&cnt](std::string_view key, std::uint64_t value) {
      if (cnt == 1) {
        EXPECT_EQ(2, value);
      } else if (cnt == 3) {
        EXPECT_EQ(4, value);
      } else {
        EXPECT_TRUE(false) << "unexpected cnt " << cnt << " with value " << value;
      }
      ++cnt;
    },
    [&cnt](std::string_view key, double value) {
      EXPECT_EQ(4, cnt++);
      EXPECT_EQ(5.0, value);
    },
    [&cnt](std::string_view key, std::string_view value) {
      EXPECT_EQ("blubb", value) << "value: " << value;
      EXPECT_EQ(5, cnt++);
    },
  });
  LogContext::current().visit(visitor);
  EXPECT_EQ(cnt, 6);
}

TEST_F(LogContextTest, ScopedValue_sets_values_from_ValueBuilder_for_current_scope) {
  unsigned cnt = 0;
  LogContext::OverloadVisitor countingVisitor([&cnt](std::string_view, auto&&) { ++cnt; });
  
  {
    ScopedValue v(LogContext::makeValue().with<LogKey1>("blubb").with<LogKey2>(42));
    LogContext::current().visit(countingVisitor);
  }
  EXPECT_EQ(2, cnt);
  
  LogContext::current().visit(countingVisitor);
  EXPECT_EQ(2, cnt);
}

TEST_F(LogContextTest, ScopedValue_sets_Values_for_current_scope) {
  unsigned cnt = 0;
  LogContext::OverloadVisitor countingVisitor([&cnt](std::string_view, auto&&) { ++cnt; });
  auto values = LogContext::makeValue().with<LogKey1>("blubb").with<LogKey2>(42).share();

  {
    ScopedValue v(values);
    LogContext::current().visit(countingVisitor);
  }
  EXPECT_EQ(2, cnt);

  LogContext::current().visit(countingVisitor);
  EXPECT_EQ(2, cnt);

  {
    ScopedValue v(values);
    LogContext::current().visit(countingVisitor);
  }
  EXPECT_EQ(4, cnt);
}

TEST_F(LogContextTest, current_returns_copy_of_the_threads_current_LogContext) {
  unsigned cnt = 0;
  LogContext::OverloadVisitor countingVisitor([&cnt](std::string_view, auto&&) { ++cnt; });
  LogContext ctx;
  {
    auto v = ScopedValue::with<LogKey1>("blubb");
    ctx = LogContext::current();
  }

  ctx.visit(countingVisitor);
  EXPECT_EQ(1, cnt);
}

TEST_F(LogContextTest, ScopedContext_sets_the_given_LogContext_for_the_current_scope) {
  unsigned cnt = 0;
  LogContext::OverloadVisitor countingVisitor([&cnt](std::string_view, auto&&) { ++cnt; });
  LogContext ctx;
  {
    ScopedValue v(LogContext::makeValue().with<LogKey1>("blubb").with<LogKey2>(42));
    ctx = LogContext::current();
  }
  
  {
    LogContext::ScopedContext c(ctx);
    LogContext::current().visit(countingVisitor);
    EXPECT_EQ(2, cnt);
  }
  
  LogContext::current().visit(countingVisitor);
  EXPECT_EQ(2, cnt);
}

TEST_F(LogContextTest, ScopedContext_does_nothing_if_contexts_are_equivalent) {
  unsigned cnt = 0;
  LogContext::OverloadVisitor countingVisitor([&cnt](std::string_view, auto&&) { ++cnt; });
  
  ScopedValue v(LogContext::makeValue().with<LogKey1>("blubb").with<LogKey2>(42));
  LogContext ctx = LogContext::current();
  
  {
    LogContext::ScopedContext c(ctx);
    LogContext::current().visit(countingVisitor);
    EXPECT_EQ(2, cnt);
  }
  
  LogContext::current().visit(countingVisitor);
  EXPECT_EQ(4, cnt);
}

TEST_F(LogContextTest, withLogContext_captures_the_current_LogContext_and_sets_it_for_the_scope_of_the_wrapped_callable) {
  unsigned cnt = 0;
  LogContext::OverloadVisitor countingVisitor([&cnt](std::string_view, auto&&) { ++cnt; });
  ScopedValue v(LogContext::makeValue().with<LogKey1>("blubb").with<LogKey2>(42));
  
  auto func = withLogContext([](LogContext::Visitor const& visitor) {
    LogContext::current().visit(visitor);
  });
  
  func(countingVisitor);
  EXPECT_EQ(2, cnt);
}

}
