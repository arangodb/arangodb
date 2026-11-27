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
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Basics/ResultAssertions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackAssertions.h"
#include "Basics/voc-errors.h"
#include "Indexes/IndexTypeCatalog.h"
#include "Mocks/Servers.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "Indexes/VectorIndexDefinitions.h"

#include <velocypack/Builder.h>

#include <string>
#include <string_view>
#include <vector>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::velocypack;

namespace {

std::string paramsError(std::string_view reason) {
  return std::string{
      "Error with parsing the `params` attribute in vector index definition: "}
      .append(reason);
}

}  // namespace

class VectorIndexDefinitionTest : public ::testing::Test {
 protected:
  Result normalize(VPackSlice definition, VPackBuilder& normalized) {
    return factory.enhanceIndexDefinition(definition, normalized,
                                          /*isCreation*/ true,
                                          server.getSystemDatabase());
  }

  mocks::MockRestServer server;
  IndexTypeCatalog catalog{server.server()};
  RocksDBIndexFactory factory{server.server(), catalog};
};

TEST_F(VectorIndexDefinitionTest, MinimalDefinitionNormalizesWithDefaults) {
  VPackBuilder normalized;
  ASSERT_TRUE(IsOk(normalize(minimalVectorDefinition(), normalized)));

  EXPECT_TRUE(normalized.slice().get(StaticStrings::IndexName).isString());
  EXPECT_TRUE(normalized.slice().get(StaticStrings::ObjectId).isString());
  // vector normalize never writes "unique"
  EXPECT_TRUE(EqualsVPack(
      R"({"type":"vector","estimates":false,"fields":["embedding"],)"
      R"("params":{"dimension":2,"metric":"l2",)"
      R"("nLists":{"strategy":"autoSqrt","multiplier":4,"minNLists":2,)"
      R"("tiers":[{"threshold":1000000,"fixedValue":16384},)"
      R"({"threshold":10000000,"fixedValue":65536},)"
      R"({"threshold":300000000,"fixedValue":131072}]},)"
      R"("trainingIterations":25,"defaultNProbe":1,)"
      R"("numberOfDocsPerCentroid":100},)"
      R"("sparse":false,"inBackground":false})"_vpack,
      withoutKeys(normalized.slice(), {"name", "objectId"}).slice()));
}

TEST_F(VectorIndexDefinitionTest,
       DefinitionWithEveryOptionNormalizesKeepingCreateOptions) {
  VPackBuilder normalized;
  ASSERT_TRUE(IsOk(normalize(fullVectorDefinition(), normalized)));

  EXPECT_TRUE(
      EqualsVPack(R"({"type":"vector","name":"vec","estimates":false,)"
                  R"("fields":["embedding"],"storedValues":["a","b.c"],)"
                  R"("params":{"dimension":4,"metric":"cosine","nLists":3,)"
                  R"("factory":"IVF3,Flat","trainingIterations":10,)"
                  R"("defaultNProbe":2,"numberOfDocsPerCentroid":50},)"
                  R"("sparse":true,"inBackground":true,"parallelism":3})"_vpack,
                  withoutKeys(normalized.slice(), {"objectId"}).slice()));
}

TEST_F(VectorIndexDefinitionTest, InvalidDefinitionsAreRejected) {
  struct Case {
    VPackString definition;
    std::string message;
  };
  std::vector<Case> const cases{
      {R"({"type":"vector","params":{"dimension":2,"metric":"l2"}})"_vpack,
       "invalid number of index attributes"},
      {R"({"type":"vector","fields":["a","b"],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "invalid number of index attributes"},
      {R"({"type":"vector","fields":[""],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "index field names must be non-empty strings"},
      {R"({"type":"vector","fields":[{"name":"a"}],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "index field names must be only strings"},
      {R"({"type":"vector","fields":["_id"],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "_id attribute cannot be indexed"},
      {R"({"type":"vector","fields":["a[*]"],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "cannot use [*] expansion for this type of index"},
      {R"({"type":"vector","fields":["a"],"error":true,)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "bad parameter"},
      {R"({"type":"vector","fields":["a"]})"_vpack,
       paramsError("Expecting type Object")},
      {R"({"type":"vector","fields":["a"],"params":1})"_vpack,
       paramsError("Expecting type Object")},
      {R"({"type":"vector","fields":["a"],"params":{"metric":"l2"}})"_vpack,
       paramsError("Missing required attribute 'dimension'")},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":0,"metric":"l2"}})"_vpack,
       paramsError("Dimension must be greater than 0!")},
      {R"({"type":"vector","fields":["a"],"params":{"dimension":2}})"_vpack,
       paramsError("Missing required attribute 'metric'")},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"manhattan"}})"_vpack,
       paramsError("Unknown enum value manhattan")},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":0}})"_vpack,
       paramsError("nLists must be 1 or greater!")},
      // the spec's own invariant message gets lost in the variant
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":)"
       R"({"strategy":"autoSqrt","multiplier":0,"minNLists":2}}})"_vpack,
       paramsError("Could not find matching inline type")},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","defaultNProbe":0}})"_vpack,
       paramsError("defaultNProbe must be 1 or greater!")},
      {R"({"type":"vector","fields":["a"],"params":)"
       R"({"dimension":2,"metric":"l2","numberOfDocsPerCentroid":0}})"_vpack,
       paramsError("numberOfDocsPerCentroid must be 1 or greater!")},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","nProbe":1}})"_vpack,
       paramsError("Found unexpected attribute 'nProbe'")},
      {R"({"type":"vector","fields":["a"],"unique":true,)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "Vector index cannot be unique"},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","factory":"IVF4,Flat"}})"_vpack,
       "The factory string 'IVF4,Flat' fixes nLists, which conflicts with a "
       "scaling nLists specification; give a fixed nLists or use 'IVF{}'"},
      {R"({"type":"vector","fields":["a"],"params":)"
       R"({"dimension":2,"metric":"l2","nLists":3,"factory":"IVF4,Flat"}})"_vpack,
       "The nLists parameter (3) has to agree with the actual nlists implied "
       "by the factory string 'IVF4,Flat' (which is 4)"},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":3,"factory":"Flat"}})"_vpack,
       "Invalid factory string 'Flat': expected an IVF index"},
      {R"({"type":"vector","fields":["a"],"storedValues":"b",)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "when parsing fields for stored values: storedValues must be an "
       "array"},
      {R"({"type":"vector","fields":["a"],"storedValues":[],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "when parsing fields for stored values: invalid number of index "
       "attributes"},
      {R"({"type":"vector","fields":["a"],"storedValues":["b","b"],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "when parsing fields for stored values: duplicate attribute name in "
       "index fields list"},
      // thrown by the attribute parser, so no stored-values prefix
      {R"({"type":"vector","fields":["a"],"storedValues":["b[*]"],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       "cannot use [*] expansion for this type of index"},
  };

  for (auto const& c : cases) {
    VPackBuilder normalized;
    auto res = normalize(c.definition, normalized);
    EXPECT_TRUE(IsError(res, TRI_ERROR_BAD_PARAMETER))
        << c.definition.slice().toJson();
    EXPECT_EQ(res.errorMessage(), c.message) << c.definition.slice().toJson();
  }
}

TEST_F(VectorIndexDefinitionTest, UnparseableFactoryStringIsRejected) {
  VPackBuilder normalized;
  auto res = normalize(
      R"({"type":"vector","fields":["a"],"params":)"
      R"({"dimension":2,"metric":"l2","nLists":3,"factory":"IVF3,Random"}})"_vpack,
      normalized);

  // the rest is faiss' own text
  EXPECT_TRUE(IsError(res, TRI_ERROR_BAD_PARAMETER));
  EXPECT_THAT(std::string{res.errorMessage()},
              ::testing::StartsWith("Invalid factory string 'IVF3,Random': "));
}
