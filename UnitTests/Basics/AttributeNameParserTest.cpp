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

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct AttributeNameParserSetup {
  AttributeNameParserSetup () {
    BOOST_TEST_MESSAGE("setup AttributeNameParser");
  }

  ~AttributeNameParserSetup () {
    BOOST_TEST_MESSAGE("tear-down AttributeNameParser");
  }
};


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE (AttributeNameParserTest, AttributeNameParserSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test_simpleString
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_simpleString) {
  std::string input = "test";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result);
  
  BOOST_CHECK_EQUAL(result.size(), static_cast<size_t>(1));
  BOOST_CHECK_EQUAL(result[0].name, input);
  BOOST_CHECK_EQUAL(result[0].shouldExpand, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_subAttribute
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_subAttribute) {
  std::string input = "foo.bar";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result);
  
  BOOST_CHECK_EQUAL(result.size(), static_cast<size_t>(2));
  BOOST_CHECK_EQUAL(result[0].name, "foo");
  BOOST_CHECK_EQUAL(result[0].shouldExpand, false);
  BOOST_CHECK_EQUAL(result[1].name, "bar");
  BOOST_CHECK_EQUAL(result[1].shouldExpand, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_subsubAttribute
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_subsubAttribute) {
  std::string input = "foo.bar.baz";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result);
  
  BOOST_CHECK_EQUAL(result.size(), static_cast<size_t>(3));
  BOOST_CHECK_EQUAL(result[0].name, "foo");
  BOOST_CHECK_EQUAL(result[0].shouldExpand, false);
  BOOST_CHECK_EQUAL(result[1].name, "bar");
  BOOST_CHECK_EQUAL(result[1].shouldExpand, false);
  BOOST_CHECK_EQUAL(result[2].name, "baz");
  BOOST_CHECK_EQUAL(result[2].shouldExpand, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandAttribute
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_expandAttribute) {
  std::string input = "foo[*]";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result);
  
  BOOST_CHECK_EQUAL(result.size(), static_cast<size_t>(1));
  BOOST_CHECK_EQUAL(result[0].name, "foo");
  BOOST_CHECK_EQUAL(result[0].shouldExpand, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandSubAttribute
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_expandSubAttribute) {
  std::string input = "foo.bar[*]";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result);
  
  BOOST_CHECK_EQUAL(result.size(), static_cast<size_t>(2));
  BOOST_CHECK_EQUAL(result[0].name, "foo");
  BOOST_CHECK_EQUAL(result[0].shouldExpand, false);
  BOOST_CHECK_EQUAL(result[1].name, "bar");
  BOOST_CHECK_EQUAL(result[1].shouldExpand, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_expandedSubAttribute
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_expandedSubAttribute) {
  std::string input = "foo[*].bar";
  std::vector<AttributeName> result;

  TRI_ParseAttributeString(input, result);
  
  BOOST_CHECK_EQUAL(result.size(), static_cast<size_t>(2));
  BOOST_CHECK_EQUAL(result[0].name, "foo");
  BOOST_CHECK_EQUAL(result[0].shouldExpand, true);
  BOOST_CHECK_EQUAL(result[1].name, "bar");
  BOOST_CHECK_EQUAL(result[1].shouldExpand, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_invalidAttributeAfterExpand
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_invalidAttributeAfterExpand) {
  std::string input = "foo[*]bar";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result);
    BOOST_FAIL("Expected the function to throw");
  } catch (Exception& e) {
    BOOST_CHECK_EQUAL(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonClosing[
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_nonClosingBracket) {
  std::string input = "foo[*bar";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result);
    BOOST_FAIL("Expected the function to throw");
  } catch (Exception& e) {
    BOOST_CHECK_EQUAL(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonClosing[2
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_nonClosingBracket2) {
  std::string input = "foo[ * ].baz";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result);
    BOOST_FAIL("Expected the function to throw");
  } catch (Exception& e) {
    BOOST_CHECK_EQUAL(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonAsterisk
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_nonAsterisk) {
  std::string input = "foo[0]";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result);
    BOOST_FAIL("Expected the function to throw");
  } catch (Exception& e) {
    BOOST_CHECK_EQUAL(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_nonAsterisk
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_nonAsterisk2) {
  std::string input = "foo[0].value";
  std::vector<AttributeName> result;

  try {
    TRI_ParseAttributeString(input, result);
    BOOST_FAIL("Expected the function to throw");
  } catch (Exception& e) {
    BOOST_CHECK_EQUAL(e.code(), TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransform
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_reverseTransform) {
  std::string input = "foo[*].bar.baz[*]";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  BOOST_CHECK_EQUAL(output, input);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformSimple
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_reverseTransformSimple) {
  std::string input = "i";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  BOOST_CHECK_EQUAL(output, input);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformSimpleMultiAttributes
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_reverseTransformSimpleMultiAttributes) {
  std::string input = "a.j";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result);

  std::string output = "";
  TRI_AttributeNamesToString(result, output);
  BOOST_CHECK_EQUAL(output, input);
}
////////////////////////////////////////////////////////////////////////////////
/// @brief test_reverseTransformToPidPath
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_reverseTransformToPidPath) {
  std::string input = "foo[*].bar.baz[*]";
  std::string expected = "foo.bar.baz";
  std::vector<AttributeName> result;
  TRI_ParseAttributeString(input, result);

  std::string output = "";
  TRI_AttributeNamesToString(result, output, true);
  BOOST_CHECK_EQUAL(output, expected);
}




////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
