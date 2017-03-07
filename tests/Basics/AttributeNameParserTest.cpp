////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for PathEnumerator class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::basics;


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("AttributeNameParserTest", "[attributenameparsertest]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test_simpleString
////////////////////////////////////////////////////////////////////////////////

SECTION("test_simpleString") {
  std::string input = "test";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, false);
  
  CHECK(result.size() == static_cast<size_t>(1));
  CHECK(result[0].name == input);
  CHECK(result[0].shouldExpand == false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_subAttribute
////////////////////////////////////////////////////////////////////////////////

SECTION("test_subAttribute") {
  std::string input = "foo.bar";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, false);
  
  CHECK(result.size() == static_cast<size_t>(2));
  CHECK(result[0].name == "foo");
  CHECK(result[0].shouldExpand == false);
  CHECK(result[1].name == "bar");
  CHECK(result[1].shouldExpand == false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_subsubAttribute
////////////////////////////////////////////////////////////////////////////////

SECTION("test_subsubAttribute") {
  std::string input = "foo.bar.baz";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, false);
  
  CHECK(result.size() == static_cast<size_t>(3));
  CHECK(result[0].name == "foo");
  CHECK(result[0].shouldExpand == false);
  CHECK(result[1].name == "bar");
  CHECK(result[1].shouldExpand == false);
  CHECK(result[2].name == "baz");
  CHECK(result[2].shouldExpand == false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandAttribute
////////////////////////////////////////////////////////////////////////////////

SECTION("test_expandAttribute") {
  std::string input = "foo[*]";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, true);
  
  CHECK(result.size() == static_cast<size_t>(1));
  CHECK(result[0].name == "foo");
  CHECK(result[0].shouldExpand == true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandSubAttribute
////////////////////////////////////////////////////////////////////////////////

SECTION("test_expandSubAttribute") {
  std::string input = "foo.bar[*]";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, true);
  
  CHECK(result.size() == static_cast<size_t>(2));
  CHECK(result[0].name == "foo");
  CHECK(result[0].shouldExpand == false);
  CHECK(result[1].name == "bar");
  CHECK(result[1].shouldExpand == true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandedSubAttribute
////////////////////////////////////////////////////////////////////////////////

SECTION("test_expandedSubAttribute") {
  std::string input = "foo[*].bar";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result, true);
  
  CHECK(result.size() == static_cast<size_t>(2));
  CHECK(result[0].name == "foo");
  CHECK(result[0].shouldExpand == true);
  CHECK(result[1].name == "bar");
  CHECK(result[1].shouldExpand == false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_invalidAttributeAfterExpand
////////////////////////////////////////////////////////////////////////////////

SECTION("test_invalidAttributeAfterExpand") {
  std::string input = "foo[*]bar";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_BAD_PARAMETER);
  }
  
  try {
    TRI_ParseAttributeString(input, result, true);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonClosing[
////////////////////////////////////////////////////////////////////////////////

SECTION("test_nonClosingBracket") {
  std::string input = "foo[*bar";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_BAD_PARAMETER);
  }
  
  try {
    TRI_ParseAttributeString(input, result, true);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonClosing[2
////////////////////////////////////////////////////////////////////////////////

SECTION("test_nonClosingBracket2") {
  std::string input = "foo[ * ].baz";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_BAD_PARAMETER);
  }
  
  try {
    TRI_ParseAttributeString(input, result, true);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonAsterisk
////////////////////////////////////////////////////////////////////////////////

SECTION("test_nonAsterisk") {
  std::string input = "foo[0]";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_BAD_PARAMETER);
  }
  
  try {
    TRI_ParseAttributeString(input, result, true);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonAsterisk
////////////////////////////////////////////////////////////////////////////////

SECTION("test_nonAsterisk2") {
  std::string input = "foo[0].value";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result, false);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_BAD_PARAMETER);
  }
  
  try {
    TRI_ParseAttributeString(input, result, true);
    CHECK(false);
  } catch (Exception& e) {
    CHECK(e.code() == TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransform
////////////////////////////////////////////////////////////////////////////////
/*
SECTION("test_reverseTransform") {
  std::string input = "foo[*].bar.baz[*]";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result, true);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  CHECK(output == input);
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformSimple
////////////////////////////////////////////////////////////////////////////////

SECTION("test_reverseTransformSimple") {
  std::string input = "i";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result, false);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  CHECK(output == input);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformSimpleMultiAttributes
////////////////////////////////////////////////////////////////////////////////

SECTION("test_reverseTransformSimpleMultiAttributes") {
  std::string input = "a.j";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result, false);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  CHECK(output == input);
}
////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformToPidPath
////////////////////////////////////////////////////////////////////////////////
/*
SECTION("test_reverseTransformToPidPath") {
  std::string input = "foo[*].bar.baz[*]";
  std::string expected = "foo.bar.baz";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result, true);

  std::string output = "";
  TRI_AttributeNamesToString(result, output, true);
  CHECK(output == expected);
}
*/




////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
