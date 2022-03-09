////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief test_simpleString
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_simpleString) {
  std::string input = "test";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, false);

  EXPECT_EQ(result.size(), static_cast<size_t>(1));
  EXPECT_EQ(result[0].name, input);
  EXPECT_FALSE(result[0].shouldExpand);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_subAttribute
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_subAttribute) {
  std::string input = "foo.bar";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, false);

  EXPECT_EQ(result.size(), static_cast<size_t>(2));
  EXPECT_EQ(result[0].name, "foo");
  EXPECT_FALSE(result[0].shouldExpand);
  EXPECT_EQ(result[1].name, "bar");
  EXPECT_FALSE(result[1].shouldExpand);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_subsubAttribute
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_subsubAttribute) {
  std::string input = "foo.bar.baz";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, false);

  EXPECT_EQ(result.size(), static_cast<size_t>(3));
  EXPECT_EQ(result[0].name, "foo");
  EXPECT_FALSE(result[0].shouldExpand);
  EXPECT_EQ(result[1].name, "bar");
  EXPECT_FALSE(result[1].shouldExpand);
  EXPECT_EQ(result[2].name, "baz");
  EXPECT_FALSE(result[2].shouldExpand);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandAttribute
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_expandAttribute) {
  std::string input = "foo[*]";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, true);

  EXPECT_EQ(result.size(), static_cast<size_t>(1));
  EXPECT_EQ(result[0].name, "foo");
  EXPECT_TRUE(result[0].shouldExpand);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandSubAttribute
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_expandSubAttribute) {
  std::string input = "foo.bar[*]";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, true);

  EXPECT_EQ(result.size(), static_cast<size_t>(2));
  EXPECT_EQ(result[0].name, "foo");
  EXPECT_FALSE(result[0].shouldExpand);
  EXPECT_EQ(result[1].name, "bar");
  EXPECT_TRUE(result[1].shouldExpand);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandedSubAttribute
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_expandedSubAttribute) {
  std::string input = "foo[*].bar";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, true);

  EXPECT_EQ(result.size(), static_cast<size_t>(2));
  EXPECT_EQ(result[0].name, "foo");
  EXPECT_TRUE(result[0].shouldExpand);
  EXPECT_EQ(result[1].name, "bar");
  EXPECT_FALSE(result[1].shouldExpand);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_invalidAttributeAfterExpand
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_invalidAttributeAfterExpand) {
  std::string input = "foo[*]bar";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_BAD_PARAMETER);
  }

  try {
    TRI_ParseAttributeString(input, result, true);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonClosing[
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_nonClosingBracket) {
  std::string input = "foo[*bar";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_BAD_PARAMETER);
  }

  try {
    TRI_ParseAttributeString(input, result, true);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonClosing[2
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_nonClosingBracket2) {
  std::string input = "foo[ * ].baz";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_BAD_PARAMETER);
  }

  try {
    TRI_ParseAttributeString(input, result, true);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonAsterisk
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_nonAsterisk) {
  std::string input = "foo[0]";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_BAD_PARAMETER);
  }

  try {
    TRI_ParseAttributeString(input, result, true);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonAsterisk
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_nonAsterisk2) {
  std::string input = "foo[0].value";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_BAD_PARAMETER);
  }

  try {
    TRI_ParseAttributeString(input, result, true);
    EXPECT_TRUE(false);
  } catch (Exception& e) {
    EXPECT_EQ(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransform
////////////////////////////////////////////////////////////////////////////////
/*
TEST(AttributeNameParserTest, test_reverseTransform) {
  std::string input = "foo[*].bar.baz[*]";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result, true);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  EXPECT_EQ(output, input);
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformSimple
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_reverseTransformSimple) {
  std::string input = "i";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result, false);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  EXPECT_EQ(output, input);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformSimpleMultiAttributes
////////////////////////////////////////////////////////////////////////////////

TEST(AttributeNameParserTest, test_reverseTransformSimpleMultiAttributes) {
  std::string input = "a.j";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result, false);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  EXPECT_EQ(output, input);
}
