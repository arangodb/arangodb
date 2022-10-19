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

#include "VocBase/Properties/ClusteringConstantProperties.h"
#include "Basics/ResultT.h"
#include "Inspection/VPack.h"

#include "InspectTestHelperMakros.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {
class ClusteringConstantPropertiesTest : public ::testing::Test {
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
  static ResultT<ClusteringConstantProperties> parse(VPackSlice body) {
    ClusteringConstantProperties res;
    try {
      auto status = velocypack::deserializeWithStatus(body, res);
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

  static VPackBuilder serialize(ClusteringConstantProperties testee) {
    VPackBuilder result;
    velocypack::serialize(result, testee);
    return result;
  }
};

TEST_F(ClusteringConstantPropertiesTest, test_minimal_user_input) {
  VPackBuilder body;
  { VPackObjectBuilder bodyBuilder{&body}; }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  // Test Default values
  EXPECT_FALSE(testee->numberOfShards.has_value());
  EXPECT_FALSE(testee->distributeShardsLike.has_value());
  // NOTE: We may want to add some context here
  EXPECT_FALSE(testee->shardingStrategy.has_value());
  ASSERT_EQ(testee->shardKeys.size(), 1);
  EXPECT_EQ(testee->shardKeys.at(0), StaticStrings::KeyString);

  __HELPER_equalsAfterSerializeParseCircle(testee.get());
}

TEST_F(ClusteringConstantPropertiesTest, test_shardingStrategy) {
  auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,
                                 std::string const& expected) {
    auto testee = parse(body.slice());
    EXPECT_EQ(testee->shardingStrategy, expected)
        << "Parsing error in " << body.toJson();
  };
  std::vector<std::string> allowedStrategies{"",
                                             "hash",
                                             "enterprise-hash-smart-edge",
                                             "community-compat",
                                             "enterprise-compat",
                                             "enterprise-smart-edge-compat"};

  for (auto const& strategy : allowedStrategies) {
    shouldBeEvaluatedTo(
        createMinimumBodyWithOneValue("shardingStrategy", strategy), strategy);
  }

  GenerateFailsOnBool(shardingStrategy);
  GenerateFailsOnNonEmptyString(shardingStrategy);
  GenerateFailsOnInteger(shardingStrategy);
  GenerateFailsOnDouble(shardingStrategy);
  GenerateFailsOnArray(shardingStrategy);
  GenerateFailsOnObject(shardingStrategy);
}

GeneratePositiveIntegerAttributeTest(ClusteringConstantPropertiesTest,
                                     numberOfShards);

GenerateOptionalStringAttributeTest(ClusteringConstantPropertiesTest,
                                    distributeShardsLike);
}  // namespace arangodb::tests
