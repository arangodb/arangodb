////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <velocypack/Compare.h>
#include <velocypack/Parser.h>
#include "Agency/TransactionBuilder.h"


using namespace arangodb::agency;

namespace {
std::shared_ptr<VPackBuilder> vpackFromJsonString(char const* c) {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);
  return parser.steal();
}

auto operator"" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}
}

TEST(TransactionBuilderTest, read_transaction) {
  VPackBuilder builder;
  envelope::into_builder(builder).read().key("a").end().done();

  auto expected = R"=([["a"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, read_transaction_multiple) {
  VPackBuilder builder;
  envelope::into_builder(builder).read().key("a").key("b").key("c").end().done();

  auto expected = R"=([["a", "b", "c"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, write_transaction) {
  VPackBuilder builder;
  envelope::into_builder(builder).write().set("a", 12).end("client-id").done();

  auto expected = R"=([[{"a":{"op":"set", "new": 12}}, {}, "client-id"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, write_transaction_emplace) {
  VPackBuilder builder;
  envelope::into_builder(builder).write().emplace("a", [](VPackBuilder& builder) {
    builder.add("foo", VPackValue("bar"));
  }).end("client-id").done();

  auto expected = R"=([[{"a":{"foo":"bar"}}, {}, "client-id"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, write_transaction_multi) {
  VPackBuilder builder;
  envelope::into_builder(builder).write().set("a", 12).inc("b").end("client-id").done();

  auto expected = R"=([[{"a":{"op":"set", "new": 12}, "b":{"op":"increment", "delta":1}}, {}, "client-id"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, write_transaction_inc) {
  VPackBuilder builder;
  envelope::into_builder(builder).write().inc("b", 5).end("client-id").done();

  auto expected = R"=([[{"b":{"op":"increment", "delta":5}}, {}, "client-id"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, write_transaction_remove) {
  VPackBuilder builder;
  envelope::into_builder(builder).write().remove("c").end("client-id").done();

  auto expected = R"=([[{"c":{"op":"delete"}}, {}, "client-id"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, write_transaction_precs) {
  VPackBuilder builder;
  envelope::into_builder(builder).write().set("a", 12).precs().isEmpty("b").end("client-id").done();

  auto expected = R"=([[{"a":{"op":"set", "new": 12}}, {"b":{"oldEmpty":true}}, "client-id"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, write_transaction_precs_multi) {
  VPackBuilder builder;
  envelope::into_builder(builder).write().set("a", 12).precs().isEmpty("b").isEqual("c", 12).end("client-id").done();

  auto expected = R"=([[{"a":{"op":"set", "new": 12}}, {"b":{"oldEmpty":true}, "c":{"old": 12}}, "client-id"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}

TEST(TransactionBuilderTest, multi_envelope) {
  VPackBuilder builder;
  envelope::into_builder(builder)
    .write()
      .set("a", 12)
    .precs()
      .isEmpty("b")
      .isEqual("c", 12)
    .end("client-id")
    .read()
      .key("a")
    .end()
  .done();

  auto expected = R"=([[{"a":{"op":"set", "new": 12}}, {"b":{"oldEmpty":true}, "c":{"old": 12}}, "client-id"], ["a"]])="_vpack;
  ASSERT_EQ(builder.toJson(), expected->toJson());
}
