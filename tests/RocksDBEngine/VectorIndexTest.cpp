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

#include "Indexes/VectorIndexDefinitions.h"
#include "RocksDBEngine/StorageEngineIndexTest.h"

#include "Basics/ResultAssertions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackAssertions.h"
#include "Inspection/VPack.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "VectorIndex/Definition.h"

#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::velocypack;

namespace {

VPackBuilder toVPack(Index const& index, Index::Serialize flag) {
  VPackBuilder builder;
  index.toVelocyPack(builder, Index::makeFlags(flag));
  return builder;
}

VPackBuilder withId(VPackSlice definition, VPackValue id) {
  VPackBuilder idObject;
  idObject.openObject();
  idObject.add(StaticStrings::IndexId, id);
  idObject.close();
  return VPackCollection::merge(definition, idObject.slice(),
                                /*mergeValues*/ false);
}

// frozen on-disk format from when trainedData lived in the definition
VPackBuilder preMigrationMarker() {
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
  return marker;
}

}  // namespace

// makeIndex skips enhanceIndexDefinition, where the vector checks live

class StorageEngineVectorIndexTest : public StorageEngineIndexTest {
 protected:
  Result normalize(VPackSlice definition, VPackBuilder& normalized) {
    return engine().indexFactory().enhanceIndexDefinition(
        definition, normalized, /*isCreation*/ true, *_database);
  }

  std::shared_ptr<Index> createNormalized(VPackSlice definition) {
    VPackBuilder normalized;
    auto const res = normalize(definition, normalized);
    EXPECT_TRUE(IsOk(res));
    return res.ok() ? makeIndex(normalized.slice()) : nullptr;
  }

  std::shared_ptr<Index> loadIndex(VPackSlice marker) {
    return engine().indexFactory().prepareIndexFromSlice(
        marker, /*generateKey*/ false, *_collection,
        /*isClusterConstructor*/ true);
  }
};

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
  // frozen on-disk format, fix the reader rather than this literal
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
  auto index = loadIndex(preMigrationMarker().slice());
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

TEST_F(StorageEngineVectorIndexTest, StoredValuesKeepTheirOrderThroughReload) {
  // entries keep the values by position, a reorder would misread them
  auto const expected = R"(["b.c","a"])"_vpack;
  auto index = createNormalized(
      R"({"type":"vector","fields":["embedding"],"storedValues":["b.c","a"],)"
      R"("params":{"dimension":2,"metric":"l2","nLists":1}})"_vpack);
  ASSERT_NE(index, nullptr);

  auto marker = toVPack(*index, Index::Serialize::Internals);
  EXPECT_TRUE(EqualsVPack(
      expected, marker.slice().get(StaticStrings::IndexStoredValues)));

  auto reloaded = loadIndex(marker.slice());
  ASSERT_NE(reloaded, nullptr);
  EXPECT_TRUE(
      EqualsVPack(expected, toVPack(*reloaded, Index::Serialize::Internals)
                                .slice()
                                .get(StaticStrings::IndexStoredValues)));
}

TEST_F(StorageEngineVectorIndexTest, ErrorMessageOnlyWhileUnusable) {
  auto untrained = createNormalized(minimalVectorDefinition());
  ASSERT_NE(untrained, nullptr);
  EXPECT_TRUE(toVPack(*untrained, Index::Serialize::Basics)
                  .slice()
                  .hasKey(StaticStrings::ErrorMessage));

  auto trained = loadIndex(preMigrationMarker().slice());
  ASSERT_NE(trained, nullptr);
  auto out = toVPack(*trained, Index::Serialize::Basics);
  EXPECT_EQ(out.slice().get(StaticStrings::IndexTrainingState).stringView(),
            StaticStrings::IndexTrainingStateReady);
  EXPECT_FALSE(out.slice().hasKey(StaticStrings::ErrorMessage));
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
