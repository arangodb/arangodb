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

#include "RocksDBEngine/StorageEngineIndexTest.h"

#include "Basics/ResultAssertions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/voc-errors.h"
#include "Inspection/VPack.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "VectorIndex/Definition.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>
#include <gmock/gmock.h>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

#include <string>
#include <string_view>
#include <vector>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::velocypack;

// persistent/hash/skiplist all instantiate RocksDBVPackIndex with only
// type()/typeName() overridden - parameterized instead of duplicated per type.

class StorageEngineIndexTypeTest
    : public StorageEngineIndexTest,
      public testing::WithParamInterface<std::string_view> {
 protected:
  static VPackString indexDefinition(std::string_view type) {
    VPackBuilder b;
    b.openObject();
    b.add("type", VPackValue(type));
    b.add("fields", VPackValue(VPackValueType::Array));
    b.add(VPackValue("value"));
    b.close();
    b.close();
    return VPackString{b.slice()};
  }
};

TEST_P(StorageEngineIndexTypeTest, ReflectsInsertedDocuments) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("a").first);
}

TEST_P(StorageEngineIndexTypeTest, UpdateReplacesIndexEntry) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  // A stale entry (keyed on value 1) would make this return two.
  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("k").first);
}

TEST_P(StorageEngineIndexTypeTest, RemoveDeletesIndexEntry) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  ASSERT_EQ(scanIndex(*index).size(), 2u);

  ASSERT_TRUE(IsOk(remove(keyOnly("a"))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("b").first);
}

TEST_P(StorageEngineIndexTypeTest, ReturnsEntriesInAscendingOrder) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("a", 3))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("c", 2))));

  std::vector<LocalDocumentId> expected{
      lookupKey("b").first, lookupKey("c").first, lookupKey("a").first};
  EXPECT_EQ(scanIndex(*index), expected);
}

INSTANTIATE_TEST_CASE_P(VPackIndexAliases, StorageEngineIndexTypeTest,
                        ::testing::Values("persistent", "hash", "skiplist"));

// RocksDBTtlIndex::insert converts the attribute to a timestamp, so a
// document is only indexed if that conversion succeeds.

namespace {
VPackString ttlIndexDefinition() {
  return R"({"type":"ttl","fields":["value"],"expireAfter":3600,)"
         R"("unique":false,"sparse":true})"_vpack;
}
}  // namespace

TEST_F(StorageEngineIndexTest, TtlIndexIndexesDocumentsWithNumericExpireValue) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("a").first);
}

TEST_F(StorageEngineIndexTest,
       TtlIndexExcludesDocumentsMissingExpireAttribute) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyOnly("a"))));

  EXPECT_TRUE(scanIndex(*index).empty());
}

TEST_F(StorageEngineIndexTest, TtlIndexReturnsEntriesInAscendingOrder) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("a", 3))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("c", 2))));

  std::vector<LocalDocumentId> expected{
      lookupKey("b").first, lookupKey("c").first, lookupKey("a").first};
  EXPECT_EQ(scanIndex(*index), expected);
}

TEST_F(StorageEngineIndexTest, TtlIndexUpdateReplacesIndexEntry) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  // A stale entry (keyed on the old timestamp) would make this return two.
  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("k").first);
}

TEST_F(StorageEngineIndexTest, TtlIndexRemoveDeletesIndexEntry) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  ASSERT_EQ(scanIndex(*index).size(), 2u);

  ASSERT_TRUE(IsOk(remove(keyOnly("a"))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("b").first);
}

// primary/edge can't be created through this path
TEST_F(StorageEngineIndexTest, FactoryDispatchesToExpectedIndexType) {
  struct Case {
    IndexType type;
    VPackString definition;
  };
  std::vector<Case> const cases{
      {IndexType::Geo, R"({"type":"geo","fields":["loc"]})"_vpack},
      {IndexType::Geo1, R"({"type":"geo1","fields":["loc"]})"_vpack},
      {IndexType::Geo2, R"({"type":"geo2","fields":["lat","lon"]})"_vpack},
      {IndexType::Fulltext, R"({"type":"fulltext","fields":["x"]})"_vpack},
      {IndexType::Zkd, R"({"type":"zkd","fields":["x"]})"_vpack},
      {IndexType::MDI, R"({"type":"mdi","fields":["x"]})"_vpack},
      {IndexType::MDIPrefixed,
       R"({"type":"mdi-prefixed","fields":["x"]})"_vpack},
  };

  for (auto const& c : cases) {
    auto index = makeIndex(c.definition);
    EXPECT_EQ(index->type(), c.type) << c.definition.slice().toJson();
  }
}

// makeIndex skips enhanceIndexDefinition, where the vector checks live

class StorageEngineVectorIndexTest : public StorageEngineIndexTest {
 protected:
  Result normalize(VPackSlice definition, VPackBuilder& normalized) {
    return engine().indexFactory().enhanceIndexDefinition(
        definition, normalized, /*isCreation*/ true, *_database);
  }

  std::shared_ptr<Index> createNormalized(VPackSlice definition) {
    VPackBuilder normalized;
    EXPECT_TRUE(IsOk(normalize(definition, normalized)));
    return makeIndex(normalized.slice());
  }

  std::shared_ptr<Index> loadIndex(VPackSlice marker) {
    return engine().indexFactory().prepareIndexFromSlice(
        marker, /*generateKey*/ false, *_collection,
        /*isClusterConstructor*/ true);
  }

  static VPackBuilder toVPack(Index const& index, Index::Serialize flag) {
    VPackBuilder builder;
    index.toVelocyPack(builder, Index::makeFlags(flag));
    return builder;
  }

  static VPackBuilder withoutKeys(VPackSlice slice,
                                  std::vector<std::string> const& keys) {
    return VPackCollection::remove(slice, keys);
  }

  static VPackBuilder withId(VPackSlice definition, VPackValue id) {
    VPackBuilder idObject;
    idObject.openObject();
    idObject.add(StaticStrings::IndexId, id);
    idObject.close();
    return VPackCollection::merge(definition, idObject.slice(),
                                  /*mergeValues*/ false);
  }

  static ::testing::AssertionResult EqualsVPack(VPackSlice expected,
                                                VPackSlice actual) {
    if (basics::VelocyPackHelper::equal(expected, actual, /*useUTF8*/ true)) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "expected " << expected.toJson()
                                         << "\n  actual " << actual.toJson();
  }
};

namespace {
VPackString minimalVectorDefinition() {
  return R"({"type":"vector","fields":["embedding"],)"
         R"("params":{"dimension":2,"metric":"l2"}})"_vpack;
}

VPackString fullVectorDefinition() {
  return R"({"type":"vector","name":"vec","fields":["embedding"],)"
         R"("sparse":true,"estimates":true,"inBackground":true,)"
         R"("parallelism":3,"storedValues":["a","b.c"],)"
         R"("params":{"dimension":4,"metric":"cosine","nLists":3,)"
         R"("factory":"IVF3,Flat","trainingIterations":10,)"
         R"("defaultNProbe":2,"numberOfDocsPerCentroid":50}})"_vpack;
}
}  // namespace

TEST_F(StorageEngineVectorIndexTest, MinimalDefinitionNormalizesWithDefaults) {
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

TEST_F(StorageEngineVectorIndexTest,
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

TEST_F(StorageEngineVectorIndexTest, MinimalDefinitionSerializesWithDefaults) {
  auto index = createNormalized(minimalVectorDefinition());
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->type(), IndexType::Vector);

  auto out = toVPack(*index, Index::Serialize::Basics);
  EXPECT_EQ(out.slice().get(StaticStrings::IndexId).stringView(),
            std::to_string(index->id().id()));
  EXPECT_TRUE(out.slice()
                  .get(StaticStrings::IndexName)
                  .stringView()
                  .starts_with("idx_"));
  EXPECT_TRUE(EqualsVPack(
      R"({"type":"vector","fields":["embedding"],"unique":false,)"
      R"("sparse":false,"params":{"dimension":2,"metric":"l2",)"
      R"("nLists":{"strategy":"autoSqrt","multiplier":4,"minNLists":2,)"
      R"("tiers":[{"threshold":1000000,"fixedValue":16384},)"
      R"({"threshold":10000000,"fixedValue":65536},)"
      R"({"threshold":300000000,"fixedValue":131072}]},)"
      R"("trainingIterations":25,"defaultNProbe":1,)"
      R"("numberOfDocsPerCentroid":100},)"
      R"("trainingState":"unusable",)"
      R"("errorMessage":"not enough training data for vector index"})"_vpack,
      withoutKeys(out.slice(), {"id", "name"}).slice()));
}

TEST_F(StorageEngineVectorIndexTest,
       DefinitionWithEveryOptionSerializesAllOfThem) {
  auto index = createNormalized(fullVectorDefinition());
  ASSERT_NE(index, nullptr);

  auto out = toVPack(*index, Index::Serialize::Basics);
  EXPECT_TRUE(EqualsVPack(
      R"({"type":"vector","name":"vec","fields":["embedding"],)"
      R"("unique":false,"sparse":true,)"
      R"("params":{"dimension":4,"metric":"cosine","nLists":3,)"
      R"("factory":"IVF3,Flat","trainingIterations":10,)"
      R"("defaultNProbe":2,"numberOfDocsPerCentroid":50},)"
      R"("storedValues":["a","b.c"],"trainingState":"unusable",)"
      R"("errorMessage":"not enough training data for vector index"})"_vpack,
      withoutKeys(out.slice(), {"id"}).slice()));
}

TEST_F(StorageEngineVectorIndexTest, ScalingNListsSerializesWithDefaultTiers) {
  auto index = createNormalized(
      R"({"type":"vector","fields":["embedding"],)"
      R"("params":{"dimension":2,"metric":"innerProduct",)"
      R"("nLists":{"strategy":"autoSqrt","multiplier":2,"minNLists":5},)"
      R"("factory":"IVF{},Flat"}})"_vpack);
  ASSERT_NE(index, nullptr);

  auto out = toVPack(*index, Index::Serialize::Basics);
  EXPECT_TRUE(EqualsVPack(
      R"({"dimension":2,"metric":"innerProduct",)"
      R"("nLists":{"strategy":"autoSqrt","multiplier":2,"minNLists":5,)"
      R"("tiers":[{"threshold":1000000,"fixedValue":16384},)"
      R"({"threshold":10000000,"fixedValue":65536},)"
      R"({"threshold":300000000,"fixedValue":131072}]},)"
      R"("factory":"IVF{},Flat","trainingIterations":25,)"
      R"("defaultNProbe":1,"numberOfDocsPerCentroid":100})"_vpack,
      out.slice().get("params")));
}

TEST_F(StorageEngineVectorIndexTest, InternalsMarkerRoundTripsThroughReload) {
  for (auto const& definition :
       {minimalVectorDefinition(), fullVectorDefinition()}) {
    auto index = createNormalized(definition);
    ASSERT_NE(index, nullptr);

    auto marker = toVPack(*index, Index::Serialize::Internals);
    EXPECT_EQ(marker.slice().get(StaticStrings::ObjectId).stringView(),
              std::to_string(
                  std::static_pointer_cast<RocksDBIndex>(index)->objectId()));
    EXPECT_TRUE(EqualsVPack(
        toVPack(*index, Index::Serialize::Basics).slice(),
        withoutKeys(marker.slice(), {StaticStrings::ObjectId}).slice()));

    auto reloaded = loadIndex(marker.slice());
    ASSERT_NE(reloaded, nullptr);
    EXPECT_EQ(reloaded->type(), IndexType::Vector);
    EXPECT_TRUE(
        EqualsVPack(marker.slice(),
                    toVPack(*reloaded, Index::Serialize::Internals).slice()));
  }
}

TEST_F(StorageEngineVectorIndexTest, OldMarkerReloadsWithTodaysDefaults) {
  auto index =
      loadIndex(R"({"id":"4711","type":"vector","fields":["embedding"],)"
                R"("objectId":"98765",)"
                R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack);
  ASSERT_NE(index, nullptr);

  EXPECT_TRUE(EqualsVPack(
      R"({"id":"4711","type":"vector","name":"idx_4711",)"
      R"("fields":["embedding"],"objectId":"98765",)"
      R"("unique":false,"sparse":false,)"
      R"("params":{"dimension":2,"metric":"l2","nLists":1,)"
      R"("trainingIterations":25,"defaultNProbe":1,)"
      R"("numberOfDocsPerCentroid":100},)"
      R"("trainingState":"unusable",)"
      R"("errorMessage":"not enough training data for vector index"})"_vpack,
      toVPack(*index, Index::Serialize::Internals).slice()));
}

TEST_F(StorageEngineVectorIndexTest,
       PreMigrationMarkerLoadsTrainedDataFromDefinition) {
  faiss::IndexFlatL2 quantizer(2);
  faiss::IndexIVFFlat ivf(&quantizer, 2, 1);
  std::vector<float> const points{0, 0, 1, 1, 2, 2, 3, 3};
  ivf.train(4, points.data());
  auto trainedData = arangodb::vector::serializeIndex(ivf);

  // fresh objectId, so there's no metadata slot and the definition is used
  VPackBuilder marker;
  {
    VPackObjectBuilder guard(&marker);
    marker.add(VPackObjectIterator(
        R"({"id":"4712","type":"vector","name":"vec",)"
        R"("fields":["embedding"],"objectId":"98766",)"
        R"("unique":false,"sparse":false,)"
        R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack));
    marker.add(VPackValue("trainedData"));
    velocypack::serialize(marker, trainedData);
  }

  auto index = loadIndex(marker.slice());
  ASSERT_NE(index, nullptr);
  auto out = toVPack(*index, Index::Serialize::Internals);
  EXPECT_EQ(out.slice().get(StaticStrings::IndexTrainingState).stringView(),
            StaticStrings::IndexTrainingStateReady);
  EXPECT_EQ(out.slice()
                .get(StaticStrings::IndexResolvedNLists)
                .getNumber<std::size_t>(),
            1u);

  // trainedData is never written back, so its own marker reloads untrained
  EXPECT_TRUE(out.slice().get("trainedData").isNone());
  auto reloaded = loadIndex(out.slice());
  ASSERT_NE(reloaded, nullptr);
  EXPECT_EQ(toVPack(*reloaded, Index::Serialize::Basics)
                .slice()
                .get(StaticStrings::IndexTrainingState)
                .stringView(),
            StaticStrings::IndexTrainingStateUnusable);
}

TEST_F(StorageEngineVectorIndexTest, InvalidDefinitionsAreRejected) {
  struct Case {
    VPackString definition;
    std::string_view message;
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
       "Error with parsing the `params` attribute in vector index "
       "definition: Expecting type Object"},
      {R"({"type":"vector","fields":["a"],"params":1})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: Expecting type Object"},
      {R"({"type":"vector","fields":["a"],"params":{"metric":"l2"}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: Missing required attribute 'dimension'"},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":0,"metric":"l2"}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: Dimension must be greater than 0!"},
      {R"({"type":"vector","fields":["a"],"params":{"dimension":2}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: Missing required attribute 'metric'"},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"manhattan"}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: Unknown enum value manhattan"},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":0}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: nLists must be 1 or greater!"},
      // the spec's own invariant message gets lost in the variant
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":)"
       R"({"strategy":"autoSqrt","multiplier":0,"minNLists":2}}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: Could not find matching inline type"},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","defaultNProbe":0}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: defaultNProbe must be 1 or greater!"},
      {R"({"type":"vector","fields":["a"],"params":)"
       R"({"dimension":2,"metric":"l2","numberOfDocsPerCentroid":0}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: numberOfDocsPerCentroid must be 1 or greater!"},
      {R"({"type":"vector","fields":["a"],)"
       R"("params":{"dimension":2,"metric":"l2","nProbe":1}})"_vpack,
       "Error with parsing the `params` attribute in vector index "
       "definition: Found unexpected attribute 'nProbe'"},
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

TEST_F(StorageEngineVectorIndexTest, UnparseableFactoryStringIsRejected) {
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

TEST_F(StorageEngineVectorIndexTest,
       MatchesDefinitionComparesFieldsFlagsAndParams) {
  auto index = createNormalized(
      R"({"type":"vector","fields":["embedding"],)"
      R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack);
  ASSERT_NE(index, nullptr);

  struct Case {
    VPackString definition;
    bool matches;
  };
  std::vector<Case> const cases{
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
       true},
      // params are compared after parsing, so explicit defaults match
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1,)"
       R"("trainingIterations":25,"defaultNProbe":1,)"
       R"("numberOfDocsPerCentroid":100}})"_vpack,
       true},
      {R"({"type":"vector","fields":["embedding"],"name":"other",)"
       R"("sparse":false,"unique":false,)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
       true},
      {R"({"type":"vector","fields":["embedding"],"storedValues":["x"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
       true},
      {R"({"type":"vector","fields":["other"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],"sparse":true,)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],"unique":true,)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":3,"metric":"l2","nLists":1}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"cosine","nLists":1}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":2}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2"}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1,)"
       R"("trainingIterations":10}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1,)"
       R"("defaultNProbe":2}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1,)"
       R"("numberOfDocsPerCentroid":50}})"_vpack,
       false},
      {R"({"type":"vector","fields":["embedding"],)"
       R"("params":{"dimension":2,"metric":"l2","nLists":1,)"
       R"("factory":"IVF1,Flat"}})"_vpack,
       false},
  };

  for (auto const& c : cases) {
    EXPECT_EQ(index->matchesDefinition(c.definition), c.matches)
        << c.definition.slice().toJson();
  }
}

TEST_F(StorageEngineVectorIndexTest,
       MatchesDefinitionShortCircuitsOnIdButStillComparesParams) {
  auto const definition =
      R"({"type":"vector","fields":["embedding"],)"
      R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack;
  auto index = createNormalized(definition);
  ASSERT_NE(index, nullptr);
  auto const id = std::to_string(index->id().id());
  auto const otherId = std::to_string(index->id().id() + 1);

  EXPECT_TRUE(index->matchesDefinition(
      withId(R"({"type":"vector","fields":["other"],"sparse":true,)"
             R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
             VPackValue(id))
          .slice()));
  EXPECT_FALSE(index->matchesDefinition(
      withId(R"({"type":"vector","fields":["embedding"],)"
             R"("params":{"dimension":3,"metric":"l2","nLists":1}})"_vpack,
             VPackValue(id))
          .slice()));
  EXPECT_FALSE(index->matchesDefinition(
      withId(definition, VPackValue(otherId)).slice()));
  // a numeric id never matches
  EXPECT_FALSE(index->matchesDefinition(
      withId(definition, VPackValue(index->id().id())).slice()));
}

TEST_F(StorageEngineVectorIndexTest,
       EnsuringMatchingDefinitionReturnsExistingIndex) {
  auto index = createNormalized(
      R"({"type":"vector","fields":["embedding"],)"
      R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack);
  ASSERT_NE(index, nullptr);

  VPackBuilder normalized;
  ASSERT_TRUE(IsOk(
      normalize(R"({"type":"vector","name":"other","fields":["embedding"],)"
                R"("storedValues":["x"],)"
                R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack,
                normalized)));
  bool created = true;
  auto existing =
      _collection->createIndex(normalized.slice(), created).waitAndGet();

  EXPECT_FALSE(created);
  ASSERT_NE(existing, nullptr);
  EXPECT_EQ(existing->id(), index->id());
}
