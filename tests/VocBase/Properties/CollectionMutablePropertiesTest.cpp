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

#include "VocBase/Properties/CollectionMutableProperties.h"
#include "Basics/ResultT.h"
#include "Inspection/VPack.h"

#include "InspectTestHelperMakros.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {
class CollectionMutablePropertiesTest : public ::testing::Test {
 protected:
  // Returns minimal, valid JSON object for the struct to test.
  // Only the given attributeName has the given value.
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

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  static ResultT<CollectionMutableProperties> parse(VPackSlice body) {
    CollectionMutableProperties res;
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

  static VPackBuilder serialize(CollectionMutableProperties testee) {
    VPackBuilder result;
    velocypack::serialize(result, testee);
    return result;
  }
};

TEST_F(CollectionMutablePropertiesTest, test_requires_some_input) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  auto testee = parse(body.slice());
  EXPECT_TRUE(testee.fail()) << " On body " << body.toJson();
}

TEST_F(CollectionMutablePropertiesTest, test_minimal_user_input) {
  std::string colName = "test";
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue(colName));
  }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  EXPECT_EQ(testee->name, colName);
  // Test Default values
  // TODO: this is just rudimentary
  // does not test internals yet
  EXPECT_TRUE(testee->computedValues.slice().isNull());
  EXPECT_FALSE(testee->schema.has_value());
}

TEST_F(CollectionMutablePropertiesTest, test_illegal_names) {
  // The empty string
  __HELPER_assertParsingThrows(name, "");

  // Non String types
  __HELPER_assertParsingThrows(name, 0);
  __HELPER_assertParsingThrows(name, VPackSlice::emptyObjectSlice());
  __HELPER_assertParsingThrows(name, VPackSlice::emptyArraySlice());

  GenerateFailsOnBool(name);
  GenerateFailsOnInteger(name);
  GenerateFailsOnDouble(name);
  GenerateFailsOnArray(name);
  GenerateFailsOnObject(name);
}

}  // namespace arangodb::tests
