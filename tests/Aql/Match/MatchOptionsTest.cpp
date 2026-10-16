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

#include "Aql/Match/MatchTestHelper.h"

#include "Aql/Match/MatchOptions.h"
#include "Aql/Match/PatternNormalizer.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests::aql::match;

namespace {

class MatchOptionsTest : public MatchTestFixture {};

TEST_F(MatchOptionsTest, matchWithoutOptionsStillWorks) {
  auto parsed = parseMatch("MATCH (v :vc) RETURN v");
  ast::MatchNode matchNode(parsed.matchNode);
  EXPECT_EQ(1u, matchNode.numPatterns());
  EXPECT_EQ(nullptr, matchNode.options());

  auto statement = normalize(parsed);
  EXPECT_EQ(1u, statement.patterns.size());
}

TEST_F(MatchOptionsTest, emptyOptionsAfterVertexPattern) {
  auto parsed = parseMatch("MATCH (v :vc) OPTIONS {} RETURN v");
  ast::MatchNode matchNode(parsed.matchNode);
  EXPECT_EQ(1u, matchNode.numPatterns());
  ASSERT_NE(nullptr, matchNode.options());
  EXPECT_EQ(NODE_TYPE_OBJECT, matchNode.options()->type);

  auto statement = normalize(parsed);
  EXPECT_EQ(1u, statement.patterns.size());
}

TEST_F(MatchOptionsTest, emptyOptionsAfterEdgePattern) {
  auto parsed =
      parseMatch("MATCH (v :vc) -[e :ec]-> (w :vc) OPTIONS {} RETURN w");
  ast::MatchNode matchNode(parsed.matchNode);
  EXPECT_EQ(1u, matchNode.numPatterns());
  ASSERT_NE(nullptr, matchNode.options());

  auto statement = normalize(parsed);
  EXPECT_EQ(1u, statement.patterns.size());
  EXPECT_EQ(1u, statement.patterns.front().segments.size());
}

TEST_F(MatchOptionsTest, rejectsUnknownOption) {
  try {
    parseMatch("MATCH (v :vc) OPTIONS {foo: 123} RETURN v");
    FAIL() << "expected unknown MATCH OPTIONS field to be rejected";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE, ex.code());
  }
}

TEST_F(MatchOptionsTest, rejectsNonObjectOptionsNumber) {
  try {
    parseMatch("MATCH (v :vc) OPTIONS 123 RETURN v");
    FAIL() << "expected non-object MATCH OPTIONS to be rejected";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_QUERY_PARSE, ex.code());
  }
}

TEST_F(MatchOptionsTest, rejectsNonObjectOptionsArray) {
  try {
    parseMatch("MATCH (v :vc) OPTIONS [] RETURN v");
    FAIL() << "expected non-object MATCH OPTIONS to be rejected";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_QUERY_PARSE, ex.code());
  }
}

TEST_F(MatchOptionsTest, rejectsDuplicateOptions) {
  try {
    parseMatch("MATCH (v :vc) OPTIONS {} OPTIONS {} RETURN v");
    FAIL() << "expected duplicate MATCH OPTIONS to be rejected";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_QUERY_PARSE, ex.code());
  }
}

TEST_F(MatchOptionsTest, fromAstNodeAcceptsEmptyObject) {
  auto parsed = parseMatch("MATCH (v :vc) OPTIONS {} RETURN v");
  auto options = match::MatchOptions::fromAstNode(
      ast::MatchNode(parsed.matchNode).options());
  // No fields yet; successful deserialization is the assertion.
  (void)options;
}

TEST_F(MatchOptionsTest, fromAstNodeNullptrIsDefault) {
  auto options = match::MatchOptions::fromAstNode(nullptr);
  (void)options;
}

}  // namespace
