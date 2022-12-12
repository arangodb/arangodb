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

#include "VocBase/Properties/CollectionConstantProperties.h"
#include "Basics/ResultT.h"
#include "Basics/VelocyPackHelper.h"
#include "Inspection/VPack.h"

#include "InspectTestHelperMakros.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {
class CollectionConstantPropertiesTest : public ::testing::Test {
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
  static ResultT<CollectionConstantProperties> parse(VPackSlice body) {
    CollectionConstantProperties res;
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

  static VPackBuilder serialize(CollectionConstantProperties testee) {
    VPackBuilder result;
    velocypack::serialize(result, testee);
    return result;
  }
};

TEST_F(CollectionConstantPropertiesTest, test_minimal_user_input) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  auto testee = parse(body.slice());
  ASSERT_TRUE(testee.ok());
  EXPECT_EQ(testee->type, TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);
  EXPECT_FALSE(testee->isSystem);
  EXPECT_FALSE(testee->cacheEnabled);
  EXPECT_FALSE(testee->smartJoinAttribute.has_value());
  EXPECT_TRUE(std::holds_alternative<TraditionalKeyGeneratorProperties>(
      testee->keyOptions));
  EXPECT_FALSE(testee->isSmart);
  EXPECT_FALSE(testee->isDisjoint);
  EXPECT_FALSE(testee->smartGraphAttribute.has_value());
}

TEST_F(CollectionConstantPropertiesTest, test_collection_type) {
  auto shouldBeEvaluatedToType = [&](VPackBuilder const& body,
                                     TRI_col_type_e type) {
    auto testee = parse(body.slice());
    EXPECT_EQ(testee->type, type) << "Parsing error in " << body.toJson();
  };

  // Edge types, we only have two valid ways to get edges
  shouldBeEvaluatedToType(createMinimumBodyWithOneValue("type", 3),
                          TRI_col_type_e::TRI_COL_TYPE_EDGE);

  shouldBeEvaluatedToType(createMinimumBodyWithOneValue("type", 2),
                          TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);

  /// The following defaulted to edge before (not mentioned in doc since 3.3)
  __HELPER_assertParsingThrows(type, "edge");
  /// The following defaulted to document before (not mentioned in doc
  /// since 3.3):
  __HELPER_assertParsingThrows(type, 0);
  __HELPER_assertParsingThrows(type, 1);
  __HELPER_assertParsingThrows(type, 4);

  __HELPER_assertParsingThrows(type, "document");
  __HELPER_assertParsingThrows(type, "dogfather");

  GenerateFailsOnArray(type);
  GenerateFailsOnObject(type);
}

TEST_F(CollectionConstantPropertiesTest,
       test_smartGraphAttribtueRequiresIsSmart) {
  // Setting only SmartGraphAttribut is disallowed
  __HELPER_assertParsingThrows(smartGraphAttribute, "test");
}

GenerateBoolAttributeTest(CollectionConstantPropertiesTest, isSystem);
GenerateBoolAttributeTest(CollectionConstantPropertiesTest, isSmart);
GenerateBoolAttributeTest(CollectionConstantPropertiesTest, isDisjoint);
GenerateBoolAttributeTest(CollectionConstantPropertiesTest, cacheEnabled);

GenerateOptionalStringAttributeTest(CollectionConstantPropertiesTest,
                                    smartJoinAttribute);

// Ignored for backwards compatibility with MMFiles
GenerateIgnoredAttributeTest(CollectionConstantPropertiesTest, doCompact);
GenerateIgnoredAttributeTest(CollectionConstantPropertiesTest, isVolatile);

class CollectionConstantSmartPropertiesTest
    : public CollectionConstantPropertiesTest {
 protected:
  // Returns minimal, valid JSON object for the struct to test.
  // Only the given attributeName has the given value.
  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      if (attributeName != "isSmart") {
        body.add("isSmart", VPackValue(true));
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
};

GenerateBoolAttributeTest(CollectionConstantSmartPropertiesTest, isSystem);
GenerateBoolAttributeTest(CollectionConstantSmartPropertiesTest, isDisjoint);
GenerateBoolAttributeTest(CollectionConstantSmartPropertiesTest, cacheEnabled);

GenerateOptionalStringAttributeTest(CollectionConstantSmartPropertiesTest,
                                    smartGraphAttribute);

GenerateOptionalStringAttributeTest(CollectionConstantSmartPropertiesTest,
                                    smartJoinAttribute);

// Ignored for backwards compatibility with MMFiles
GenerateIgnoredAttributeTest(CollectionConstantSmartPropertiesTest, doCompact);
GenerateIgnoredAttributeTest(CollectionConstantSmartPropertiesTest, isVolatile);

}  // namespace arangodb::tests
