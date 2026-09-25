////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
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
  // The backwards compatible retry asks for the server role, which has no
  // default. Tests that need a different one call setRole().
  void SetUp() override { setRole(ServerState::ROLE_COORDINATOR); }
  void TearDown() override { setRole(_previousRole); }

  static void setRole(ServerState::RoleEnum role) {
    ServerState::instance()->setRole(role);
  }

  ServerState::RoleEnum const _previousRole{ServerState::instance()->getRole()};

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
    return CreateCollectionBody::fromCreateAPIBody(body, config, false);
  }

  // Same as parse() except activateBackwardsCompatibility = true;
  // this is prod behavior
  static ResultT<CreateCollectionBody> parseCompatible(VPackSlice body) {
    return CreateCollectionBody::fromCreateAPIBody(body, defaultDBConfig());
  }

  // name and type are arguments of the V8 API, not part of the body
  static ResultT<CreateCollectionBody> parseV8(VPackSlice body) {
    return CreateCollectionBody::fromCreateAPIV8(
        body, "test", TRI_COL_TYPE_DOCUMENT, defaultDBConfig());
  }

  static ResultT<CreateCollectionBody> parseRestore(VPackSlice body) {
    return CreateCollectionBody::fromRestoreAPIBody(body, defaultDBConfig());
  }

  static void assertParsingThrows(VPackBuilder const& body) {
    auto p = CreateCollectionBody::fromCreateAPIBody(body.slice(),
                                                     defaultDBConfig(), false);
    EXPECT_TRUE(p.fail()) << " On body " << body.toJson();
  }

  /// @brief the sharding leader of a oneShard database, which is the only way
  /// to get a default distributeShardsLike. Hence it has to have one shard.
  static UserInputCollectionProperties defaultLeaderProps() {
    UserInputCollectionProperties res;
    res.numberOfShards = 1;
    res.replicationFactor = 3;
    res.writeConcern = 2;
    res.id = DataSourceId{42};
    res.shardingStrategy = "hash";
    res.shardKeys = std::vector<std::string>{StaticStrings::KeyString};
    return res;
  }
};

// The retry keeps only real bools for these, so a wrong type is dropped in
// compat and rejected without it.
#define GenerateBoolPropertyTest(attributeName)                              \
  TEST_F(CreateCollectionBodyTest, test_##attributeName) {                   \
    auto body = createMinimumBodyWithOneValue(#attributeName, true);         \
    for (auto const& valid :                                                 \
         {parseCompatible(body.slice()), parse(body.slice())}) {             \
      EXPECT_TRUE(valid.ok()) << " On body " << body.toJson();               \
    }                                                                        \
    body = createMinimumBodyWithOneValue(#attributeName, "yes");             \
    EXPECT_TRUE(parseCompatible(body.slice()).ok())                          \
        << " On body " << body.toJson();                                     \
    EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson(); \
  }

// The retry hands these back unchanged, so a wrong type is rejected on both
// paths.
#define GenerateKeptPropertyTest(testName, attributeName, validValue)        \
  TEST_F(CreateCollectionBodyTest, test_##testName) {                        \
    auto body = createMinimumBodyWithOneValue(attributeName, validValue);    \
    for (auto const& valid :                                                 \
         {parseCompatible(body.slice()), parse(body.slice())}) {             \
      EXPECT_TRUE(valid.ok()) << " On body " << body.toJson();               \
    }                                                                        \
    body = createMinimumBodyWithOneValue(attributeName,                      \
                                         VPackSlice::emptyArraySlice());     \
    EXPECT_TRUE(parseCompatible(body.slice()).fail())                        \
        << " On body " << body.toJson();                                     \
    EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson(); \
  }

// globallyUniqueId and deleted are ignored by the parser itself, so any value
// is accepted and dropped on both paths
#define GenerateIgnoredPropertyTest(testName, attributeName)              \
  TEST_F(CreateCollectionBodyTest, test_##testName) {                     \
    auto body = createMinimumBodyWithOneValue(attributeName, "anything"); \
    for (auto const& testee :                                             \
         {parseCompatible(body.slice()), parse(body.slice())}) {          \
      EXPECT_TRUE(testee.ok()) << " On body " << body.toJson();           \
    }                                                                     \
  }

// cid and planId are not part of the create API. compat drops them, without it
// they are rejected as unknown attributes.
#define GenerateUnknownPropertyTest(testName, attributeName)                 \
  TEST_F(CreateCollectionBodyTest, test_##testName) {                        \
    auto body = createMinimumBodyWithOneValue(attributeName, "123");         \
    EXPECT_TRUE(parseCompatible(body.slice()).ok())                          \
        << " On body " << body.toJson();                                     \
    EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson(); \
  }

/**********************
 * fromCreateAPIBody
 *********************/

// parseCompatible() is prod behavior: a value the parse rejects can still be
// dropped or corrected by retry; parse() is the same call without that retry

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
  auto testee = CreateCollectionBody::fromCreateAPIBody(
      body.slice(), defaultDBConfig(), false);

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

    auto testee = CreateCollectionBody::fromCreateAPIBody(
        body.slice(), defaultDBConfig(), false);
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

    auto testee = CreateCollectionBody::fromCreateAPIBody(
        body.slice(), defaultDBConfig(), false);
    ASSERT_TRUE(testee.ok()) << testee.result().errorNumber() << " -> "
                             << testee.result().errorMessage();
    ASSERT_TRUE(testee->writeConcern.has_value());
    EXPECT_EQ(testee->writeConcern.value(), 3ul);
  }
}

// writeConcern 0 needs a satellite. A single server drops the attribute, but
// only in compat.
TEST_F(CreateCollectionBodyTest, test_writeConcern_zeroNeedsSatellite) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue("test"));
    body.add(StaticStrings::WriteConcern, VPackValue(0));
    body.add(StaticStrings::ReplicationFactor, VPackValue(2));
  }
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();

  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();

  // a single server only drops the attribute in the retry
  setRole(ServerState::ROLE_SINGLE);
  EXPECT_TRUE(parseCompatible(body.slice()).ok())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

TEST_F(CreateCollectionBodyTest, test_satelliteReplicationFactor) {
  auto shouldBeEvaluatedTo = [&](VPackBuilder const& body, uint64_t number) {
    auto testee = CreateCollectionBody::fromCreateAPIBody(
        body.slice(), defaultDBConfig(), false);
#ifdef USE_ENTERPRISE
    ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
    ASSERT_TRUE(testee->replicationFactor.has_value());
    EXPECT_EQ(testee->replicationFactor.value(), number)
        << "Parsing error in " << body.toJson();
#else
    ASSERT_FALSE(testee.ok()) << "Created a satellite collection without "
                                 "enterprise features enabled.";
    EXPECT_EQ(testee.result().errorNumber(), TRI_ERROR_ONLY_ENTERPRISE)
        << " with message: " << testee.result().errorMessage();
#endif
  };

  // Special handling for "satellite" string
  shouldBeEvaluatedTo(
      createMinimumBodyWithOneValue("replicationFactor", "satellite"), 0);
}

// replicationFactor: compat rewrites 0 to satellite, non-compat rejects it.
// "satellite" is kept by both.
TEST_F(CreateCollectionBodyTest, test_replicationFactor_zeroBecomesSatellite) {
#ifdef USE_ENTERPRISE
  for (auto value : {VPackValue(0), VPackValue(StaticStrings::Satellite)}) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add(StaticStrings::ReplicationFactor, value);
    }
    auto testee = parseCompatible(body.slice());
    ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
    EXPECT_EQ(testee->replicationFactor, 0u) << " On body " << body.toJson();
  }

  // "satellite" needs no repair, so both paths take it
  auto satellite = createMinimumBodyWithOneValue(
      StaticStrings::ReplicationFactor, StaticStrings::Satellite);
  auto valid = parse(satellite.slice());
  ASSERT_TRUE(valid.ok()) << " On body " << satellite.toJson();
  EXPECT_EQ(valid->replicationFactor, 0u);

  // only the retry turns a numeric 0 into a satellite
  auto zero =
      createMinimumBodyWithOneValue(StaticStrings::ReplicationFactor, 0);
  EXPECT_TRUE(parse(zero.slice()).fail()) << " On body " << zero.toJson();
#endif
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
          CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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
          CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
      ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
      ASSERT_TRUE(testee->numberOfShards.has_value());
      EXPECT_EQ(testee->numberOfShards, 1024ul)
          << "Parsing error in " << body.toJson();
    }
    {
      // 16 < 1024 should fail
      config.maxNumberOfShards = 16;
      auto testee =
          CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
      EXPECT_FALSE(testee.ok())
          << "Configured " << config.maxNumberOfShards << " but "
          << testee->numberOfShards.value() << "passed.";
    }
  }
}

// numberOfShards: a positive number is kept. compat accepts null as absent,
// non-compat rejects it. 0 is rejected by both.
TEST_F(CreateCollectionBodyTest, test_numberOfShards) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 3);
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->numberOfShards, 3u);
  }

  body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards,
                                       VPackSlice::nullSlice());
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(
      testee->numberOfShards,
      parseCompatible(createMinimumBodyWithOneValue("name", "test").slice())
          ->numberOfShards);

  // without the retry the null is not dropped, so it stays rejected
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();

  body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 0);
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
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
  auto testee = CreateCollectionBody::fromCreateAPIBody(
      body.slice(), defaultDBConfig(), false);
  EXPECT_FALSE(testee.ok()) << "Configured smartCollection as 'satellite'.";
}

// isSmart is an enterprise only feature, and a smart document collection has
// to name its shardKeys
TEST_F(CreateCollectionBodyTest, test_isSmart) {
  auto onlyIsSmart =
      createMinimumBodyWithOneValue(StaticStrings::IsSmart, true);
  EXPECT_TRUE(parseCompatible(onlyIsSmart.slice()).fail())
      << " On body " << onlyIsSmart.toJson();
  EXPECT_TRUE(parse(onlyIsSmart.slice()).fail())
      << " On body " << onlyIsSmart.toJson();

  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue("test"));
    body.add(StaticStrings::IsSmart, VPackValue(true));
    VPackArrayBuilder shardKeys(&body, StaticStrings::ShardKeys);
    body.add(VPackValue(StaticStrings::PrefixOfKeyString));
  }
  for (auto const& testee :
       {parseCompatible(body.slice()), parse(body.slice())}) {
#ifdef USE_ENTERPRISE
    EXPECT_TRUE(testee.ok()) << " On body " << body.toJson();
#else
    ASSERT_TRUE(testee.fail());
    EXPECT_EQ(testee.errorNumber(), TRI_ERROR_ONLY_ENTERPRISE);
#endif
  }
}

TEST_F(CreateCollectionBodyTest, test_distributeShardsLike_default) {
  // We do not need any special configuration
  // default is good enough
  std::string defaultShardBy = "_graphs";
  auto leader = defaultLeaderProps();
  auto config = defaultDBConfig({{defaultShardBy, leader}});
  config.oneShardDBConfiguration =
      OneShardDatabaseConfiguration{defaultShardBy};

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
  config.oneShardDBConfiguration =
      OneShardDatabaseConfiguration{defaultShardBy};

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
  config.oneShardDBConfiguration =
      OneShardDatabaseConfiguration{defaultShardBy};

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
  config.oneShardDBConfiguration =
      OneShardDatabaseConfiguration{defaultShardBy};

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
  config.oneShardDBConfiguration =
      OneShardDatabaseConfiguration{defaultShardBy};

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
  config.oneShardDBConfiguration =
      OneShardDatabaseConfiguration{defaultShardBy};

  // Specific shardKey is disallowed
  auto body = createMinimumBodyWithOneValue("numberOfShards", 5);
  auto testee = parse(body.slice(), config);
  EXPECT_FALSE(testee.ok()) << "Number of Shards violates oneShard database";
}

// distributeShardsLike: compat accepts null as absent, non-compat rejects it
TEST_F(CreateCollectionBodyTest, test_distributeShardsLike_null) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DistributeShardsLike,
                                            VPackSlice::nullSlice());
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->distributeShardsLike.has_value());

  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
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
  auto testee = CreateCollectionBody::fromCreateAPIBody(
      body.slice(), defaultDBConfig(), false);
  EXPECT_FALSE(testee.ok())
      << "Configured smartChild collection as 'satellite'.";
}

// a wrong type is handed back to the parser and rejected on either path
GenerateKeptPropertyTest(isSmartChild, StaticStrings::IsSmartChild, true);

TEST_F(CreateCollectionBodyTest, test_smartJoinAttribute_cannot_be_empty) {
  auto config = defaultDBConfig();

  // Specific shardKey is disallowed
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::SmartJoinAttribute, "");
  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
  // This could already fail, as soon as we have a context
  EXPECT_FALSE(testee.ok()) << "Let an empty smartJoinAttribute through";
}

TEST_F(CreateCollectionBodyTest, test_smartGraphAttribtueRequiresIsSmart) {
  // Setting only SmartGraphAttribut is disallowed
  __HELPER_assertParsingThrows(smartGraphAttribute, "test");
}

#ifdef USE_ENTERPRISE
// smartGraphAttribute must not be empty on either path
TEST_F(CreateCollectionBodyTest, test_smartGraphAttribute_cannot_be_empty) {
  auto body = createMinimumBodyWithOneValue(
      StaticStrings::GraphSmartGraphAttribute, "");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}
#endif

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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

  auto testee =
      CreateCollectionBody::fromCreateAPIBody(body.slice(), config, false);
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

// name must not be empty on either path (compat and non-compat)
TEST_F(CreateCollectionBodyTest, test_name_cannot_be_empty) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::DataSourceName, "test");
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->name, "test");
  }

  body = createMinimumBodyWithOneValue(StaticStrings::DataSourceName, "");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// schema must be an object on either path; an empty one removes the schema
TEST_F(CreateCollectionBodyTest, test_schema_must_be_an_object) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::Schema,
                                            VPackSlice::emptyObjectSlice());
  EXPECT_TRUE(parseCompatible(body.slice()).ok())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).ok()) << " On body " << body.toJson();

  body = createMinimumBodyWithOneValue(StaticStrings::Schema, 42);
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// type: compat corrects an unknown value to document and "edge" to edge,
// non-compat rejects both
TEST_F(CreateCollectionBodyTest, test_type_unknownValueBecomesDocument) {
  // a valid type is accepted by both paths
  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType,
                                            TRI_COL_TYPE_EDGE);
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->getType(), TRI_COL_TYPE_EDGE);
  }

  body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, 4);
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->getType(), TRI_COL_TYPE_DOCUMENT);

  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();

  body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, "edge");
  testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->getType(), TRI_COL_TYPE_EDGE);

  // the retry is what rewrites the type, so without it both are rejected
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// keyOptions: "traditional" is user input, "upgrade" is rejected by both paths
TEST_F(CreateCollectionBodyTest, test_keyOptions_upgradeIsNotUserInput) {
  auto bodyWithGenerator = [](std::string_view type) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      VPackObjectBuilder keyOptions(&body, StaticStrings::KeyOptions);
      body.add("type", VPackValue(type));
    }
    return body;
  };

  auto body = bodyWithGenerator("traditional");
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_TRUE(std::holds_alternative<TraditionalKeyGeneratorProperties>(
        valid->keyOptions));
  }

  body = bodyWithGenerator("upgrade");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// shadowCollections is written by the server. The parser drops user input
// itself, so both paths accept it.
TEST_F(CreateCollectionBodyTest, test_shadowCollections_areDropped) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ShadowCollections,
                                            std::vector<std::string>{"1", "2"});
  // the parser ignores this attribute itself, so the retry changes nothing
  for (auto const& testee :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
    EXPECT_FALSE(testee->shadowCollections.has_value());
  }
}

// groupId and shardsR2 are written by the server. compat drops user input,
// non-compat rejects it.
TEST_F(CreateCollectionBodyTest, test_serverOwnedAttributes_areDropped) {
  {
    auto body = createMinimumBodyWithOneValue(StaticStrings::GroupId, 1234);
    auto testee = parseCompatible(body.slice());
    ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
    EXPECT_FALSE(testee->groupId.has_value());
    // the retry drops unknown attributes, the parser rejects them
    EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
  }
  {
    auto body = createMinimumBodyWithOneValue(
        "shardsR2", std::vector<std::string>{"s100001"});
    auto testee = parseCompatible(body.slice());
    ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
    EXPECT_FALSE(testee->shardsR2.has_value());
    EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
  }
}

// shardingStrategy must name a known strategy. A single server drops it, but
// only in compat.
TEST_F(CreateCollectionBodyTest, test_shardingStrategy_mustBeKnown) {
  auto valid =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "hash");
  EXPECT_TRUE(parseCompatible(valid.slice()).ok())
      << " On body " << valid.toJson();
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();

  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();

  // a single server only drops the attribute in the retry
  setRole(ServerState::ROLE_SINGLE);
  EXPECT_TRUE(parseCompatible(body.slice()).ok())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

GenerateBoolPropertyTest(isSystem);
GenerateBoolPropertyTest(isDisjoint);
GenerateBoolPropertyTest(cacheEnabled);
GenerateBoolPropertyTest(waitForSync);
GenerateBoolPropertyTest(syncByRevision);

GenerateKeptPropertyTest(usesRevisionsAsDocumentIds,
                         StaticStrings::UsesRevisionsAsDocumentIds, true);
GenerateKeptPropertyTest(internalValidatorType,
                         StaticStrings::InternalValidatorTypes, 1);

GenerateIgnoredPropertyTest(globallyUniqueId, StaticStrings::DataSourceGuid);
GenerateIgnoredPropertyTest(deleted, StaticStrings::DataSourceDeleted);

GenerateUnknownPropertyTest(cid, StaticStrings::DataSourceCid);
GenerateUnknownPropertyTest(planId, StaticStrings::DataSourcePlanId);

// id is user input on the create API, it travels as a string
TEST_F(CreateCollectionBodyTest, test_id) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::Id, "123");
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->id, DataSourceId{123});
  }
}

// shardKeys must be an array on either path
TEST_F(CreateCollectionBodyTest, test_shardKeys) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ShardKeys,
                                            std::vector<std::string>{"a"});
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->shardKeys.value(), (std::vector<std::string>{"a"}));
  }

  body = createMinimumBodyWithOneValue(StaticStrings::ShardKeys, "a");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

/**********************
 * fromCreateAPIV8
 *********************/

// The V8 API shares the allow list with fromCreateAPIBody, so the same values
// are accepted and corrected. name and type arrive as arguments.

// name and type are taken from the arguments when the body has neither
TEST_F(CreateCollectionBodyTest, test_v8_nameAndTypeComeFromArguments) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  auto testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << testee.errorMessage();
  EXPECT_EQ(testee->name, "test");
  EXPECT_EQ(testee->getType(), TRI_COL_TYPE_DOCUMENT);
}

// an empty name argument is rejected before the body is parsed
TEST_F(CreateCollectionBodyTest, test_v8_nameArgumentCannotBeEmpty) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  auto testee = CreateCollectionBody::fromCreateAPIV8(
      body.slice(), "", TRI_COL_TYPE_DOCUMENT, defaultDBConfig());
  ASSERT_TRUE(testee.fail());
  EXPECT_EQ(testee.errorNumber(), TRI_ERROR_ARANGO_ILLEGAL_NAME);
}

// numberOfShards: null is accepted as absent; 0 is rejected
TEST_F(CreateCollectionBodyTest, test_v8_numberOfShards) {
  VPackBuilder empty;
  { VPackObjectBuilder guard(&empty); }
  auto missing = parseV8(empty.slice());
  ASSERT_TRUE(missing.ok()) << missing.errorMessage();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards,
                                            VPackSlice::nullSlice());
  auto testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->numberOfShards, missing->numberOfShards);

  body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 0);
  EXPECT_TRUE(parseV8(body.slice()).fail()) << " On body " << body.toJson();
}

// an unknown type becomes document; "edge" becomes edge
TEST_F(CreateCollectionBodyTest, test_v8_type) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, 4);
  auto testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->getType(), TRI_COL_TYPE_DOCUMENT);

  body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, "edge");
  testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->getType(), TRI_COL_TYPE_EDGE);
}

// the "upgrade" key generator is for the load path only
TEST_F(CreateCollectionBodyTest, test_v8_keyOptions_upgradeIsNotUserInput) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    VPackObjectBuilder keyOptions(&body, StaticStrings::KeyOptions);
    body.add("type", VPackValue("upgrade"));
  }
  EXPECT_TRUE(parseV8(body.slice()).fail()) << " On body " << body.toJson();
}

/**********************
 * fromRestoreAPIBody
 *********************/

// Restore has its own allow list, which takes precedence over the one the
// other two APIs use. It is forever backwards compatible.

// the id in the body is never taken, a fresh one is generated
TEST_F(CreateCollectionBodyTest, test_restore_generatesFreshId) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::Id, "123");
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->id, DataSourceId{42}) << " the fixture id generator";
}

// numberOfShards: null is accepted as absent; 0 is rejected
TEST_F(CreateCollectionBodyTest, test_restore_numberOfShards) {
  auto missing =
      parseRestore(createMinimumBodyWithOneValue("name", "test").slice());
  ASSERT_TRUE(missing.ok()) << missing.errorMessage();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards,
                                            VPackSlice::nullSlice());
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->numberOfShards, missing->numberOfShards);

  body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 0);
  EXPECT_TRUE(parseRestore(body.slice()).fail())
      << " On body " << body.toJson();
}

// restore keeps numbers as they are, so an unknown type is rejected instead of
// being corrected; a string type is dropped and the default applies
TEST_F(CreateCollectionBodyTest, test_restore_type) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, 4);
  EXPECT_TRUE(parseRestore(body.slice()).fail())
      << " On body " << body.toJson();

  body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, "edge");
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->getType(), TRI_COL_TYPE_DOCUMENT);
}

// shardingStrategy must name a known strategy; a single server drops it
TEST_F(CreateCollectionBodyTest, test_restore_shardingStrategy_mustBeKnown) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parseRestore(body.slice()).fail())
      << " On body " << body.toJson();

  setRole(ServerState::ROLE_SINGLE);
  EXPECT_TRUE(parseRestore(body.slice()).ok()) << " On body " << body.toJson();
}

// replicationFactor: 0 is rewritten to satellite, "satellite" is kept
TEST_F(CreateCollectionBodyTest,
       test_restore_replicationFactor_zeroBecomesSatellite) {
#ifdef USE_ENTERPRISE
  for (auto value : {VPackValue(0), VPackValue(StaticStrings::Satellite)}) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add(StaticStrings::ReplicationFactor, value);
    }
    auto testee = parseRestore(body.slice());
    ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
    EXPECT_EQ(testee->replicationFactor, 0u) << " On body " << body.toJson();
  }
#endif
}

// the "upgrade" key generator is for the load path only
TEST_F(CreateCollectionBodyTest,
       test_restore_keyOptions_upgradeIsNotUserInput) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue("test"));
    VPackObjectBuilder keyOptions(&body, StaticStrings::KeyOptions);
    body.add("type", VPackValue("upgrade"));
  }
  EXPECT_TRUE(parseRestore(body.slice()).fail())
      << " On body " << body.toJson();
}

// groupId and shardsR2 are written by the server; user input is dropped
TEST_F(CreateCollectionBodyTest, test_restore_serverOwnedAttributes) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::GroupId, 1234);
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->groupId.has_value());
}

}  // namespace arangodb::tests
