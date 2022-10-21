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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <Basics/ErrorT.h>

using namespace ::arangodb::errors;

struct Error {
  std::string msg;

  Error(std::string msg) : msg(msg) {}

  bool operator==(const Error&) const = default;
};

struct Value {
  std::string val;

  Value() noexcept : val() {}
  Value(std::string val) : val(val) {}

  bool operator==(const Value&) const = default;
};

struct Value2 {
  std::string val;
  int second;

  Value2(std::string val, int second) : val(val), second(second) {}

  bool operator==(const Value2&) const = default;
};

TEST(ErrorTTest, default_construction) {
  // This only works because Value is nothrow default constructible
  auto f = ErrorT<Error, Value>();
  EXPECT_TRUE(f.ok());

  // This only works because Value is not implicitly convertible to bool
  EXPECT_TRUE(f);

  EXPECT_EQ(f.get(), Value());

  EXPECT_EQ(f->val, "");
}

TEST(ErrorTTest, simple_test_ok) {
  auto f = ErrorT<Error, int>::ok(5);
  EXPECT_TRUE(f.ok());
  EXPECT_EQ(f.get(), 5);
  EXPECT_EQ(*f, 5);

  f.get() = 6;
  EXPECT_EQ(f.get(), 6);
}

TEST(ErrorTTest, simple_test_error) {
  auto const err = Error("foo");
  auto g = ErrorT<Error, int>::error(err);
  EXPECT_FALSE(g.ok());
  EXPECT_EQ(g.error(), err);
}

TEST(ErrorTTest, both_types_same) {
  auto err = Error("foo");
  auto f = ErrorT<Error, Error>::ok(err);

  EXPECT_TRUE(f.ok());
  EXPECT_EQ(f.get(), err);

  auto g = ErrorT<Error, Error>::error(err);
  EXPECT_FALSE(g.ok());
  EXPECT_EQ(g.error(), err);
}

TEST(ErrorTTest, forward_construction) {
  auto f = ErrorT<Error, Value2>::ok("hello", 2);

  EXPECT_TRUE(f.ok());
  EXPECT_EQ(f.get(), Value2("hello", 2));

  auto g = ErrorT<Error, Value2>::error("Something went wrong");

  EXPECT_FALSE(g.ok());
  EXPECT_EQ(g.error(), Error("Something went wrong"));
}
