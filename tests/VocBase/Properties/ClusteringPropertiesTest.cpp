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
#include "Basics/ResultT.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Properties/ClusteringProperties.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "Inspection/VPack.h"

#include <unordered_map>
#include <velocypack/Builder.h>

namespace arangodb::tests {
class ClusteringPropertiesTest : public ::testing::Test {
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
    }
    return body;
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  static ResultT<ClusteringProperties> parse(VPackSlice body) {
    ClusteringProperties res;
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

  static ResultT<ClusteringProperties> parseWithDefaultOptions(
      VPackSlice body, DatabaseConfiguration const& config) {
    ClusteringProperties res;
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

  static VPackBuilder serialize(ClusteringProperties testee) {
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
    res.shardingStrategy = "hash";
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

TEST_F(ClusteringPropertiesTest, test_minimal_user_input) {
  VPackBuilder body;
  { VPackObjectBuilder bodyBuilder{&body}; }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok()) << testee.errorMessage();

  __HELPER_equalsAfterSerializeParseCircle(testee.get());
}

TEST_F(ClusteringPropertiesTest, test_oneShardDBCannotBeSatellite) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    // has to be greater or equal to used writeConcern
    body.add("replicationFactor", VPackValue("satellite"));
  }

  // Note: We can also make this parsing fail in the first place.
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok()) << testee.result().errorNumber() << " -> "
                           << testee.result().errorMessage();
  EXPECT_EQ(testee->replicationFactor.value(), 0ul);

  // No special config required, this always fails
  auto config = defaultDBConfig();
  config.isOneShardDB = true;
  auto res = testee->applyDefaultsAndValidateDatabaseConfiguration(config);
  EXPECT_FALSE(res.ok())
      << "Configured a oneShardDB collection as 'satellite'.";
}

TEST_F(ClusteringPropertiesTest, test_shardKeyOnSatellites) {
  // We do not need any special configuration
  // default is good enough
  auto config = defaultDBConfig();

  // Sharding by specific shardKey, or prefix/postfix of _key is not allowed
  for (auto const& key : {"testKey", "a", ":_key", "_key:"}) {
    // Specific shardKey is disallowed
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      // has to be greater or equal to used writeConcern
      body.add("name", VPackValue("test"));
      body.add("replicationFactor", VPackValue("satellite"));
      body.add(VPackValue("shardKeys"));
      {
        VPackArrayBuilder arrayGuard{&body};
        body.add(VPackValue("testKey"));
      }
    }
    auto testee = parseWithDefaultOptions(body.slice(), defaultDBConfig());

    EXPECT_FALSE(testee.ok())
        << "Created a satellite collection with a shardkey: " << key;
  }
  {
    // Shard by _key is allowed
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("replicationFactor", VPackValue("satellite"));
      body.add(VPackValue("shardKeys"));
      {
        VPackArrayBuilder arrayGuard{&body};
        body.add(VPackValue("_key"));
      }
    }
    auto testee = parseWithDefaultOptions(body.slice(), defaultDBConfig());
#ifdef USE_ENTERPRISE
    auto result = testee.result();
    EXPECT_TRUE(result.ok())
        << "Failed to created a satellite collection with default sharding "
        << result.errorMessage();
#else
    auto result = testee.result();
    EXPECT_FALSE(result.ok())
        << "Created a 'satellite' collection in community edition. "
        << result.errorMessage();
#endif
  }

  {
    // Shard by _key + something is not allowed
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      // has to be greater or equal to used writeConcern
      body.add("name", VPackValue("test"));
      body.add("replicationFactor", VPackValue("satellite"));
      body.add(VPackValue("shardKeys"));
      {
        VPackArrayBuilder arrayGuard{&body};
        body.add(VPackValue("_key"));
        body.add(VPackValue("testKey"));
      }
    }
    auto testee = parseWithDefaultOptions(body.slice(), defaultDBConfig());
    auto result = testee.result();
    EXPECT_FALSE(result.ok())
        << "Created a satellite collection with a shardkeys [_key, testKey]";
  }
}

TEST_F(ClusteringPropertiesTest, test_satellite) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("replicationFactor", VPackValue("satellite"));
  }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  auto config = defaultDBConfig();
  auto res = testee->applyDefaultsAndValidateDatabaseConfiguration(config);
#if USE_ENTERPRISE
  ASSERT_TRUE(res.ok()) << "Failed with " << res.fail();
  EXPECT_TRUE(testee->isSatellite());
  ASSERT_TRUE(testee->writeConcern.has_value());
  EXPECT_EQ(testee->writeConcern.value(), 1ull);
  ASSERT_TRUE(testee->numberOfShards.has_value());
  EXPECT_EQ(testee->numberOfShards.value(), 1ull);
  __HELPER_equalsAfterSerializeParseCircle(testee.get());
#else
  EXPECT_FALSE(res.ok())
      << "Created a 'satellite' collection in community edition. "
      << res.errorMessage();
#endif
}

TEST_F(ClusteringPropertiesTest, test_satellite_numberOfShards_forbidden) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("numberOfShards", VPackValue(3));
  }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  auto config = defaultDBConfig();
  auto res = testee->applyDefaultsAndValidateDatabaseConfiguration(config);
  ASSERT_FALSE(res.ok()) << "Allowed illegal: " << body.toJson();
}

TEST_F(ClusteringPropertiesTest, test_satellite_numberOfShards_allowed) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("numberOfShards", VPackValue(1));
  }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  auto config = defaultDBConfig();
  auto res = testee->applyDefaultsAndValidateDatabaseConfiguration(config);
#ifdef USE_ENTERPRISE
  ASSERT_TRUE(res.ok()) << "Failed with " << res.fail();
  EXPECT_TRUE(testee->isSatellite());
  ASSERT_TRUE(testee->writeConcern.has_value());
  EXPECT_EQ(testee->writeConcern.value(), 1ull);
  ASSERT_TRUE(testee->numberOfShards.has_value());
  EXPECT_EQ(testee->numberOfShards.value(), 1ull);
  __HELPER_equalsAfterSerializeParseCircle(testee.get());
#else
  EXPECT_FALSE(res.ok())
      << "Created a 'satellite' collection in community edition. "
      << res.errorMessage();
#endif
}

TEST_F(ClusteringPropertiesTest, test_satellite_writeConcern_forbidden) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("writeConcern", VPackValue(3));
  }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  auto config = defaultDBConfig();
  auto res = testee->applyDefaultsAndValidateDatabaseConfiguration(config);
  ASSERT_FALSE(res.ok()) << "Allowed illegal: " << body.toJson();
}

TEST_F(ClusteringPropertiesTest, test_satellite_writeConcern_forbidden_0) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("writeConcern", VPackValue(0));
  }
  auto testee = parse(body.slice());
  EXPECT_FALSE(testee.ok()) << "Allowed illegal: " << body.toJson();
}

TEST_F(ClusteringPropertiesTest, test_satellite_writeConcern_allowed) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("writeConcern", VPackValue(1));
  }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  auto config = defaultDBConfig();
  auto res = testee->applyDefaultsAndValidateDatabaseConfiguration(config);
#ifdef USE_ENTERPRISE
  ASSERT_TRUE(res.ok()) << "Failed with " << res.fail();
  EXPECT_TRUE(testee->isSatellite());
  ASSERT_TRUE(testee->writeConcern.has_value());
  EXPECT_EQ(testee->writeConcern.value(), 1ull);
  ASSERT_TRUE(testee->numberOfShards.has_value());
  EXPECT_EQ(testee->numberOfShards.value(), 1ull);
  __HELPER_equalsAfterSerializeParseCircle(testee.get());
#else
  EXPECT_FALSE(res.ok())
      << "Created a 'satellite' collection in community edition. "
      << res.errorMessage();
#endif
}

}  // namespace arangodb::tests
