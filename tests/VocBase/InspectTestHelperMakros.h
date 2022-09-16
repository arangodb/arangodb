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

#pragma once

/**********************
 * MACRO SECTION
 *
 * Here are some helper Makros to fill some of the test code
 * for inspect APIS that tests general behaviour of attribute types
 *
 *
 * Tests using these Macros need to implement:
 *
 * // Returns minimal, valid JSON object for the struct to test.
 * // Only the given attributeName has the given value.
 * template<typename T>
 * VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
 *                                            T const& attributeValue);
 *
 *
 *  // Tries to parse the given body and returns a ResulT of your Type under
 *  //test.
 *   ResultT<YourStructToTest> parse(VPackSlice body);
 *********************/

#define __HELPER_assertParsingThrows(attributeName, value)            \
  {                                                                   \
    auto body = createMinimumBodyWithOneValue(#attributeName, value); \
    auto testee = parse(body.slice());                                \
    EXPECT_TRUE(testee.fail()) << " On body " << body.toJson();       \
  }

#define GenerateFailsOnBool(attributeName)           \
  __HELPER_assertParsingThrows(attributeName, true); \
  __HELPER_assertParsingThrows(attributeName, false);

#define GenerateFailsOnInteger(attributeName)      \
  __HELPER_assertParsingThrows(attributeName, 1);  \
  __HELPER_assertParsingThrows(attributeName, 0);  \
  __HELPER_assertParsingThrows(attributeName, 42); \
  __HELPER_assertParsingThrows(attributeName, -2);

#define GenerateFailsOnDouble(attributeName)        \
  __HELPER_assertParsingThrows(attributeName, 4.5); \
  __HELPER_assertParsingThrows(attributeName, 0.2); \
  __HELPER_assertParsingThrows(attributeName, -0.3);

#define GenerateFailsOnNonEmptyString(attributeName)   \
  __HELPER_assertParsingThrows(attributeName, "test"); \
  __HELPER_assertParsingThrows(attributeName, "dogfather");

#define GenerateFailsOnString(attributeName)       \
  __HELPER_assertParsingThrows(attributeName, ""); \
  GenerateFailsOnNonEmptyString(attribtueName)

#define GenerateFailsOnArray(attributeName) \
  __HELPER_assertParsingThrows(attributeName, VPackSlice::emptyArraySlice());

#define GenerateFailsOnObject(attributeName) \
  __HELPER_assertParsingThrows(attributeName, VPackSlice::emptyObjectSlice());

// This macro generates a basic bool value test, checking if we get true/false
// through and other basic types are rejected.

#define GenerateBoolAttributeTest(TestClass, attributeName)                   \
  TEST_F(TestClass, test_##attributeName) {                                   \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body, bool expected) { \
      auto testee = parse(body.slice());                                      \
      EXPECT_EQ(testee->attributeName, expected)                              \
          << "Parsing error in " << body.toJson();                            \
    };                                                                        \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, true),  \
                        true);                                                \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, false), \
                        false);                                               \
    GenerateFailsOnInteger(attributeName);                                    \
    GenerateFailsOnDouble(attributeName);                                     \
    GenerateFailsOnString(attributeName);                                     \
    GenerateFailsOnArray(attributeName);                                      \
    GenerateFailsOnObject(attributeName);                                     \
  }

#define GenerateStringAttributeTest(TestClass, attributeName)                  \
  TEST_F(TestClass, test_##attributeName) {                                    \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,                   \
                                   std::string const& expected) {              \
      auto testee = parse(body.slice());                                       \
      EXPECT_EQ(testee->attributeName, expected)                               \
          << "Parsing error in " << body.toJson();                             \
    };                                                                         \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, "test"), \
                        "test");                                               \
    shouldBeEvaluatedTo(                                                       \
        createMinimumBodyWithOneValue(#attributeName, "unknown"), "unknown");  \
    GenerateFailsOnBool(attributeName);                                        \
    GenerateFailsOnInteger(attributeName);                                     \
    GenerateFailsOnDouble(attributeName);                                      \
    GenerateFailsOnArray(attributeName);                                       \
    GenerateFailsOnObject(attributeName);                                      \
  }

#define GenerateOptionalStringAttributeTest(TestClass, attributeName)          \
  TEST_F(TestClass, test_##attributeName) {                                    \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,                   \
                                   std::string const& expected) {              \
      auto testee = parse(body.slice());                                       \
      ASSERT_TRUE(testee->attributeName.has_value())                           \
          << "Parsing error in " << body.toJson();                             \
      EXPECT_EQ(testee->attributeName.value(), expected)                       \
          << "Parsing error in " << body.toJson();                             \
    };                                                                         \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, "test"), \
                        "test");                                               \
    shouldBeEvaluatedTo(                                                       \
        createMinimumBodyWithOneValue(#attributeName, "unknown"), "unknown");  \
    GenerateFailsOnBool(attributeName);                                        \
    GenerateFailsOnInteger(attributeName);                                     \
    GenerateFailsOnDouble(attributeName);                                      \
    GenerateFailsOnArray(attributeName);                                       \
    GenerateFailsOnObject(attributeName);                                      \
  }
