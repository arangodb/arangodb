////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Auth/Rbac/BackendImpl.h"
#include "Inspection/VPackSaveInspector.h"

#include <velocypack/Dumper.h>
#include <velocypack/Parser.h>

using namespace arangodb;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Serialize a value to VPack using the inspection framework.
template<typename T>
velocypack::Builder serializeToVPack(T& value) {
  velocypack::Builder builder;
  inspection::VPackSaveInspector<> inspector(builder);
  auto res = inspector.apply(value);
  EXPECT_TRUE(res.ok()) << (res.ok() ? "" : res.error());
  return builder;
}

// Normalize a VPack slice to a compact JSON string for comparison.
// This lets us write expected values as readable JSON strings and compare
// them after round-tripping through VPack, stripping any formatting
// differences.
std::string toNormalizedJson(velocypack::Slice slice) {
  return velocypack::Dumper::toString(slice);
}

std::string toNormalizedJson(std::string_view json) {
  auto builder = velocypack::Parser::fromJson(json);
  return velocypack::Dumper::toString(builder->slice());
}

// Serialize value and compare its normalized JSON against the expected JSON.
template<typename T>
void expectEqualsJson(T& value, std::string_view expectedJson) {
  auto actual = serializeToVPack(value);
  EXPECT_EQ(toNormalizedJson(actual.slice()), toNormalizedJson(expectedJson));
}

}  // namespace

// ---------------------------------------------------------------------------
// Tests: one per type explicitly serialized in BackendImpl
// ---------------------------------------------------------------------------

TEST(RbacSerializationTest, RequestItem) {
  auto item = rbac::Backend::RequestItem{.action = "db:ReadDatabase",
                                         .resource = "db:database:mydata",
                                         .attributeValues = {"p1", "p2"}};
  expectEqualsJson(item, R"({
    "action": "db:ReadDatabase",
    "resource": "db:database:mydata",
    "context": {
      "parameters": {
        "attribute": { "values": ["p1", "p2"] }
      }
    }
  })");
}

TEST(RbacSerializationTest, EvaluateManyRequest) {
  auto req = rbac::EvaluateManyRequest{
      .user = "alice",
      .roles = {"admin", "reader"},
      .items = {rbac::Backend::RequestItem{.action = "db:ReadDatabase",
                                           .resource = "db:database:d",
                                           .attributeValues = {}}}};
  expectEqualsJson(req, R"({
    "user": "alice",
    "roles": ["admin", "reader"],
    "items": [{
      "action": "db:ReadDatabase",
      "resource": "db:database:d",
      "context": { "parameters": { "attribute": { "values": [] } } }
    }]
  })");
}

TEST(RbacSerializationTest, EvaluateTokenManyRequest) {
  auto req = rbac::EvaluateTokenManyRequest{
      .token = "eyJhbGciOiJFUzI1NiJ9.payload.sig", .items = {}};
  expectEqualsJson(req, R"({
    "token": "eyJhbGciOiJFUzI1NiJ9.payload.sig",
    "items": []
  })");
}

TEST(RbacSerializationTest, ResponseItem) {
  auto item = rbac::Backend::ResponseItem{.effect = rbac::Backend::Effect::Deny,
                                          .message = "not authorized"};
  expectEqualsJson(item,
                   R"({ "effect": "Deny", "message": "not authorized" })");
}

TEST(RbacSerializationTest, EvaluateResponseMany) {
  auto resp = rbac::Backend::EvaluateResponseMany{};
  resp.effect = rbac::Backend::Effect::Allow;
  resp.message = "";
  resp.items = {
      rbac::Backend::ResponseItem{.effect = rbac::Backend::Effect::Allow,
                                  .message = ""},
      rbac::Backend::ResponseItem{.effect = rbac::Backend::Effect::Deny,
                                  .message = "denied"},
  };
  expectEqualsJson(resp, R"({
    "effect": "Allow",
    "message": "",
    "items": [
      { "effect": "Allow", "message": "" },
      { "effect": "Deny",  "message": "denied" }
    ]
  })");
}
