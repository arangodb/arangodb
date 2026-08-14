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

#include "VocBase/Properties/ClusteringMutableProperties.h"
#include "Basics/ResultT.h"
#include "Inspection/VPack.h"

#include "InspectTestHelperMakros.h"
#include "velocypack/Parser.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {
class ClusteringMutablePropertiesTest : public ::testing::Test {
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
  static ResultT<ClusteringMutableProperties> parse(VPackSlice body) {
    ClusteringMutableProperties res;
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

  // The internal read path (persisted markers, agency plan entries) is
  // deliberately more permissive than user input.
  static ResultT<ClusteringMutableProperties> parseInternal(VPackSlice body) {
    ClusteringMutableProperties res;
    auto status = velocypack::deserializeWithStatus(body, res, {},
                                                    InspectInternalContext{});
    if (!status.ok()) {
      return Result{
          TRI_ERROR_BAD_PARAMETER,
          status.error() +
              (status.path().empty() ? "" : " on path " + status.path())};
    }
    return res;
  }

  static VPackBuilder serialize(ClusteringMutableProperties testee) {
    VPackBuilder result;
    velocypack::serialize(result, testee);
    return result;
  }
};

TEST_F(ClusteringMutablePropertiesTest, test_minimal_user_input) {
  VPackBuilder body;
  { VPackObjectBuilder bodyBuilder{&body}; }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  // Test Default values
  EXPECT_FALSE(testee->waitForSync);
  EXPECT_FALSE(testee->replicationFactor.has_value());
  EXPECT_FALSE(testee->writeConcern.has_value());
  __HELPER_equalsAfterSerializeParseCircle(testee.get());
}

GenerateBoolAttributeTest(ClusteringMutablePropertiesTest, waitForSync);

GeneratePositiveIntegerAttributeTest(ClusteringMutablePropertiesTest,
                                     replicationFactor);
GeneratePositiveIntegerAttributeTest(ClusteringMutablePropertiesTest,
                                     writeConcern);
GeneratePositiveIntegerAttributeTestInternal(ClusteringMutablePropertiesTest,
                                             minReplicationFactor, writeConcern,
                                             false);

// EE SmartGraph edge collections are persisted with writeConcern == 0 and a
// non-satellite replicationFactor, so "writeConcern > 0 unless satellite" does
// not hold for every valid instance. It constrains user input only.
TEST_F(ClusteringMutablePropertiesTest, test_writeConcernZeroIsInternalOnly) {
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("writeConcern", VPackValue(0));
    body.add("replicationFactor", VPackValue(2));
  }

  EXPECT_TRUE(parse(body.slice()).fail());

  auto testee = parseInternal(body.slice());
  ASSERT_TRUE(testee.ok()) << testee.errorMessage();
  ASSERT_TRUE(testee->writeConcern.has_value());
  EXPECT_EQ(testee->writeConcern.value(), 0u);
  EXPECT_FALSE(testee->isSatellite());
}

}  // namespace arangodb::tests
