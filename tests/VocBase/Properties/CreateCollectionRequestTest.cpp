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
#include "VocBase/Properties/CollectionDescriptor.h"
#include "VocBase/Properties/CreateCollectionRequest.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

#include "InspectTestHelperMakros.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {

namespace {
ResultT<CollectionDescriptor> parseBody(VPackSlice body,
                                        DatabaseConfiguration const& config) {
  auto request =
      CreateCollectionRequest::fromCreateAPIBody(body, config, false);
  if (request.fail()) {
    return request.result();
  }
  return std::move(request->descriptor);
}
}  // namespace

/**********************
 * TEST SECTION
 *********************/

class CreateCollectionRequestTest : public ::testing::Test {
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

  static VPackBuilder serialize(CollectionDescriptor const& testee) {
    VPackBuilder result;
    velocypack::serializeWithContext(result, testee, InspectUserContext{});
    return result;
  }

  static DatabaseConfiguration defaultDBConfig(
      std::unordered_map<std::string, CollectionDescriptor> lookupMap = {}) {
    // Leaders are looked up by name on a create and by id afterwards, because
    // validation rewrites distributeShardsLike to the leader's id. The
    // resolver behind this in production takes either.
    std::unordered_map<std::string, CollectionDescriptor> byId;
    for (auto const& [name, props] : lookupMap) {
      byId.emplace(std::to_string(props.internal.id.id()), props);
    }
    lookupMap.merge(byId);

    return DatabaseConfiguration{
        []() { return DataSourceId(42); },
        [lookupMap = std::move(lookupMap)](
            std::string const& nameOrId) -> ResultT<CollectionDescriptor> {
          // Set a lookup method
          if (!lookupMap.contains(nameOrId)) {
            return {TRI_ERROR_INTERNAL};
          }
          return lookupMap.at(nameOrId);
        }};
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  static ResultT<CollectionDescriptor> parse(
      VPackSlice body,
      DatabaseConfiguration const& config = defaultDBConfig()) {
    return parseBody(body, config);
  }

  // Same as parse() except activateBackwardsCompatibility = true;
  // this is prod behavior
  static ResultT<CollectionDescriptor> parseCompatible(VPackSlice body) {
    auto request =
        CreateCollectionRequest::fromCreateAPIBody(body, defaultDBConfig());
    if (request.fail()) {
      return request.result();
    }
    return std::move(request->descriptor);
  }

  // name and type are arguments of the V8 API, not part of the body
  static ResultT<CollectionDescriptor> parseV8(VPackSlice body) {
    auto request = CreateCollectionRequest::fromCreateAPIV8(
        body, "test", TRI_COL_TYPE_DOCUMENT, defaultDBConfig());
    if (request.fail()) {
      return request.result();
    }
    return std::move(request->descriptor);
  }

  static ResultT<CollectionDescriptor> parseRestore(VPackSlice body) {
    auto request =
        CreateCollectionRequest::fromRestoreAPIBody(body, defaultDBConfig());
    if (request.fail()) {
      return request.result();
    }
    return std::move(request->descriptor);
  }

  static VPackBuilder keyOptionsBody(std::string_view generatorType) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      VPackObjectBuilder keyOptions(&body, StaticStrings::KeyOptions);
      body.add("type", VPackValue(generatorType));
    }
    return body;
  }

  static VPackBuilder writeConcernZeroBody() {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add(StaticStrings::WriteConcern, VPackValue(0));
      body.add(StaticStrings::ReplicationFactor, VPackValue(2));
    }
    return body;
  }

  static VPackBuilder smartCollectionBody() {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add(StaticStrings::IsSmart, VPackValue(true));
      VPackArrayBuilder shardKeys(&body, StaticStrings::ShardKeys);
      body.add(VPackValue(StaticStrings::PrefixOfKeyString));
    }
    return body;
  }

  static void expectRejected(ResultT<CollectionDescriptor> const& testee,
                             std::string_view attributeName,
                             VPackBuilder const& body) {
    EXPECT_TRUE(testee.fail()) << " On body " << body.toJson();
    if (testee.fail()) {
      EXPECT_NE(testee.errorMessage().find(attributeName),
                std::string_view::npos)
          << "'" << attributeName
          << "' is not named in: " << testee.errorMessage();
    }
  }

  static void assertParsingThrows(VPackBuilder const& body) {
    auto p = parse(body.slice());
    EXPECT_TRUE(p.fail()) << " On body " << body.toJson();
  }

  /// @brief the sharding leader of a oneShard database, which is the only way
  /// to get a default distributeShardsLike. Hence it has to have one shard.
  static CollectionDescriptor defaultLeaderProps() {
    CollectionDescriptor res;
    res.clusteringConstant.numberOfShards = 1;
    res.clusteringMutable.replicationFactor = 3;
    res.clusteringMutable.writeConcern = 2;
    res.internal.id = DataSourceId{42};
    res.clusteringConstant.shardingStrategy = "hash";
    res.clusteringConstant.shardKeys =
        std::vector<std::string>{StaticStrings::KeyString};
    return res;
  }
};

// A wrong type is dropped by the retry, so compat accepts it while the strict
// parse rejects it. The valid value is there to show the case under test is
// the only thing failing.
#define GenerateBoolPropertyTest(group, attributeName)                       \
  TEST_F(CreateCollectionRequestTest,                                        \
         test_##attributeName##WrongTypeIsAcceptedWithCompatibility) {       \
    auto missing = createMinimumBodyWithOneValue("name", "test");            \
    auto valid = parseCompatible(missing.slice());                           \
    ASSERT_TRUE(valid.ok()) << " On body " << missing.toJson();              \
    auto body = createMinimumBodyWithOneValue(#attributeName, "yes");        \
    auto testee = parseCompatible(body.slice());                             \
    ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();                \
    EXPECT_EQ(testee->group.attributeName, valid->group.attributeName)       \
        << " On body " << body.toJson();                                     \
  }                                                                          \
  TEST_F(CreateCollectionRequestTest,                                        \
         test_##attributeName##WrongTypeIsRejectedWithoutCompatibility) {    \
    auto valid = createMinimumBodyWithOneValue(#attributeName, true);        \
    EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson(); \
    auto body = createMinimumBodyWithOneValue(#attributeName, "yes");        \
    expectRejected(parse(body.slice()), #attributeName, body);               \
  }

// The retry hands these back unchanged, so a wrong type is rejected either way.
#define GenerateKeptPropertyTest(testName, attributeName, validValue)         \
  TEST_F(CreateCollectionRequestTest,                                         \
         test_##testName##WrongTypeIsRejectedWithCompatibility) {             \
    auto valid = createMinimumBodyWithOneValue(attributeName, validValue);    \
    EXPECT_TRUE(parseCompatible(valid.slice()).ok())                          \
        << " On body " << valid.toJson();                                     \
    auto body = createMinimumBodyWithOneValue(attributeName,                  \
                                              VPackSlice::emptyArraySlice()); \
    expectRejected(parseCompatible(body.slice()), attributeName, body);       \
  }                                                                           \
  TEST_F(CreateCollectionRequestTest,                                         \
         test_##testName##WrongTypeIsRejectedWithoutCompatibility) {          \
    auto valid = createMinimumBodyWithOneValue(attributeName, validValue);    \
    EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();  \
    auto body = createMinimumBodyWithOneValue(attributeName,                  \
                                              VPackSlice::emptyArraySlice()); \
    expectRejected(parse(body.slice()), attributeName, body);                 \
  }

// globallyUniqueId and deleted are ignored by the parser itself, so any value
// is accepted and dropped either way.
#define GenerateIgnoredPropertyTest(testName, attributeName)               \
  TEST_F(CreateCollectionRequestTest,                                      \
         test_##testName##IsDroppedWithCompatibility) {                    \
    auto body = createMinimumBodyWithOneValue(attributeName, "anything");  \
    EXPECT_TRUE(parseCompatible(body.slice()).ok())                        \
        << " On body " << body.toJson();                                   \
  }                                                                        \
  TEST_F(CreateCollectionRequestTest,                                      \
         test_##testName##IsDroppedWithoutCompatibility) {                 \
    auto body = createMinimumBodyWithOneValue(attributeName, "anything");  \
    EXPECT_TRUE(parse(body.slice()).ok()) << " On body " << body.toJson(); \
  }

// cid and planId are not part of the create API. The retry drops them, the
// strict parse rejects them as unknown attributes.
#define GenerateUnknownPropertyTest(testName, attributeName)         \
  TEST_F(CreateCollectionRequestTest,                                \
         test_##testName##IsDroppedWithCompatibility) {              \
    auto body = createMinimumBodyWithOneValue(attributeName, "123"); \
    EXPECT_TRUE(parseCompatible(body.slice()).ok())                  \
        << " On body " << body.toJson();                             \
  }                                                                  \
  TEST_F(CreateCollectionRequestTest,                                \
         test_##testName##IsRejectedWithoutCompatibility) {          \
    auto body = createMinimumBodyWithOneValue(attributeName, "123"); \
    expectRejected(parse(body.slice()), attributeName, body);        \
  }

/**********************
 * fromCreateAPIBody
 *********************/

// parseCompatible() is prod behavior: a value the parse rejects can still be
// dropped or corrected by retry; parse() is the same call without that retry

TEST_F(CreateCollectionRequestTest, test_requires_some_input) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  assertParsingThrows(body);
}

TEST_F(CreateCollectionRequestTest,
       test_rejectedAttributesKeepTheirErrorCodes) {
  auto fails = [&](std::string const& attribute, auto value, ErrorCode code) {
    auto body = createMinimumBodyWithOneValue(attribute, value);
    auto testee = parse(body.slice());
    ASSERT_TRUE(testee.fail()) << " On body " << body.toJson();
    EXPECT_EQ(testee.errorNumber(), code) << " On body " << body.toJson();
  };

  fails("name", "", TRI_ERROR_ARANGO_ILLEGAL_NAME);
  fails("type", 0, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  fails("type", 1, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  fails("type", 4, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  fails("smartJoinAttribute", "", TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE);
  fails("smartGraphAttribute", "", TRI_ERROR_BAD_PARAMETER);
  fails("schema", 5, TRI_ERROR_VALIDATION_BAD_PARAMETER);
  fails("numberOfShards", 0, TRI_ERROR_BAD_PARAMETER);
  fails("shardingStrategy", "dogfather", TRI_ERROR_BAD_PARAMETER);
  fails("distributeShardsLike", "", TRI_ERROR_BAD_PARAMETER);
  fails("writeConcern", 0, TRI_ERROR_BAD_PARAMETER);
}

TEST_F(CreateCollectionRequestTest, test_minimal_user_input) {
  std::string colName = "test";
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue(colName));
  }
  auto testee = CreateCollectionRequest::fromCreateAPIBody(
      body.slice(), defaultDBConfig(), false);

  ASSERT_TRUE(testee.ok()) << testee.errorMessage();
  // Test Default values

  // This covers only non-documented APIS
  EXPECT_TRUE(testee->options.avoidServers.empty());

  __HELPER_equalsAfterSerializeParseCircle(testee->descriptor);
}

TEST_F(CreateCollectionRequestTest,
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

    auto testee = parse(body.slice(), defaultDBConfig());
    ASSERT_TRUE(testee.ok()) << testee.result().errorNumber() << " -> "
                             << testee.result().errorMessage();
    ASSERT_TRUE(testee->clusteringMutable.writeConcern.has_value());
    EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), 3ul);
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

    auto testee = parse(body.slice(), defaultDBConfig());
    ASSERT_TRUE(testee.ok()) << testee.result().errorNumber() << " -> "
                             << testee.result().errorMessage();
    ASSERT_TRUE(testee->clusteringMutable.writeConcern.has_value());
    EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), 3ul);
  }
}

// writeConcern 0 needs a satellite, so a cluster rejects it either way
TEST_F(CreateCollectionRequestTest,
       test_writeConcernZeroIsRejectedWithCompatibility) {
  EXPECT_TRUE(parseCompatible(writeConcernZeroBody().slice()).fail());
}

TEST_F(CreateCollectionRequestTest,
       test_writeConcernZeroIsRejectedWithoutCompatibility) {
  EXPECT_TRUE(parse(writeConcernZeroBody().slice()).fail());
}

// a single server has no write concern, so the retry drops the attribute
TEST_F(CreateCollectionRequestTest,
       test_writeConcernZeroIsDroppedOnSingleServerWithCompatibility) {
  setRole(ServerState::ROLE_SINGLE);
  EXPECT_TRUE(parseCompatible(writeConcernZeroBody().slice()).ok());
}

TEST_F(CreateCollectionRequestTest,
       test_writeConcernZeroIsRejectedOnSingleServerWithoutCompatibility) {
  setRole(ServerState::ROLE_SINGLE);
  EXPECT_TRUE(parse(writeConcernZeroBody().slice()).fail());
}

TEST_F(CreateCollectionRequestTest, test_satelliteReplicationFactor) {
  auto shouldBeEvaluatedTo = [&](VPackBuilder const& body, uint64_t number) {
    auto testee = parse(body.slice(), defaultDBConfig());
#ifdef USE_ENTERPRISE
    ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
    ASSERT_TRUE(testee->clusteringMutable.replicationFactor.has_value());
    EXPECT_EQ(testee->clusteringMutable.replicationFactor.value(), number)
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

#ifdef USE_ENTERPRISE
// "satellite" needs no repair, so it is taken either way
TEST_F(CreateCollectionRequestTest, test_replicationFactorSatelliteIsKept) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ReplicationFactor,
                                            StaticStrings::Satellite);
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->clusteringMutable.replicationFactor, 0u);
  }
}

// only the retry turns a numeric 0 into a satellite
TEST_F(CreateCollectionRequestTest,
       test_replicationFactorZeroBecomesSatelliteWithCompatibility) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ReplicationFactor, 0);
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->clusteringMutable.replicationFactor, 0u);
}

TEST_F(CreateCollectionRequestTest,
       test_replicationFactorZeroIsRejectedWithoutCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::ReplicationFactor,
                                             StaticStrings::Satellite);
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ReplicationFactor, 0);
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}
#endif

TEST_F(CreateCollectionRequestTest, test_configureMaxNumberOfShards) {
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
      auto testee = parse(body.slice(), config);
      ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
      ASSERT_TRUE(testee->clusteringConstant.numberOfShards.has_value());
      EXPECT_EQ(testee->clusteringConstant.numberOfShards, 1024ul)
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
      auto testee = parse(body.slice(), config);
      ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
      ASSERT_TRUE(testee->clusteringConstant.numberOfShards.has_value());
      EXPECT_EQ(testee->clusteringConstant.numberOfShards, 1024ul)
          << "Parsing error in " << body.toJson();
    }
    {
      // 16 < 1024 should fail
      config.maxNumberOfShards = 16;
      auto testee = parse(body.slice(), config);
      EXPECT_FALSE(testee.ok())
          << "Configured " << config.maxNumberOfShards << " but "
          << testee->clusteringConstant.numberOfShards.value() << "passed.";
    }
  }
}

// numberOfShards: a positive number is kept
TEST_F(CreateCollectionRequestTest, test_numberOfShardsIsKept) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 3);
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->clusteringConstant.numberOfShards, 3u);
  }
}

// the retry drops the null, so the default applies
TEST_F(CreateCollectionRequestTest,
       test_numberOfShardsNullIsAcceptedWithCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 3);
  EXPECT_TRUE(parseCompatible(valid.slice()).ok())
      << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards,
                                            VPackSlice::nullSlice());
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(
      testee->clusteringConstant.numberOfShards,
      parseCompatible(createMinimumBodyWithOneValue("name", "test").slice())
          ->clusteringConstant.numberOfShards);
}

TEST_F(CreateCollectionRequestTest,
       test_numberOfShardsNullIsRejectedWithoutCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 3);
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards,
                                            VPackSlice::nullSlice());
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// the retry keeps the zero, so it is rejected on the second parse
TEST_F(CreateCollectionRequestTest,
       test_numberOfShardsZeroIsRejectedWithCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 3);
  EXPECT_TRUE(parseCompatible(valid.slice()).ok())
      << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 0);
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_numberOfShardsZeroIsRejectedWithoutCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 3);
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 0);
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest, test_isSmartCannotBeSatellite) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    // has to be greater or equal to used writeConcern
    body.add("name", VPackValue("test"));
    body.add("isSmart", VPackValue(true));
    body.add("replicationFactor", VPackValue("satellite"));
  }

  // Note: We can also make this parsing fail in the first place.
  auto testee = parse(body.slice(), defaultDBConfig());
  EXPECT_FALSE(testee.ok()) << "Configured smartCollection as 'satellite'.";
}

// a smart document collection has to name its shardKeys
TEST_F(CreateCollectionRequestTest,
       test_isSmartWithoutShardKeysIsRejectedWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::IsSmart, true);
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_isSmartWithoutShardKeysIsRejectedWithoutCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::IsSmart, true);
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest, test_isSmartWithShardKeys) {
  auto body = smartCollectionBody();
  for (auto const& testee :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    EXPECT_TRUE(testee.ok()) << " On body " << body.toJson();
  }
}

TEST_F(CreateCollectionRequestTest, test_distributeShardsLike_default) {
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
  // the name the caller gave is replaced by the leader's id
  EXPECT_EQ(testee->clusteringConstant.distributeShardsLike.value(),
            std::to_string(leader.internal.id.id()));
  EXPECT_EQ(testee->clusteringConstant.numberOfShards.value(),
            leader.clusteringConstant.numberOfShards.value());
  EXPECT_EQ(testee->clusteringMutable.replicationFactor.value(),
            leader.clusteringMutable.replicationFactor.value());
  EXPECT_EQ(testee->clusteringMutable.writeConcern.value(),
            leader.clusteringMutable.writeConcern.value());
}

TEST_F(CreateCollectionRequestTest,
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

TEST_F(CreateCollectionRequestTest,
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
    body.add("numberOfShards",
             VPackValue(leader.clusteringConstant.numberOfShards.value()));
    body.add("replicationFactor",
             VPackValue(leader.clusteringMutable.replicationFactor.value()));
    body.add("writeConcern",
             VPackValue(leader.clusteringMutable.writeConcern.value()));
  }

  auto testee = parse(body.slice(), config);
  // Default value should be taken if none is set
  ASSERT_TRUE(testee.ok()) << "Failed on " << testee.errorMessage();
  // the name the caller gave is replaced by the leader's id
  EXPECT_EQ(testee->clusteringConstant.distributeShardsLike.value(),
            std::to_string(leader.internal.id.id()));
  EXPECT_EQ(testee->clusteringConstant.numberOfShards.value(),
            leader.clusteringConstant.numberOfShards.value());
  EXPECT_EQ(testee->clusteringMutable.replicationFactor.value(),
            leader.clusteringMutable.replicationFactor.value());
  EXPECT_EQ(testee->clusteringMutable.writeConcern.value(),
            leader.clusteringMutable.writeConcern.value());
}

TEST_F(CreateCollectionRequestTest,
       test_distributeShardsLike_default_ownValue) {
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

TEST_F(CreateCollectionRequestTest, test_oneShard_forcesDistributeShardsLike) {
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

TEST_F(CreateCollectionRequestTest, test_oneShard_moreShards) {
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

// the retry drops the null, so the attribute stays unset
TEST_F(CreateCollectionRequestTest,
       test_distributeShardsLikeNullIsAcceptedWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DistributeShardsLike,
                                            VPackSlice::nullSlice());
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->clusteringConstant.distributeShardsLike.has_value());
}

TEST_F(CreateCollectionRequestTest,
       test_distributeShardsLikeNullIsRejectedWithoutCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DistributeShardsLike,
                                            VPackSlice::nullSlice());
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest, test_isSmartChildCannotBeSatellite) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    // has to be greater or equal to used writeConcern
    body.add("name", VPackValue("test"));
    body.add("isSmartChild", VPackValue(true));
    body.add("replicationFactor", VPackValue("satellite"));
  }

  // Note: We can also make this parsing fail in the first place.
  auto testee = parse(body.slice(), defaultDBConfig());
  EXPECT_FALSE(testee.ok())
      << "Configured smartChild collection as 'satellite'.";
}

// a wrong type is handed back to the parser and rejected on either path
GenerateKeptPropertyTest(isSmartChild, StaticStrings::IsSmartChild, true);

TEST_F(CreateCollectionRequestTest, test_smartJoinAttribute_cannot_be_empty) {
  auto config = defaultDBConfig();

  // Specific shardKey is disallowed
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::SmartJoinAttribute, "");
  auto testee = parse(body.slice(), config);
  // This could already fail, as soon as we have a context
  EXPECT_FALSE(testee.ok()) << "Let an empty smartJoinAttribute through";
}

TEST_F(CreateCollectionRequestTest, test_smartGraphAttribtueRequiresIsSmart) {
  // Setting only SmartGraphAttribut is disallowed
  __HELPER_assertParsingThrows(smartGraphAttribute, "test");
}

#ifdef USE_ENTERPRISE
// smartGraphAttribute must not be empty
TEST_F(CreateCollectionRequestTest,
       test_smartGraphAttributeEmptyIsRejectedWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(
      StaticStrings::GraphSmartGraphAttribute, "");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_smartGraphAttributeEmptyIsRejectedWithoutCompatibility) {
  auto body = createMinimumBodyWithOneValue(
      StaticStrings::GraphSmartGraphAttribute, "");
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}
#endif

TEST_F(CreateCollectionRequestTest, test_oneShardDBCannotBeSatellite) {
  auto body = createMinimumBodyWithOneValue("replicationFactor", "satellite");

  auto config = defaultDBConfig();
  config.oneShardDBConfiguration = OneShardDatabaseConfiguration{};

  auto testee = parse(body.slice(), config);
  EXPECT_FALSE(testee.ok())
      << "Configured a oneShardDB collection as 'satellite'.";
}

TEST_F(CreateCollectionRequestTest, test_satelliteAcceptsOnlyKeyAsShardKey) {
  auto satelliteWithShardKeys = [&](std::vector<std::string> const& keys) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add("replicationFactor", VPackValue("satellite"));
      body.add(VPackValue("shardKeys"));
      {
        VPackArrayBuilder arrayGuard{&body};
        for (auto const& key : keys) {
          body.add(VPackValue(key));
        }
      }
    }
    return body;
  };

  // Sharding by a specific shardKey, or by a prefix/postfix of _key, is not
  // allowed for satellites
  for (auto const& key : {"testKey", "a", ":_key", "_key:"}) {
    auto body = satelliteWithShardKeys({key});
    EXPECT_FALSE(parse(body.slice()).ok())
        << "Created a satellite collection with a shardkey: " << key;
  }

  {
    // Sharding by _key is the only allowed value
    auto body = satelliteWithShardKeys({StaticStrings::KeyString});
    auto result = parse(body.slice()).result();
#ifdef USE_ENTERPRISE
    EXPECT_TRUE(result.ok())
        << "Failed to create a satellite collection with default sharding "
        << result.errorMessage();
#else
    EXPECT_FALSE(result.ok())
        << "Created a 'satellite' collection in community edition. "
        << result.errorMessage();
#endif
  }

  {
    // _key plus something else is not allowed either
    auto body = satelliteWithShardKeys({StaticStrings::KeyString, "testKey"});
    EXPECT_FALSE(parse(body.slice()).ok())
        << "Created a satellite collection with shardKeys [_key, testKey]";
  }
}

TEST_F(CreateCollectionRequestTest,
       test_satelliteDefaultsToOneShardAndWriteConcernOne) {
  auto body = createMinimumBodyWithOneValue("replicationFactor", "satellite");
  auto testee = parse(body.slice());
#ifdef USE_ENTERPRISE
  ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
  EXPECT_TRUE(testee->clusteringMutable.isSatellite());
  ASSERT_TRUE(testee->clusteringMutable.writeConcern.has_value());
  EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), 1ull);
  ASSERT_TRUE(testee->clusteringConstant.numberOfShards.has_value());
  EXPECT_EQ(testee->clusteringConstant.numberOfShards.value(), 1ull);
  __HELPER_equalsAfterSerializeParseCircle(testee.get());
#else
  EXPECT_FALSE(testee.ok())
      << "Created a 'satellite' collection in community edition.";
#endif
}

TEST_F(CreateCollectionRequestTest, test_satelliteRejectsMoreThanOneShard) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue("test"));
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("numberOfShards", VPackValue(3));
  }
  EXPECT_FALSE(parse(body.slice()).ok())
      << "Allowed illegal: " << body.toJson();
}

TEST_F(CreateCollectionRequestTest, test_satelliteAcceptsNumberOfShardsOne) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue("test"));
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("numberOfShards", VPackValue(1));
  }
  auto testee = parse(body.slice());
#ifdef USE_ENTERPRISE
  ASSERT_TRUE(testee.ok()) << testee.result().errorMessage();
  EXPECT_TRUE(testee->clusteringMutable.isSatellite());
  ASSERT_TRUE(testee->clusteringMutable.writeConcern.has_value());
  EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), 1ull);
  ASSERT_TRUE(testee->clusteringConstant.numberOfShards.has_value());
  EXPECT_EQ(testee->clusteringConstant.numberOfShards.value(), 1ull);
  __HELPER_equalsAfterSerializeParseCircle(testee.get());
#else
  EXPECT_FALSE(testee.ok())
      << "Created a 'satellite' collection in community edition.";
#endif
}

TEST_F(CreateCollectionRequestTest, test_satelliteRejectsWriteConcernAboveOne) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue("test"));
    body.add("replicationFactor", VPackValue("satellite"));
    body.add("writeConcern", VPackValue(3));
  }
  EXPECT_FALSE(parse(body.slice()).ok())
      << "Allowed illegal: " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_satelliteAcceptsWriteConcernZeroAndOne) {
  // As satellite is replicationFactor 0, writeConcern 0 and 1 are both
  // accepted: writeConcern is defined to be at most replicationFactor, and
  // some APIs pass 1.
  for (auto writeConcern : {0, 1}) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue("test"));
      body.add("replicationFactor", VPackValue("satellite"));
      body.add("writeConcern", VPackValue(writeConcern));
    }
    auto testee = parse(body.slice());
#ifdef USE_ENTERPRISE
    ASSERT_TRUE(testee.ok()) << "Did not allow legal body: " << body.toJson()
                             << " -- " << testee.result().errorMessage();
    EXPECT_TRUE(testee->clusteringMutable.isSatellite());
    ASSERT_TRUE(testee->clusteringConstant.numberOfShards.has_value());
    EXPECT_EQ(testee->clusteringConstant.numberOfShards.value(), 1ull);
#else
    EXPECT_FALSE(testee.ok())
        << "Created a 'satellite' collection in community edition.";
#endif
  }
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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed =
      !isDisAllowedInGeneral() && !requiresSystem() && !requiresExtendedNames();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->mutableProps.name, getName())
        << "Parsing error in " << body.toJson();
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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed = !isDisAllowedInGeneral() && !requiresExtendedNames();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->mutableProps.name, getName())
        << "Parsing error in " << body.toJson();
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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed = !isDisAllowedInGeneral() && !requiresSystem();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->mutableProps.name, getName())
        << "Parsing error in " << body.toJson();
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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();
  bool isAllowed = !isDisAllowedInGeneral();

  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->mutableProps.name, getName())
        << "Parsing error in " << body.toJson();
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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();

  // We only check if writeConcern is okay there is no upper bound
  // on replicationFactor
  bool isAllowed = writeConcern() <= replicationFactor();
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->clusteringMutable.replicationFactor.value(),
              replicationFactor());

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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();

  // We only check if writeConcern is okay there is no upper bound
  // on replicationFactor
  bool isAllowed = writeConcern() <= replicationFactor() &&
                   replicationFactor() <= config.maxReplicationFactor;
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->clusteringMutable.replicationFactor.value(),
              replicationFactor());
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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();

  // We only check if writeConcern is okay there is no upper bound
  // on replicationFactor
  bool isAllowed = writeConcern() <= replicationFactor() &&
                   replicationFactor() >= config.minReplicationFactor;
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->clusteringMutable.replicationFactor.value(),
              replicationFactor());
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

  auto testee = parseBody(body.slice(), config);
  auto result = testee.result();

  // Without enforcing you can do what you want, including illegal combinations
  bool isAllowed = true;
  // THis is stricter than 3.10
  isAllowed = writeConcern() <= replicationFactor();
  if (isAllowed) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    EXPECT_EQ(testee->clusteringMutable.writeConcern.value(), writeConcern());
    EXPECT_EQ(testee->clusteringMutable.replicationFactor.value(),
              replicationFactor());
  } else {
    EXPECT_FALSE(result.ok()) << "False positive on " << body.toJson();
  }
}

// name must not be empty
TEST_F(CreateCollectionRequestTest, test_nameEmptyIsRejectedWithCompatibility) {
  auto valid =
      createMinimumBodyWithOneValue(StaticStrings::DataSourceName, "test");
  EXPECT_TRUE(parseCompatible(valid.slice()).ok())
      << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceName, "");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_nameEmptyIsRejectedWithoutCompatibility) {
  auto valid =
      createMinimumBodyWithOneValue(StaticStrings::DataSourceName, "test");
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceName, "");
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// schema must be an object; an empty one removes the schema
TEST_F(CreateCollectionRequestTest,
       test_schemaNotAnObjectIsRejectedWithCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::Schema,
                                             VPackSlice::emptyObjectSlice());
  EXPECT_TRUE(parseCompatible(valid.slice()).ok())
      << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::Schema, 42);
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_schemaNotAnObjectIsRejectedWithoutCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::Schema,
                                             VPackSlice::emptyObjectSlice());
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::Schema, 42);
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// a valid type is taken as it is
TEST_F(CreateCollectionRequestTest, test_typeIsKept) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType,
                                            TRI_COL_TYPE_EDGE);
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->constant.getType(), TRI_COL_TYPE_EDGE);
  }
}

// the retry rewrites anything that is not an edge type to document
TEST_F(CreateCollectionRequestTest,
       test_typeUnknownNumberBecomesDocumentWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, 4);
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->constant.getType(), TRI_COL_TYPE_DOCUMENT);
}

TEST_F(CreateCollectionRequestTest,
       test_typeUnknownNumberIsRejectedWithoutCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::DataSourceType,
                                             TRI_COL_TYPE_EDGE);
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, 4);
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_typeStringBecomesEdgeWithCompatibility) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::DataSourceType, "edge");
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->constant.getType(), TRI_COL_TYPE_EDGE);
}

TEST_F(CreateCollectionRequestTest,
       test_typeStringIsRejectedWithoutCompatibility) {
  auto valid = createMinimumBodyWithOneValue(StaticStrings::DataSourceType,
                                             TRI_COL_TYPE_EDGE);
  EXPECT_TRUE(parse(valid.slice()).ok()) << " On body " << valid.toJson();

  auto body =
      createMinimumBodyWithOneValue(StaticStrings::DataSourceType, "edge");
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// the "upgrade" key generator is for the load path only
TEST_F(CreateCollectionRequestTest,
       test_keyOptionsUpgradeIsRejectedWithCompatibility) {
  auto valid = keyOptionsBody("traditional");
  EXPECT_TRUE(parseCompatible(valid.slice()).ok())
      << " On body " << valid.toJson();

  auto body = keyOptionsBody("upgrade");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_keyOptionsUpgradeIsRejectedWithoutCompatibility) {
  auto valid = keyOptionsBody("traditional");
  auto testee = parse(valid.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << valid.toJson();
  EXPECT_TRUE(std::holds_alternative<TraditionalKeyGeneratorProperties>(
      testee->constant.keyOptions));

  auto body = keyOptionsBody("upgrade");
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// shadowCollections is written by the server; the parser drops user input
TEST_F(CreateCollectionRequestTest,
       test_shadowCollectionsIsDroppedWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ShadowCollections,
                                            std::vector<std::string>{"1", "2"});
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->constant.shadowCollections.has_value());
}

TEST_F(CreateCollectionRequestTest,
       test_shadowCollectionsIsDroppedWithoutCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ShadowCollections,
                                            std::vector<std::string>{"1", "2"});
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->constant.shadowCollections.has_value());
}

// groupId and shardsR2 are written by the server; the retry drops user input,
// the strict parse rejects it as an unknown attribute
TEST_F(CreateCollectionRequestTest, test_groupIdIsDroppedWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::GroupId, 1234);
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->clusteringConstant.groupId.has_value());
}

TEST_F(CreateCollectionRequestTest,
       test_groupIdIsRejectedWithoutCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::GroupId, 1234);
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest, test_shardsR2IsDroppedWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(
      "shardsR2", std::vector<std::string>{"s100001"});
  auto testee = parseCompatible(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->clusteringConstant.shardsR2.has_value());
}

TEST_F(CreateCollectionRequestTest,
       test_shardsR2IsRejectedWithoutCompatibility) {
  auto body = createMinimumBodyWithOneValue(
      "shardsR2", std::vector<std::string>{"s100001"});
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// a known strategy is taken either way
TEST_F(CreateCollectionRequestTest, test_shardingStrategyIsKept) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "hash");
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    EXPECT_TRUE(valid.ok()) << " On body " << body.toJson();
  }
}

// a cluster keeps an unknown strategy, so it is rejected either way
TEST_F(CreateCollectionRequestTest,
       test_shardingStrategyUnknownIsRejectedWithCompatibility) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_shardingStrategyUnknownIsRejectedWithoutCompatibility) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

// a single server has no sharding, so the retry drops the attribute
TEST_F(CreateCollectionRequestTest,
       test_shardingStrategyUnknownIsDroppedOnSingleServerWithCompatibility) {
  setRole(ServerState::ROLE_SINGLE);
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parseCompatible(body.slice()).ok())
      << " On body " << body.toJson();
}

TEST_F(
    CreateCollectionRequestTest,
    test_shardingStrategyUnknownIsRejectedOnSingleServerWithoutCompatibility) {
  setRole(ServerState::ROLE_SINGLE);
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

GenerateBoolPropertyTest(constant, isSystem);
GenerateBoolPropertyTest(constant, isDisjoint);
GenerateBoolPropertyTest(mutableProps, cacheEnabled);
GenerateBoolPropertyTest(clusteringMutable, waitForSync);
GenerateBoolPropertyTest(internal, syncByRevision);

GenerateKeptPropertyTest(usesRevisionsAsDocumentIds,
                         StaticStrings::UsesRevisionsAsDocumentIds, true);
GenerateKeptPropertyTest(internalValidatorType,
                         StaticStrings::InternalValidatorTypes, 1);

GenerateIgnoredPropertyTest(globallyUniqueId, StaticStrings::DataSourceGuid);
GenerateIgnoredPropertyTest(deleted, StaticStrings::DataSourceDeleted);

GenerateUnknownPropertyTest(cid, StaticStrings::DataSourceCid);
GenerateUnknownPropertyTest(planId, StaticStrings::DataSourcePlanId);

// id is user input on the create API, it travels as a string
TEST_F(CreateCollectionRequestTest, test_id) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::Id, "123");
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->internal.id, DataSourceId{123});
  }
}

// shardKeys must be an array
TEST_F(CreateCollectionRequestTest, test_shardKeysIsKept) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ShardKeys,
                                            std::vector<std::string>{"a"});
  for (auto const& valid :
       {parseCompatible(body.slice()), parse(body.slice())}) {
    ASSERT_TRUE(valid.ok()) << " On body " << body.toJson();
    EXPECT_EQ(valid->clusteringConstant.shardKeys.value(),
              (std::vector<std::string>{"a"}));
  }
}

TEST_F(CreateCollectionRequestTest,
       test_shardKeysNotAnArrayIsRejectedWithCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ShardKeys, "a");
  EXPECT_TRUE(parseCompatible(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_shardKeysNotAnArrayIsRejectedWithoutCompatibility) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::ShardKeys, "a");
  EXPECT_TRUE(parse(body.slice()).fail()) << " On body " << body.toJson();
}

/**********************
 * fromCreateAPIV8
 *********************/

// The V8 API shares the allow list with fromCreateAPIBody, so the same values
// are accepted and corrected. name and type arrive as arguments.

// name and type are taken from the arguments when the body has neither
TEST_F(CreateCollectionRequestTest, test_v8_nameAndTypeComeFromArguments) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  auto testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << testee.errorMessage();
  EXPECT_EQ(testee->mutableProps.name, "test");
  EXPECT_EQ(testee->constant.getType(), TRI_COL_TYPE_DOCUMENT);
}

// an empty name argument is rejected before the body is parsed
TEST_F(CreateCollectionRequestTest, test_v8_nameArgumentCannotBeEmpty) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  auto testee = CreateCollectionRequest::fromCreateAPIV8(
      body.slice(), "", TRI_COL_TYPE_DOCUMENT, defaultDBConfig());
  ASSERT_TRUE(testee.fail());
  EXPECT_EQ(testee.errorNumber(), TRI_ERROR_ARANGO_ILLEGAL_NAME);
}

// numberOfShards: null is accepted as absent, 0 is rejected
TEST_F(CreateCollectionRequestTest, test_v8NumberOfShardsNullIsAccepted) {
  VPackBuilder empty;
  { VPackObjectBuilder guard(&empty); }
  auto missing = parseV8(empty.slice());
  ASSERT_TRUE(missing.ok()) << missing.errorMessage();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards,
                                            VPackSlice::nullSlice());
  auto testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->clusteringConstant.numberOfShards,
            missing->clusteringConstant.numberOfShards);
}

TEST_F(CreateCollectionRequestTest, test_v8NumberOfShardsZeroIsRejected) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 0);
  EXPECT_TRUE(parseV8(body.slice()).fail()) << " On body " << body.toJson();
}

// an unknown type becomes document, "edge" becomes edge
TEST_F(CreateCollectionRequestTest, test_v8TypeUnknownNumberBecomesDocument) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, 4);
  auto testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->constant.getType(), TRI_COL_TYPE_DOCUMENT);
}

TEST_F(CreateCollectionRequestTest, test_v8TypeStringBecomesEdge) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::DataSourceType, "edge");
  auto testee = parseV8(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->constant.getType(), TRI_COL_TYPE_EDGE);
}

// the "upgrade" key generator is for the load path only
TEST_F(CreateCollectionRequestTest, test_v8_keyOptions_upgradeIsNotUserInput) {
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
TEST_F(CreateCollectionRequestTest, test_restore_generatesFreshId) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::Id, "123");
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->internal.id, DataSourceId{42})
      << " the fixture id generator";
}

// numberOfShards: null is accepted as absent, 0 is rejected
TEST_F(CreateCollectionRequestTest, test_restoreNumberOfShardsNullIsAccepted) {
  auto missing =
      parseRestore(createMinimumBodyWithOneValue("name", "test").slice());
  ASSERT_TRUE(missing.ok()) << missing.errorMessage();

  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards,
                                            VPackSlice::nullSlice());
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->clusteringConstant.numberOfShards,
            missing->clusteringConstant.numberOfShards);
}

TEST_F(CreateCollectionRequestTest, test_restoreNumberOfShardsZeroIsRejected) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::NumberOfShards, 0);
  EXPECT_TRUE(parseRestore(body.slice()).fail())
      << " On body " << body.toJson();
}

// restore keeps numbers as they are, so an unknown type is rejected instead
// of being corrected
TEST_F(CreateCollectionRequestTest, test_restoreTypeUnknownNumberIsRejected) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::DataSourceType, 4);
  EXPECT_TRUE(parseRestore(body.slice()).fail())
      << " On body " << body.toJson();
}

// a string type is dropped, so the default applies
TEST_F(CreateCollectionRequestTest, test_restoreTypeStringBecomesDocument) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::DataSourceType, "edge");
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_EQ(testee->constant.getType(), TRI_COL_TYPE_DOCUMENT);
}

// a cluster keeps an unknown strategy, a single server drops it
TEST_F(CreateCollectionRequestTest,
       test_restoreShardingStrategyUnknownIsRejected) {
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parseRestore(body.slice()).fail())
      << " On body " << body.toJson();
}

TEST_F(CreateCollectionRequestTest,
       test_restoreShardingStrategyUnknownIsDroppedOnSingleServer) {
  setRole(ServerState::ROLE_SINGLE);
  auto body =
      createMinimumBodyWithOneValue(StaticStrings::ShardingStrategy, "bogus");
  EXPECT_TRUE(parseRestore(body.slice()).ok()) << " On body " << body.toJson();
}

// replicationFactor: 0 is rewritten to satellite, "satellite" is kept
TEST_F(CreateCollectionRequestTest,
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
    EXPECT_EQ(testee->clusteringMutable.replicationFactor, 0u)
        << " On body " << body.toJson();
  }
#endif
}

// the "upgrade" key generator is for the load path only
TEST_F(CreateCollectionRequestTest,
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
TEST_F(CreateCollectionRequestTest, test_restoreGroupIdIsDropped) {
  auto body = createMinimumBodyWithOneValue(StaticStrings::GroupId, 1234);
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->clusteringConstant.groupId.has_value());
}

TEST_F(CreateCollectionRequestTest, test_restoreShardsR2IsDropped) {
  auto body = createMinimumBodyWithOneValue(
      "shardsR2", std::vector<std::string>{"s100001"});
  auto testee = parseRestore(body.slice());
  ASSERT_TRUE(testee.ok()) << " On body " << body.toJson();
  EXPECT_FALSE(testee->clusteringConstant.shardsR2.has_value());
}

}  // namespace arangodb::tests
