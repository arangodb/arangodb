////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "tests-common.h"

TEST(ExceptionTest, TestMessages) {
  ASSERT_STREQ("Internal error", Exception::message(Exception::InternalError));
  ASSERT_STREQ("Not implemented",
               Exception::message(Exception::NotImplemented));
  ASSERT_STREQ("Type has no equivalent in JSON",
               Exception::message(Exception::NoJsonEquivalent));
  ASSERT_STREQ("Parse error", Exception::message(Exception::ParseError));
  ASSERT_STREQ("Unexpected control character",
               Exception::message(Exception::UnexpectedControlCharacter));
  ASSERT_STREQ("Duplicate attribute name",
               Exception::message(Exception::DuplicateAttributeName));
  ASSERT_STREQ("Index out of bounds",
               Exception::message(Exception::IndexOutOfBounds));
  ASSERT_STREQ("Number out of range",
               Exception::message(Exception::NumberOutOfRange));
  ASSERT_STREQ("Invalid UTF-8 sequence",
               Exception::message(Exception::InvalidUtf8Sequence));
  ASSERT_STREQ("Invalid attribute path",
               Exception::message(Exception::InvalidAttributePath));
  ASSERT_STREQ("Invalid value type for operation",
               Exception::message(Exception::InvalidValueType));
  ASSERT_STREQ("Cannot execute operation without custom type handler",
               Exception::message(Exception::NeedCustomTypeHandler));
  ASSERT_STREQ("Cannot execute operation without attribute translator",
               Exception::message(Exception::NeedAttributeTranslator));
  ASSERT_STREQ("Cannot translate key",
               Exception::message(Exception::CannotTranslateKey));
  ASSERT_STREQ("Key not found", Exception::message(Exception::KeyNotFound));
  ASSERT_STREQ("Builder value not yet sealed",
               Exception::message(Exception::BuilderNotSealed));
  ASSERT_STREQ("Need open Object",
               Exception::message(Exception::BuilderNeedOpenObject));
  ASSERT_STREQ("Need open Array",
               Exception::message(Exception::BuilderNeedOpenArray));
  ASSERT_STREQ("Need subvalue in current Object or Array",
               Exception::message(Exception::BuilderNeedSubvalue));
  ASSERT_STREQ("Need open compound value (Array or Object)",
               Exception::message(Exception::BuilderNeedOpenCompound));
  ASSERT_STREQ("Unexpected type",
               Exception::message(Exception::BuilderUnexpectedType));
  ASSERT_STREQ("Unexpected value",
               Exception::message(Exception::BuilderUnexpectedValue));
  ASSERT_STREQ("Externals are not allowed in this configuration",
               Exception::message(Exception::BuilderExternalsDisallowed));
  ASSERT_STREQ("Custom types are not allowed in this configuration",
               Exception::message(Exception::BuilderCustomDisallowed));
  ASSERT_STREQ("Tagged types are not allowed in this configuration",
               Exception::message(Exception::BuilderTagsDisallowed));
  ASSERT_STREQ("BCD types are not allowed in this configuration",
               Exception::message(Exception::BuilderBCDDisallowed));
  ASSERT_STREQ("Invalid type found in binary data",
               Exception::message(Exception::ValidatorInvalidType));
  ASSERT_STREQ("Invalid length found in binary data",
               Exception::message(Exception::ValidatorInvalidLength));
  ASSERT_STREQ("Array size does not match tuple size",
               Exception::message(Exception::BadTupleSize));

  ASSERT_STREQ("Unknown error", Exception::message(Exception::UnknownError));
  ASSERT_STREQ("Unknown error",
               Exception::message(static_cast<Exception::ExceptionType>(0)));
  ASSERT_STREQ(
      "Unknown error",
      Exception::message(static_cast<Exception::ExceptionType>(99999)));
}

TEST(ExceptionTest, TestStringification) {
  std::string message;
  try {
    Builder b;
    b.close();
  } catch (Exception const& ex) {
    std::stringstream out;
    out << ex;
    message = out.str();
  }
  ASSERT_EQ("[Exception Need open compound value (Array or Object)]", message);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
