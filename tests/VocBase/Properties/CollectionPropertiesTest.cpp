////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "InspectTestHelperMakros.h"

#include "Basics/debugging.h"
#include "Basics/ResultT.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "Inspection/VPack.h"

#include <unordered_map>
#include <velocypack/Builder.h>

namespace arangodb::tests {
class CollectionPropertiesTest : public ::testing::Test {
 protected:
  // Returns minimal, valid JSON object for the struct to test.
  // Only the given attributeName has the given value.
  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      if constexpr (std::is_same_v<T, VPackSlice>) {
        body.add(attributeName, attributeValue);
      } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        body.add(VPackValue(attributeName));
        VPackArrayBuilder arrayGuard(&body);
        for (auto const& val : attributeValue) {
          body.add(VPackValue(val));
        }
      } else {
        body.add(attributeName, VPackValue(attributeValue));
      }
      if (attributeName != StaticStrings::DataSourceName) {
        // We need to always have a Name.
        body.add(StaticStrings::DataSourceName, VPackValue("test"));
      }
    }
    return body;
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  static ResultT<UserInputCollectionProperties> parse(VPackSlice body) {
    UserInputCollectionProperties res;
    try {
      auto status = velocypack::deserializeWithStatus(body, res, {},
                                                      InspectUserContext{});
      if (!status.ok()) {
        return Result{
            TRI_ERROR_BAD_PARAMETER,
            status.error() +
                (status.path().empty() ? "" : " on path " + status.path())};
      }
      return res;
    } catch (basics::Exception const& e) {
      return Result{e.code(), e.message()};
    } catch (std::exception const& e) {
      return Result{TRI_ERROR_INTERNAL, e.what()};
    }
  }

  static ResultT<UserInputCollectionProperties> parseWithDefaultOptions(
      VPackSlice body, DatabaseConfiguration const& config) {
    UserInputCollectionProperties res;
    try {
      auto status = velocypack::deserializeWithStatus(body, res, {},
                                                      InspectUserContext{});
      if (!status.ok()) {
        return Result{
            TRI_ERROR_BAD_PARAMETER,
            status.error() +
                (status.path().empty() ? "" : " on path " + status.path())};
      }
      auto result = res.applyDefaultsAndValidateDatabaseConfiguration(config);
      if (result.fail()) {
        return result;
      }
      return res;
    } catch (basics::Exception const& e) {
      return Result{e.code(), e.message()};
    } catch (std::exception const& e) {
      return Result{TRI_ERROR_INTERNAL, e.what()};
    }
  }

  static VPackBuilder serialize(UserInputCollectionProperties testee) {
    VPackBuilder result;
    velocypack::serializeWithContext(result, testee, InspectUserContext{});
    return result;
  }

  static UserInputCollectionProperties defaultLeaderProps() {
    UserInputCollectionProperties res;
    res.numberOfShards = 12;
    res.replicationFactor = 3;
    res.writeConcern = 2;
    res.id = DataSourceId{42};
    return res;
  }

  static DatabaseConfiguration defaultDBConfig(
      std::unordered_map<std::string, UserInputCollectionProperties> lookupMap =
          {}) {
    return DatabaseConfiguration{
        []() { return DataSourceId(42); },
        [lookupMap = std::move(lookupMap)](
            std::string const& name) -> ResultT<UserInputCollectionProperties> {
          // Set a lookup method
          if (!lookupMap.contains(name)) {
            return {TRI_ERROR_INTERNAL};
          }
          return lookupMap.at(name);
        }};
  }
};

TEST_F(CollectionPropertiesTest, disallowed_autoincrement_with_many_shards) {
  auto prepareBody = [&](uint64_t numberOfShards) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add(VPackValue("keyOptions"));
      {
        VPackObjectBuilder keyGuard(&body);
        body.add("allowUserKeys", VPackValue(true));
        body.add("type", VPackValue("autoincrement"));
      }
      body.add("numberOfShards", VPackValue(numberOfShards));
    }
    return body.sharedSlice();
  };

  // Allowed values (1 shard)
  {
    auto body = prepareBody(1);
    auto res = parseWithDefaultOptions(body.slice(), defaultDBConfig());
    // NOTE: This test used a minimal example for keyOptions.
    // As soon as keyOptions use a struct and not a slice in parsing, we may
    // need to adapt other properties, than the 'type'
    EXPECT_TRUE(res.ok()) << "Failed to let valid collection pass: "
                          << res.errorMessage() << " body: " << body.toJson();
  }

  // Disallowed values (0 shards, and more than one):
  for (uint64_t numberOfShards : {0, 2, 5, 32}) {
    auto body = prepareBody(numberOfShards);
    auto res = parseWithDefaultOptions(body.slice(), defaultDBConfig());
    EXPECT_FALSE(res.ok()) << "Let illegal properties pass: " << body.toJson();
  }
}

TEST_F(CollectionPropertiesTest, test_atMost8ShardKeys) {
  // We split this string up into characters, to use those as shardKey
  // attributes Just for simplicity reasons, and to avoid having duplicates
  std::string shardKeySelection = "abcdefghijklm";

  std::vector<std::string> shardKeysToTest{};
  for (size_t i = 0; i < 8; ++i) {
    // Always add one character from above string, no character is used twice
    shardKeysToTest.emplace_back(shardKeySelection.substr(i, 1));
    auto body = createMinimumBodyWithOneValue("shardKeys", shardKeysToTest);
    // The first 8 have to be allowed
    auto testee = parseWithDefaultOptions(body.slice(), defaultDBConfig());

    ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
    EXPECT_EQ(testee->shardKeys, shardKeysToTest)
        << "Parsing error in " << body.toJson();
  }

  for (size_t i = 8; i < 10; ++i) {
    // Always add one character from above string, no character is used twice
    shardKeysToTest.emplace_back(shardKeySelection.substr(i, 1));
    auto body = createMinimumBodyWithOneValue("shardKeys", shardKeysToTest);

    // The first 8 have to be allowed
    auto testee = parseWithDefaultOptions(body.slice(), defaultDBConfig());

    EXPECT_FALSE(testee.ok())
        << "Created too many shard keys: " << shardKeysToTest.size();
  }
}

TEST_F(CollectionPropertiesTest, test_internal_values_as_shardkeys) {
  // Sharding by internal keys, or prefix/postfix of them is not allowed
  for (auto const& key : {"_id", "_rev", ":_id", "_id:", ":_rev", "_rev:"}) {
    // Specific shardKey is disallowed
    auto body = createMinimumBodyWithOneValue("shardKeys",
                                              std::vector<std::string>{key});
    auto testee = parseWithDefaultOptions(body.slice(), defaultDBConfig());
    EXPECT_FALSE(testee.ok()) << "Created a collection with shardkey: " << key;
  }
}

}  // namespace arangodb::tests
