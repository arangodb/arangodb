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

#include "Restore/RestoreFeature.h"

#include <Containers/Enumerate.h>
#include <velocypack/Builder.h>

using namespace arangodb;

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
  std::transform(colsJson.begin(), colsJson.end(),
                 std::back_inserter(collections), [&](auto const& json) {
                   return *velocypack::Parser::fromJson(json);
                 });
  ASSERT_EQ(colsJson.size(), collections.size());

  // testee
  RestoreFeature::sortCollectionsForCreation(collections);

  ASSERT_EQ(colsJson.size(), collections.size());

  using namespace std::string_view_literals;

  auto const expected = {
      // The distributeShardsLike target comes first
      "Person_Smart"sv,
      // Then all (other) vertex collections, lexicographically sorted
      "Comment_Smart"sv,
      "Forum"sv,
      "Organisation"sv,
      "Place"sv,
      "Post_Smart"sv,
      "Tag"sv,
      "TagClass"sv,
      // Finally all edge collections, lexicographically sorted
      "Comment_hasTag_Tag_Smart"sv,
      "Forum_hasMember_Person"sv,
      "Forum_hasTag_Tag"sv,
      "Person_hasCreated_Comment_Smart"sv,
      "Person_hasCreated_Post_Smart"sv,
      "Person_hasInterest_Tag"sv,
      "Person_knows_Person_Smart"sv,
      "Person_likes_Comment_Smart"sv,
      "Person_likes_Post_Smart"sv,
      "Person_studyAt_University"sv,
      "Person_workAt_Company"sv,
      "Post_hasTag_Tag_Smart"sv,
  };

  for (auto const& [idx, name] : enumerate(expected)) {
    auto const& actual =
        collections.at(idx).slice().get("parameters").get("name").stringView();
    EXPECT_EQ(name, actual);
  }
}