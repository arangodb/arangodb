////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <velocypack/Builder.h>

#include "Containers/Enumerate.h"
#include "Random/RandomGenerator.h"
#include "Restore/RestoreFeature.h"
#include "Utils/QuickGen.h"

using namespace arangodb;
using namespace arangodb::tests;

// @brief a regression test of a dump which was not restored in the right order
//        regarding distributeShardsLike, due to a bug in the compare function.
TEST(CollectionRestoreOrder, regression1) {
  auto const colsJson = {
      R"json({"parameters":{"name":"Comment_hasTag_Tag_Smart","type":3,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Comment_Smart","type":2,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Forum_hasMember_Person","type":3}})json",
      R"json({"parameters":{"name":"Forum_hasTag_Tag","type":3}})json",
      R"json({"parameters":{"name":"Forum","type":2}})json",
      R"json({"parameters":{"name":"Organisation","type":2}})json",
      R"json({"parameters":{"name":"Person_hasCreated_Comment_Smart","type":3,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Person_hasCreated_Post_Smart","type":3,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Person_hasInterest_Tag","type":3}})json",
      R"json({"parameters":{"name":"Person_knows_Person_Smart","type":3,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Person_likes_Comment_Smart","type":3,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Person_likes_Post_Smart","type":3,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Person_Smart","type":2}})json",
      R"json({"parameters":{"name":"Person_studyAt_University","type":3}})json",
      R"json({"parameters":{"name":"Person_workAt_Company","type":3}})json",
      R"json({"parameters":{"name":"Place","type":2}})json",
      R"json({"parameters":{"name":"Post_hasTag_Tag_Smart","type":3,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"Post_Smart","type":2,"distributeShardsLike":"Person_Smart"}})json",
      R"json({"parameters":{"name":"TagClass","type":2}})json",
      R"json({"parameters":{"name":"Tag","type":2}})json",
  };
  auto collections = std::vector<velocypack::Builder>();
  collections.reserve(colsJson.size());
  std::transform(
      colsJson.begin(), colsJson.end(), std::back_inserter(collections),
      [&](auto const& json) { return *velocypack::Parser::fromJson(json); });
  ASSERT_EQ(colsJson.size(), collections.size());

  // testee
  RestoreFeature::sortCollectionsForCreation(collections);

  ASSERT_EQ(colsJson.size(), collections.size());

  using namespace std::string_view_literals;

  auto const expected = {
      // 1) First all collections without distributeShardsLike
      // 1.1) Vertex collections, lexicographically sorted
      "Forum"sv,
      "Organisation"sv,
      "Person_Smart"sv,
      "Place"sv,
      "Tag"sv,
      "TagClass"sv,
      // 1.2) Edge collections, lexicographically sorted
      "Forum_hasMember_Person"sv,
      "Forum_hasTag_Tag"sv,
      "Person_hasInterest_Tag"sv,
      "Person_studyAt_University"sv,
      "Person_workAt_Company"sv,
      // 2) All collections with distributeShardsLike
      // 1.1) Vertex collections, lexicographically sorted
      "Comment_Smart"sv,
      "Post_Smart"sv,
      // 1.2) Edge collections, lexicographically sorted
      "Comment_hasTag_Tag_Smart"sv,
      "Person_hasCreated_Comment_Smart"sv,
      "Person_hasCreated_Post_Smart"sv,
      "Person_knows_Person_Smart"sv,
      "Person_likes_Comment_Smart"sv,
      "Person_likes_Post_Smart"sv,
      "Post_hasTag_Tag_Smart"sv,
  };

  for (auto const& [idx, name] : enumerate(expected)) {
    auto const& actual =
        collections.at(idx).slice().get("parameters").get("name").stringView();
    EXPECT_EQ(name, actual);
  }
}

struct CollectionName {
  std::string value;
};
static auto operator<<(std::ostream& ostream, CollectionName const& colName)
    -> std::ostream& {
  return ostream << "\"" << colName.value << "\"";
}
struct Collection {
  CollectionName name;
  TRI_col_type_e type = TRI_COL_TYPE_DOCUMENT;
  Collection const* distributeShardsLike = nullptr;

  void toVelocyPack(velocypack::Builder& builder) {
    using namespace velocypack;
    auto collection = ObjectBuilder(&builder);
    builder.add(Value("parameters"));
    {
      auto parameters = ObjectBuilder(&builder);
      builder.add("name", Value(name.value));
      builder.add("type", Value(type));
      if (distributeShardsLike != nullptr) {
        builder.add("distributeShardsLike",
                    Value(distributeShardsLike->name.value));
      }
    }
  }
};
static auto operator<<(std::ostream& ostream, Collection const& col)
    -> std::ostream& {
  ostream << "Collection{ name=" << col.name << ", type=" << col.type;
  if (col.distributeShardsLike != nullptr) {
    ostream << ", distributeShardsLike=" << col.distributeShardsLike->name;
  }
  ostream << " }";
  return ostream;
}
struct CollectionSet {
  // use pointers so we can shuffle collections around, but keep references
  // between them intact
  std::vector<std::unique_ptr<Collection>> collections;
};
static auto operator<<(std::ostream& ostream, CollectionSet const& colSet)
    -> std::ostream& {
  ostream << "[";
  bool first = true;
  for (auto const& col : colSet.collections) {
    if (first) {
      first = false;
      ostream << " ";
    } else {
      ostream << ", ";
    }
    ostream << *col;
  }
  ostream << "]";
  return ostream;
}

namespace arangodb::tests::quick {
template<>
auto generate<CollectionName>() -> CollectionName {
  auto name = std::string();
  // make system collection with p=1/4
  if (RandomGenerator::interval(0, 3) < 1) {
    name += "_";
  }
  // short names suffice for these tests, so stay in SSO
  auto const length = RandomGenerator::interval(1, 14);
  for (auto i = 0; i < length; ++i) {
    name += generate<AlphaNumeric>().c;
  }

  return {name};
}

template<>
auto generate<CollectionSet>(std::int32_t max) -> CollectionSet {
  std::size_t const n = RandomGenerator::interval(0, max);

  auto collections = std::vector<std::unique_ptr<Collection>>();
  collections.reserve(n);

  // generate names until we get n different ones
  auto collectionNames = std::unordered_set<std::string>();
  collectionNames.reserve(n);
  while (collectionNames.size() < n) {
    // generate a name
    auto const& [it, inserted] =
        collectionNames.emplace(generate<CollectionName>().value);
    if (inserted) {
      // if unique, insert into vector
      auto collection = Collection{};
      collection.name.value = *it;
      // generate the type
      collection.type = generate<TRI_col_type_e>();
      collections.emplace_back(std::make_unique<Collection>(collection));
    }
  }

  // every collection but the first gets a distributeShardsLike with p=1/4
  for (std::size_t i = 1; i < n; ++i) {
    if (RandomGenerator::interval(0, 3) < 1) {
      // choose a prototype with a smaller index, so we don't get cycles
      std::size_t protoIdx =
          RandomGenerator::interval(0, static_cast<std::int32_t>(i) - 1);
      auto const* const proto = collections[protoIdx].get();
      // But don't target a collection that already has distributeShardsLike
      // itself. This reduces the actual probability a little.
      if (proto->distributeShardsLike == nullptr) {
        collections[i]->distributeShardsLike = proto;
      }
    }
  }

  // shuffle again to remove the pattern that distributeShardsLike always points
  // "backwards".
  auto urbg = RandomGenerator::UniformRandomGenerator<std::uint64_t>();
  std::shuffle(collections.begin(), collections.end(), urbg);

  return {std::move(collections)};
}
}  // namespace arangodb::tests::quick

auto to_string(std::vector<velocypack::Builder> const& collections)
    -> std::string {
  auto res = std::string();
  res += "[";
  bool first = true;
  for (auto const& it : collections) {
    if (first) {
      first = false;
    } else {
      res += ", ";
    }
    res += it.slice().toJson();
  }
  res += "]";
  return res;
}

// Only checks order regarding distributeShardsLike for now
void checkOrderValidity(std::vector<velocypack::Builder> const& collections) {
  auto const n = collections.size();
  auto colToIdx = std::unordered_map<std::string, std::size_t>();
  colToIdx.reserve(collections.size());
  auto enumCols = enumerate(collections);
  std::transform(
      enumCols.begin(), enumCols.end(), std::inserter(colToIdx, colToIdx.end()),
      [](auto const& it) -> std::pair<std::string, std::size_t> {
        auto const& [idx, builder] = it;
        return {builder.slice().get("parameters").get("name").copyString(),
                idx};
      });
  ASSERT_EQ(n, colToIdx.size()) << to_string(collections);

  for (auto const& [idx, builder] : enumerate(collections)) {
    auto const parametersSlice = builder.slice().get("parameters");
    if (auto const distributeShardsLikeSlice =
            parametersSlice.get("distributeShardsLike");
        !distributeShardsLikeSlice.isNone()) {
      auto const distributeShardsLike = distributeShardsLikeSlice.copyString();
      auto dslIdx = colToIdx.at(distributeShardsLike);
      EXPECT_LT(dslIdx, idx) << to_string(collections);
    }
  }
}

void testSortCollectionsForCreationOn(CollectionSet const& collectionSet) {
  auto const n = collectionSet.collections.size();

  auto collections = std::vector<velocypack::Builder>();
  std::transform(collectionSet.collections.begin(),
                 collectionSet.collections.end(),
                 std::back_inserter(collections), [](auto const& col) {
                   auto builder = velocypack::Builder();
                   col->toVelocyPack(builder);
                   return builder;
                 });
  ASSERT_EQ(n, collections.size()) << "input: " << collectionSet;

  RestoreFeature::sortCollectionsForCreation(collections);

  ASSERT_EQ(n, collections.size()) << "input: " << collectionSet;

  checkOrderValidity(collections);
}

void testRandomIterations(std::int32_t setSize, int iterations) {
  for (auto i = 0; i < iterations; ++i) {
    auto const collectionSet = quick::generate<CollectionSet>(setSize);
    testSortCollectionsForCreationOn(collectionSet);
  }
}

TEST(CollectionRestoreOrder, random10) { testRandomIterations(10, 1'000); }

TEST(CollectionRestoreOrder, random100) { testRandomIterations(100, 100); }

TEST(CollectionRestoreOrder, random1000) { testRandomIterations(1'000, 10); }

TEST(CollectionRestoreOrder, random10000) { testRandomIterations(10'000, 1); }
