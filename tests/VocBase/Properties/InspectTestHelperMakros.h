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
 *
 *  // Tries to serialize the given object of your type and returns
 *   // a filled VPackBuilder
 *   VPackBuilder serialize(YourStructToTest testee);
 *********************/

#define __HELPER_equalsAfterSerializeParseCircle(testee)               \
  {                                                                    \
    auto body = serialize(testee);                                     \
    auto parsed = parse(body.slice());                                 \
    ASSERT_TRUE(parsed.ok())                                           \
        << "Failed to deserialize " << parsed.result().errorMessage(); \
    EXPECT_EQ(testee, parsed.get())                                    \
        << "SerializeCircle failed on " << body.toJson();              \
  }

#define __HELPER_assertParsingThrows(attributeName, value)            \
  {                                                                   \
    auto body = createMinimumBodyWithOneValue(#attributeName, value); \
    auto testee = parse(body.slice());                                \
    EXPECT_TRUE(testee.fail()) << " On body " << body.toJson();       \
  }

#define GenerateFailsOnNull(attributeName) \
  __HELPER_assertParsingThrows(attributeName, VPackSlice::nullSlice());

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
      __HELPER_equalsAfterSerializeParseCircle(testee.get())                  \
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
    GenerateFailsOnNull(attributeName)                                        \
  }

#define GenerateStringAttributeTest(TestClass, attributeName)                  \
  TEST_F(TestClass, test_##attributeName) {                                    \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,                   \
                                   std::string const& expected) {              \
      auto testee = parse(body.slice());                                       \
      EXPECT_EQ(testee->attributeName, expected)                               \
          << "Parsing error in " << body.toJson();                             \
      __HELPER_equalsAfterSerializeParseCircle(testee.get())                   \
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
    GenerateFailsOnNull(attributeName)                                         \
  }

#define GenerateOptionalStringAttributeTest(TestClass, attributeName)          \
  TEST_F(TestClass, test_##attributeName) {                                    \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,                   \
                                   std::string const& expected) {              \
      auto testee = parse(body.slice());                                       \
      ASSERT_TRUE(testee.ok())                                                 \
          << testee.errorMessage() << " on " << body.toJson();                 \
      ASSERT_TRUE(testee->attributeName.has_value())                           \
          << "Parsing error in " << body.toJson();                             \
      EXPECT_EQ(testee->attributeName.value(), expected)                       \
          << "Parsing error in " << body.toJson();                             \
      __HELPER_equalsAfterSerializeParseCircle(testee.get())                   \
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
    GenerateFailsOnNull(attributeName)                                         \
  }

// This macro generates a basic integer value test, checking if we get 2 and 42
// through and other basic types are rejected.
// NOTE: we also test 4.5 (double) right now this passes the validator, need to
// discuss if this is correct

#define GeneratePositiveIntegerAttributeTestInternal(TestClass, attributeName, \
                                                     valueName, allowZero)     \
  TEST_F(TestClass, test_##attributeName) {                                    \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,                   \
                                   uint64_t expected) {                        \
      auto testee = parse(body.slice());                                       \
      EXPECT_EQ(testee->valueName, expected)                                   \
          << "Parsing error in " << body.toJson();                             \
      __HELPER_equalsAfterSerializeParseCircle(testee.get())                   \
    };                                                                         \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, 2), 2);  \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, 42),     \
                        42);                                                   \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, 4.5),    \
                        4);                                                    \
    if (allowZero) {                                                           \
      shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, 0),    \
                          0);                                                  \
    } else {                                                                   \
      __HELPER_assertParsingThrows(attributeName, 0);                          \
    }                                                                          \
    __HELPER_assertParsingThrows(attributeName, -1);                           \
    __HELPER_assertParsingThrows(attributeName, -4.5);                         \
    GenerateFailsOnBool(attributeName);                                        \
    GenerateFailsOnString(attributeName);                                      \
    GenerateFailsOnArray(attributeName);                                       \
    GenerateFailsOnObject(attributeName);                                      \
    GenerateFailsOnNull(attributeName)                                         \
  }

#define GeneratePositiveIntegerAttributeTest(TestClass, attributeName)   \
  GeneratePositiveIntegerAttributeTestInternal(TestClass, attributeName, \
                                               attributeName, false)

#define GenerateIntegerAttributeTest(TestClass, attributeName)           \
  GeneratePositiveIntegerAttributeTestInternal(TestClass, attributeName, \
                                               attributeName, true)

#define GenerateIgnoredAttributeTest(TestClass, attributeName)                 \
  TEST_F(TestClass, test_##attributeName) {                                    \
    auto shouldPass = [&](VPackBuilder const& body) {                          \
      auto testee = parse(body.slice());                                       \
      EXPECT_TRUE(testee.ok())                                                 \
          << "Parsing error in " << body.toJson() << " attribute: '"           \
          << #attributeName << "' should be ignored";                          \
      __HELPER_equalsAfterSerializeParseCircle(testee.get())                   \
    };                                                                         \
    shouldPass(createMinimumBodyWithOneValue(#attributeName, 2));              \
    shouldPass(createMinimumBodyWithOneValue(#attributeName, -1));             \
    shouldPass(createMinimumBodyWithOneValue(#attributeName, "test"));         \
    shouldPass(createMinimumBodyWithOneValue(#attributeName, 3.5));            \
    shouldPass(createMinimumBodyWithOneValue(#attributeName,                   \
                                             VPackSlice::emptyObjectSlice())); \
    shouldPass(createMinimumBodyWithOneValue(#attributeName,                   \
                                             VPackSlice::emptyArraySlice()));  \
    shouldPass(createMinimumBodyWithOneValue(#attributeName,                   \
                                             VPackSlice::nullSlice()));        \
  }
