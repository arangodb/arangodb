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

#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Inspection/VPack.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

#include "InspectTestHelperMakros.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {

/**********************
 * TEST SECTION
 *********************/

class CreateCollectionBodyTest : public ::testing::Test {
 protected:
  /// @brief this generates the minimal required body, only exchanging one
  /// attribute with the given value should work on all basic types
  /// if your attribute is "name" you will get a body back only with the name,
  /// otherwise there will be a body with a valid name + your given value.
  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    std::string colName = "test";
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      if (attributeName != "name") {
        body.add("name", VPackValue(colName));
      }
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

  static VPackBuilder serialize(CreateCollectionBody testee) {
    VPackBuilder result;
    velocypack::serializeWithContext(result, testee, InspectUserContext{});
    return result;
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

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  static ResultT<CreateCollectionBody> parse(
      VPackSlice body,
      DatabaseConfiguration const& config = defaultDBConfig()) {
    return CreateCollectionBody::fromCreateAPIBody(body, config);
  }

  static void assertParsingThrows(VPackBuilder const& body) {
    auto p = CreateCollectionBody::fromCreateAPIBody(body.slice(),
                                                     defaultDBConfig());
    EXPECT_TRUE(p.fail()) << " On body " << body.toJson();
  }

  static UserInputCollectionProperties defaultLeaderProps() {
    UserInputCollectionProperties res;
    res.numberOfShards = 12;
    res.replicationFactor = 3;
    res.writeConcern = 2;
    res.id = DataSourceId{42};
    res.shardingStrategy = "hash";
    res.shardKeys = std::vector<std::string>{StaticStrings::KeyString};
    return res;
  }
};

TEST_F(CreateCollectionBodyTest, test_requires_some_input) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  assertParsingThrows(body);
}

TEST_F(CreateCollectionBodyTest, test_minimal_user_input) {
  std::string colName = "test";
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue(colName));
  }
  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), defaultDBConfig());

  ASSERT_TRUE(testee.ok()) << testee.errorMessage();
  // Test Default values

  // This covers only non-documented APIS
  EXPECT_TRUE(testee->avoidServers.empty());

  __HELPER_equalsAfterSerializeParseCircle(testee.get());
}

TEST_F(CreateCollectionBodyTest,
       test_writeConcernWinsVersusminReplicationFactor) {
  std::string colName = "test";
  {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue(colName));
      body.add("writeConcern", VPackValue(3));
      body.add("minReplicationFactor", VPackValue(5));
      // has to be greater or equal to used writeConcern
      body.add("replicationFactor", VPackValue(4));
    }

    auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(),
                                                          defaultDBConfig());
    ASSERT_TRUE(testee.ok()) << testee.result().errorNumber() << " -> "
                             << testee.result().errorMessage();
    ASSERT_TRUE(testee->writeConcern.has_value());
    EXPECT_EQ(testee->writeConcern.value(), 3ul);
  }
  {
    // We change order of attributes in the input vpack
    // to ensure ordering in json does not have a subtle impact
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      // has to be greater or equal to used writeConcern
      body.add("name", VPackValue(colName));
      body.add("replicationFactor", VPackValue(4));
      body.add("minReplicationFactor", VPackValue(5));
      body.add("writeConcern", VPackValue(3));
    }

    auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(),
                                                          defaultDBConfig());
    ASSERT_TRUE(testee.ok()) << testee.result().errorNumber() << " -> "
                             << testee.result().errorMessage();
    ASSERT_TRUE(testee->writeConcern.has_value());
    EXPECT_EQ(testee->writeConcern.value(), 3ul);
  }
}

TEST_F(CreateCollectionBodyTest, test_satelliteReplicationFactor) {
  auto shouldBeEvaluatedTo = [&](VPackBuilder const& body, uint64_t number) {
    auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(),
                                                          defaultDBConfig());
    ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
    ASSERT_TRUE(testee->replicationFactor.has_value());
    EXPECT_EQ(testee->replicationFactor.value(), number)
        << "Parsing error in " << body.toJson();
  };

  // Special handling for "satellite" string
  shouldBeEvaluatedTo(
      createMinimumBodyWithOneValue("replicationFactor", "satellite"), 0);
}

TEST_F(CreateCollectionBodyTest, test_configureMaxNumberOfShards) {
  auto body = createMinimumBodyWithOneValue("numberOfShards", 1024);

  DatabaseConfiguration config = defaultDBConfig();
  EXPECT_EQ(config.maxNumberOfShards, 0ul);
  EXPECT_EQ(config.shouldValidateClusterSettings, false);

  {
    config.shouldValidateClusterSettings = false;
    // If shouldValidateClusterSettings is false, numberOfShards should not have
    // effect
    for (uint32_t maxShards : std::vector<uint32_t>{0, 16, 1023, 1024, 1025}) {
      config.maxNumberOfShards = maxShards;
      auto testee =
          CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
      ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
      ASSERT_TRUE(testee->numberOfShards.has_value());
      EXPECT_EQ(testee->numberOfShards, 1024ul)
          << "Parsing error in " << body.toJson();
    }
  }
  {
    config.shouldValidateClusterSettings = true;
    // If shouldValidateClusterSettings is true, numberOfShards should be
    // checked
    // Positive cases:
    // 0 := unlimited shards, 1024 should be okay
    // 1024 == 1024 should be okay
    // 1025 >= 1024 should be okay
    for (uint32_t maxShards : std::vector<uint32_t>{0, 1024, 1025}) {
      config.maxNumberOfShards = maxShards;
      auto testee =
          CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
      ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
      ASSERT_TRUE(testee->numberOfShards.has_value());
      EXPECT_EQ(testee->numberOfShards, 1024ul)
          << "Parsing error in " << body.toJson();
    }
    {
      // 16 < 1024 should fail
      config.maxNumberOfShards = 16;
      auto testee =
          CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
      EXPECT_FALSE(testee.ok())
          << "Configured " << config.maxNumberOfShards << " but "
          << testee->numberOfShards.value() << "passed.";
    }
  }
}

TEST_F(CreateCollectionBodyTest, test_isSmartCannotBeSatellite) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    // has to be greater or equal to used writeConcern
    body.add("name", VPackValue("test"));
    body.add("isSmart", VPackValue(true));
    body.add("replicationFactor", VPackValue("satellite"));
  }

  // Note: We can also make this parsing fail in the first place.
  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), defaultDBConfig());
  EXPECT_FALSE(testee.ok()) << "Configured smartCollection as 'satellite'.";
}

TEST_F(CreateCollectionBodyTest, test_distributeShardsLike_default) {
  // We do not need any special configuration
  // default is good enough
  std::string defaultShardBy = "_graphs";
  auto leader = defaultLeaderProps();
  auto config = defaultDBConfig({{defaultShardBy, leader}});
  config.defaultDistributeShardsLike = defaultShardBy;

  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("name", VPackValue("test"));
  }
  auto testee = parse(body.slice(), config);
  // Default value should be taken if none is set
  ASSERT_TRUE(testee.ok()) << "Failed on " << testee.errorMessage();
  EXPECT_EQ(testee->distributeShardsLike.value(), defaultShardBy);
  EXPECT_EQ(testee->numberOfShards.value(), leader.numberOfShards.value());
  EXPECT_EQ(testee->replicationFactor.value(),
            leader.replicationFactor.value());
  EXPECT_EQ(testee->writeConcern.value(), leader.writeConcern.value());
}

TEST_F(CreateCollectionBodyTest,
       test_distributeShardsLike_default_other_values) {
  // We do not need any special configuration
  // default is good enough
  std::string defaultShardBy = "_graphs";
  auto leader = defaultLeaderProps();
  auto config = defaultDBConfig({{defaultShardBy, leader}});
  config.defaultDistributeShardsLike = defaultShardBy;

  for (auto const& attribute : {"writeConcern", "replicationFactor",
                                "numberOfShards", "minReplicationFactor"}) {
    // 4 is not used by any of the above attributes
    auto body = createMinimumBodyWithOneValue(attribute, 4);
    auto testee = parse(body.slice(), config);
    // Default value should be taken if none is set
    EXPECT_FALSE(testee.ok())
        << "Managed to overwrite value '" << attribute
        << "' given by distributeShardsLike body: " << body.toJson();
  }
}

TEST_F(CreateCollectionBodyTest,
       test_distributeShardsLike_default_same_values) {
  // We do not need any special configuration
  // default is good enough
  std::string defaultShardBy = "_graphs";
  auto leader = defaultLeaderProps();
  auto config = defaultDBConfig({{defaultShardBy, leader}});
  config.defaultDistributeShardsLike = defaultShardBy;

  VPackBuilder body;
  {
    VPackObjectBuilder bodyBuilder{&body};
    body.add("name", VPackValue("test"));
    body.add("numberOfShards", VPackValue(leader.numberOfShards.value()));
    body.add("replicationFactor", VPackValue(leader.replicationFactor.value()));
    body.add("writeConcern", VPackValue(leader.writeConcern.value()));
  }

  auto testee = parse(body.slice(), config);
  // Default value should be taken if none is set
  ASSERT_TRUE(testee.ok()) << "Failed on " << testee.errorMessage();
  EXPECT_EQ(testee->distributeShardsLike.value(), defaultShardBy);
  EXPECT_EQ(testee->numberOfShards.value(), leader.numberOfShards.value());
  EXPECT_EQ(testee->replicationFactor.value(),
            leader.replicationFactor.value());
  EXPECT_EQ(testee->writeConcern.value(), leader.writeConcern.value());
}

TEST_F(CreateCollectionBodyTest, test_distributeShardsLike_default_ownValue) {
  // We do not need any special configuration
  // default is good enough
  std::string defaultShardBy = "_graphs";
  auto config = defaultDBConfig();
  config.defaultDistributeShardsLike = defaultShardBy;
  // OneShard and DistributeShardsLike only show up in pairs.
  config.isOneShardDB = true;

  auto body = createMinimumBodyWithOneValue("distributeShardsLike", "test");
  auto testee = parse(body.slice(), config);
  // Default value should be taken if none is set
  EXPECT_FALSE(testee.ok())
      << "Managed to set own distributeShardsLike and override DB setting";
}

TEST_F(CreateCollectionBodyTest, test_oneShard_forcesDistributeShardsLike) {
  // We do not need any special configuration
  // default is good enough
  std::string defaultShardBy = "_graphs";
  auto config = defaultDBConfig();
  config.defaultDistributeShardsLike = defaultShardBy;
  config.isOneShardDB = true;

  // Specific shardKey is disallowed
  auto body = createMinimumBodyWithOneValue("distributeShardsLike", "test");
  auto testee = parse(body.slice(), config);
  EXPECT_FALSE(testee.ok())
      << "Distribute shards like violates oneShard database";
}

TEST_F(CreateCollectionBodyTest, test_oneShard_moreShards) {
  // Configure oneShardDB properly
  std::string defaultShardBy = "_graphs";
  auto config = defaultDBConfig();
  config.defaultDistributeShardsLike = defaultShardBy;
  config.isOneShardDB = true;

  // Specific shardKey is disallowed
  auto body = createMinimumBodyWithOneValue("numberOfShards", 5);
  auto testee = parse(body.slice(), config);
  EXPECT_FALSE(testee.ok()) << "Number of Shards violates oneShard database";
}

TEST_F(CreateCollectionBodyTest, test_isSmartChildCannotBeSatellite) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    // has to be greater or equal to used writeConcern
    body.add("name", VPackValue("test"));
    body.add("isSmartChild", VPackValue(true));
    body.add("replicationFactor", VPackValue("satellite"));
  }

  // Note: We can also make this parsing fail in the first place.
  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), defaultDBConfig());
  EXPECT_FALSE(testee.ok())
      << "Configured smartChild collection as 'satellite'.";
}

TEST_F(CreateCollectionBodyTest, test_smartJoinAttribute_cannot_be_empty) {
  auto config = defaultDBConfig();

  // Specific shardKey is disallowed
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::SmartJoinAttribute, "");
  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  // This could already fail, as soon as we have a context
  EXPECT_FALSE(testee.ok()) << "Let an empty smartJoinAttribute through";
}

// Tests for generic attributes without special needs

namespace {
enum AllowedFlags : uint8_t {
  Allways = 0,
  Disallowed = 1 << 0,
  AsSystem = 1 << 1,
  WithExtension = 1 << 2
};

struct CollectionNameTestParam {
  std::string name;
  uint8_t allowedFlags;
  std::string disallowReason;
};
}  // namespace
class PlanCollectionNamesTest
    : public ::testing::TestWithParam<CollectionNameTestParam> {
 protected:
  [[nodiscard]] std::string getName() const {
    auto p = GetParam();
    return p.name;
  }

  [[nodiscard]] std::string getErrorReason() const {
    auto p = GetParam();
    return p.disallowReason + " on collection " + p.name;
  }

  [[nodiscard]] bool isDisAllowedInGeneral() const {
    auto p = GetParam();
    return p.allowedFlags & AllowedFlags::Disallowed;
  };

  [[nodiscard]] bool requiresSystem() const {
    auto p = GetParam();
    return p.allowedFlags & AllowedFlags::AsSystem;
  };

  [[nodiscard]] bool requiresExtendedNames() const {
    auto p = GetParam();
    return p.allowedFlags & AllowedFlags::WithExtension;
  };

  static DatabaseConfiguration defaultDBConfig() {
    return DatabaseConfiguration{
        []() { return DataSourceId(42); },
        [](std::string const&) { return Result{TRI_ERROR_INTERNAL}; }};
  }
};

INSTANTIATE_TEST_CASE_P(
    PlanCollectionNamesTest, PlanCollectionNamesTest,
    ::testing::Values(
        CollectionNameTestParam{"", AllowedFlags::Disallowed,
                                "name cannot be empty"},
        CollectionNameTestParam{"test", AllowedFlags::Allways, ""},
        CollectionNameTestParam{std::string(256, 'x'), AllowedFlags::Allways,
                                "maximum allowed length"},
        CollectionNameTestParam{std::string(257, 'x'), AllowedFlags::Disallowed,
                                "above maximum allowed length"},
        CollectionNameTestParam{"_test", AllowedFlags::AsSystem,
                                "_ at the beginning requires system"},

        CollectionNameTestParam{"Десятую", AllowedFlags::WithExtension,
                                "non-ascii characters"},
        CollectionNameTestParam{"💩🍺🌧t⛈c🌩_⚡🔥💥🌨",
                                AllowedFlags::WithExtension,
                                "non-ascii characters"},
        CollectionNameTestParam{
            "_💩🍺🌧t⛈c🌩_⚡🔥💥🌨",
            AllowedFlags::AsSystem | AllowedFlags::WithExtension,
            "non-ascii and system"}));

TEST_P(PlanCollectionNamesTest, test_allowed_without_flags) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyGuard{&body};
    body.add("name", VPackValue(getName()));
  }
  auto config = defaultDBConfig();
  EXPECT_EQ(config.allowExtendedNames, false);

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed =
      !isDisAllowedInGeneral() && !requiresSystem() && !requiresExtendedNames();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->name, getName()) << "Parsing error in " << body.toJson();
  } else {
    EXPECT_FALSE(result.ok()) << getErrorReason();
  }
}

TEST_P(PlanCollectionNamesTest, test_allowed_with_isSystem_flag) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyGuard{&body};
    body.add("name", VPackValue(getName()));
    body.add("isSystem", VPackValue(true));
  }
  auto config = defaultDBConfig();
  EXPECT_EQ(config.allowExtendedNames, false);

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed = !isDisAllowedInGeneral() && !requiresExtendedNames();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->name, getName()) << "Parsing error in " << body.toJson();
  } else {
    EXPECT_FALSE(result.ok()) << getErrorReason();
  }
}

TEST_P(PlanCollectionNamesTest, test_allowed_with_extendendNames_flag) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyGuard{&body};
    body.add("name", VPackValue(getName()));
  }
  auto config = defaultDBConfig();
  config.allowExtendedNames = true;

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed = !isDisAllowedInGeneral() && !requiresSystem();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->name, getName()) << "Parsing error in " << body.toJson();
  } else {
    EXPECT_FALSE(result.ok()) << getErrorReason();
  }
}

TEST_P(PlanCollectionNamesTest,
       test_allowed_with_isSystem_andextendedNames_flag) {
  VPackBuilder body;
  {
    VPackObjectBuilder bodyGuard{&body};
    body.add("name", VPackValue(getName()));
    body.add("isSystem", VPackValue(true));
  }
  auto config = defaultDBConfig();
  config.allowExtendedNames = true;

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed = !isDisAllowedInGeneral();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->name, getName()) << "Parsing error in " << body.toJson();
  } else {
    EXPECT_FALSE(result.ok()) << getErrorReason();
  }
}

class PlanCollectionReplicationFactorTest
    : public ::testing::TestWithParam<std::tuple<uint32_t, uint32_t>> {
 protected:
  [[nodiscard]] uint32_t writeConcern() const {
    auto p = GetParam();
    return std::get<0>(p);
  };

  [[nodiscard]] uint32_t replicationFactor() const {
    auto p = GetParam();
    return std::get<1>(p);
  };

  [[nodiscard]] VPackBuilder testBody() {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add("writeConcern", VPackValue(writeConcern()));
      body.add("replicationFactor", VPackValue(replicationFactor()));
    }
    return body;
  }

  static DatabaseConfiguration defaultDBConfig() {
    return DatabaseConfiguration{
        []() { return DataSourceId(42); },
        [](std::string const&) { return Result{TRI_ERROR_INTERNAL}; }};
  }
};

INSTANTIATE_TEST_CASE_P(
    PlanCollectionReplicationFactorTest, PlanCollectionReplicationFactorTest,
    ::testing::Combine(::testing::Values(1ul, 2ul, 5ul, 8ul, 16ul),
                       ::testing::Values(1ul, 3ul, 5ul, 9ul, 15ul)));

TEST_P(PlanCollectionReplicationFactorTest, test_noMaxReplicationFactor) {
  auto body = testBody();
  auto config = defaultDBConfig();
  EXPECT_EQ(config.minReplicationFactor, 0ul);
  EXPECT_EQ(config.maxReplicationFactor, 0ul);
  EXPECT_EQ(config.enforceReplicationFactor, true);

  config.enforceReplicationFactor = true;

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();

  // We only check if writeConcern is okay there is no upper bound
  // on replicationFactor
  bool isAllowed = writeConcern() <= replicationFactor();
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->replicationFactor.value(), replicationFactor());

  } else {
    EXPECT_FALSE(result.ok()) << result.errorMessage();
  }
}

TEST_P(PlanCollectionReplicationFactorTest, test_maxReplicationFactor) {
  auto body = testBody();
  auto config = defaultDBConfig();
  EXPECT_EQ(config.minReplicationFactor, 0ul);
  EXPECT_EQ(config.maxReplicationFactor, 0ul);
  EXPECT_EQ(config.enforceReplicationFactor, true);

  config.enforceReplicationFactor = true;
  config.maxReplicationFactor = 5;

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();

  // We only check if writeConcern is okay there is no upper bound
  // on replicationFactor
  bool isAllowed = writeConcern() <= replicationFactor() &&
                   replicationFactor() <= config.maxReplicationFactor;
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->replicationFactor.value(), replicationFactor());
  } else {
    EXPECT_FALSE(result.ok()) << result.errorMessage();
  }
}

TEST_P(PlanCollectionReplicationFactorTest, test_minReplicationFactor) {
  auto body = testBody();
  auto config = defaultDBConfig();
  EXPECT_EQ(config.minReplicationFactor, 0ul);
  EXPECT_EQ(config.maxReplicationFactor, 0ul);
  EXPECT_EQ(config.enforceReplicationFactor, true);

  config.enforceReplicationFactor = true;
  config.minReplicationFactor = 5;

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();

  // We only check if writeConcern is okay there is no upper bound
  // on replicationFactor
  bool isAllowed = writeConcern() <= replicationFactor() &&
                   replicationFactor() >= config.minReplicationFactor;
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->replicationFactor.value(), replicationFactor());
  } else {
    EXPECT_FALSE(result.ok()) << "False positive on " << body.toJson();
  }
}

TEST_P(PlanCollectionReplicationFactorTest, test_nonoEnforce) {
  auto body = testBody();
  auto config = defaultDBConfig();
  EXPECT_EQ(config.minReplicationFactor, 0ull);
  EXPECT_EQ(config.maxReplicationFactor, 0ull);
  EXPECT_EQ(config.enforceReplicationFactor, true);

  config.enforceReplicationFactor = false;
  config.minReplicationFactor = 2;
  config.maxReplicationFactor = 5;

  auto testee = CreateCollectionBody::fromCreateAPIBody(body.slice(), config);
  auto result = testee.result();

  // Without enforcing you can do what you want, including illegal combinations
  bool isAllowed = true;
  // THis is stricter than 3.10
  isAllowed = writeConcern() <= replicationFactor();
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->replicationFactor.value(), replicationFactor());
  } else {
    EXPECT_FALSE(result.ok()) << "False positive on " << body.toJson();
  }
}

}  // namespace arangodb::tests
