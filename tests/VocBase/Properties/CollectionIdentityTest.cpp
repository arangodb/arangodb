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
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "VocBase/Properties/CollectionIdentity.h"
#include "Basics/ResultT.h"
#include "Inspection/VPack.h"

#include "InspectTestHelperMakros.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {
class CollectionIdentityTest : public ::testing::Test {
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
  static ResultT<CollectionIdentity> parse(VPackSlice body) {
    CollectionIdentity res;
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

  static VPackBuilder serialize(CollectionIdentity testee) {
    VPackBuilder result;
    velocypack::serialize(result, testee);
    return result;
  }
};

TEST_F(CollectionIdentityTest, test_minimal_user_input) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  EXPECT_EQ(testee->id.id(), 0);
  EXPECT_TRUE(testee->guid.empty());
  EXPECT_EQ(testee->planId, DataSourceId::none());
}

TEST_F(CollectionIdentityTest, test_id) {
  auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,
                                 DataSourceId const& expected) {
    auto testee = parse(body.slice());
    EXPECT_EQ(testee->id, expected) << "Parsing error in " << body.toJson();
    __HELPER_equalsAfterSerializeParseCircle(testee.get())
  };
  shouldBeEvaluatedTo(createMinimumBodyWithOneValue("id", "test"),
                      DataSourceId(0));
  shouldBeEvaluatedTo(createMinimumBodyWithOneValue("id", "unknown"),
                      DataSourceId(0));

  shouldBeEvaluatedTo(createMinimumBodyWithOneValue("id", "123"),
                      DataSourceId(123));
  shouldBeEvaluatedTo(createMinimumBodyWithOneValue("id", "42"),
                      DataSourceId(42));

  shouldBeEvaluatedTo(createMinimumBodyWithOneValue("id", "4.2"),
                      DataSourceId(0));

  GenerateFailsOnBool(id);
  GenerateFailsOnInteger(id);
  GenerateFailsOnDouble(id);
  GenerateFailsOnArray(id);
  GenerateFailsOnObject(id);
}

// Covers a non-documented API
GenerateIgnoredAttributeTest(CollectionIdentityTest, globallyUniqueId);

// planId is not declared outside the internal context, so the create API
// rejects it. guid is documented and therefore only ignored, see above.
TEST_F(CollectionIdentityTest, test_planId_is_rejected) {
  auto testee = parse(createMinimumBodyWithOneValue("planId", "123").slice());
  EXPECT_TRUE(testee.fail());
  EXPECT_EQ(testee.errorNumber(), TRI_ERROR_BAD_PARAMETER);
}

}  // namespace arangodb::tests
