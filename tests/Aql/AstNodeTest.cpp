////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Parser/Parser.h"
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Aql/StandaloneCalculation.h"
#include "Logger/LogMacros.h"
#include "Mocks/Servers.h"
#include "Transaction/OperationOrigin.h"

#include "gtest/gtest.h"

#include <functional>
#include <string_view>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

class AstNodeTest : public ::testing::Test {
 public:
  AstNodeTest()
      : _server{}, _query{_server.createFakeQuery()}, _ast{_query->ast()} {}

  void toVelocyPackHelper(
      std::function<void(AstNode const*)> const& validateAst, bool verbose) {
    AstNode* root = _ast->nodeFromVPack(_builder.slice(), true);
    EXPECT_NE(root, nullptr);

    validateAst(root);

    _builder.clear();
    root->toVelocyPack(_builder, verbose);
  }

 protected:
  tests::mocks::MockAqlServer _server;
  std::shared_ptr<Query> _query;
  Ast* _ast;

  VPackBuilder _builder;
};

AstNode const* getObjectElement(AstNode const* object, std::string_view name) {
  EXPECT_EQ(NODE_TYPE_OBJECT, object->type);
  for (size_t i = 0; i < object->numMembers(); ++i) {
    AstNode const* member = object->getMember(i);
    if (member->type != NODE_TYPE_OBJECT_ELEMENT) {
      continue;
    }
    if (member->getStringView() == name) {
      return member;
    }
  }
  return nullptr;
}

AstNode const* getObjectElementValue(AstNode const* object,
                                     std::string_view name) {
  AstNode const* member = getObjectElement(object, name);
  EXPECT_NE(nullptr, member);
  if (member == nullptr) {
    return nullptr;
  }
  EXPECT_EQ(1, member->numMembers());
  return member->getMember(0);
}

void expectObjectInt(AstNode const* object, std::string_view name,
                     int64_t value) {
  AstNode const* member = getObjectElementValue(object, name);
  ASSERT_NE(nullptr, member);
  ASSERT_EQ(NODE_TYPE_VALUE, member->type);
  EXPECT_EQ(value, member->getIntValue());
}

void expectObjectString(AstNode const* object, std::string_view name,
                        std::string_view value) {
  AstNode const* member = getObjectElementValue(object, name);
  ASSERT_NE(nullptr, member);
  ASSERT_EQ(NODE_TYPE_VALUE, member->type);
  EXPECT_EQ(value, member->getStringView());
}

AstNode const* expectObjectObject(AstNode const* object,
                                  std::string_view name) {
  AstNode const* member = getObjectElementValue(object, name);
  EXPECT_NE(nullptr, member);
  if (member == nullptr) {
    return nullptr;
  }
  EXPECT_EQ(NODE_TYPE_OBJECT, member->type);
  return member;
}

AstNode const* parseReturnExpression(tests::mocks::MockAqlServer const& server,
                                     std::string_view query) {
  auto queryContext = StandaloneCalculation::buildQueryContext(
      server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  Ast ast(*queryContext);
  QueryString queryString(query);
  Parser parser(*queryContext, ast, queryString);
  parser.parse();

  AstNode const* rootNode = ast.root();
  if (rootNode == nullptr) {
    ADD_FAILURE() << "root node is null for query: " << query;
    return nullptr;
  }
  EXPECT_EQ(NODE_TYPE_ROOT, rootNode->type);
  if (rootNode->numMembers() == 0) {
    return nullptr;
  }

  for (size_t i = 0; i < rootNode->numMembers(); ++i) {
    AstNode const* node = rootNode->getMember(i);
    if (node->type != NODE_TYPE_RETURN) {
      continue;
    }
    EXPECT_EQ(1, node->numMembers());
    if (node->numMembers() != 1) {
      return nullptr;
    }
    return node->getMember(0);
  }

  ADD_FAILURE() << "no RETURN node found in query: " << query;
  return nullptr;
}

AstNode const* findReturnExpression(AstNode const* node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->type == NODE_TYPE_RETURN) {
    if (node->numMembers() != 1) {
      return nullptr;
    }
    return node->getMember(0);
  }

  size_t const n = node->numMembers();
  for (size_t i = 0; i < n; ++i) {
    if (AstNode const* result = findReturnExpression(node->getMember(i))) {
      return result;
    }
  }
  return nullptr;
}

AstNode const* parseAndOptimizeReturnExpression(
    tests::mocks::MockAqlServer const& server, std::string_view query) {
  auto queryContext = StandaloneCalculation::buildQueryContext(
      server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  Ast ast(*queryContext);
  QueryString queryString(query);
  Parser parser(*queryContext, ast, queryString);
  parser.parse();
  ast.validateAndOptimize(queryContext->trxForOptimization(),
                          {.optimizeNonCacheable = true});

  AstNode const* rootNode = ast.root();
  EXPECT_EQ(NODE_TYPE_ROOT, rootNode->type);
  if (rootNode == nullptr) {
    return nullptr;
  }

  AstNode const* returnExpression = findReturnExpression(rootNode);
  if (returnExpression == nullptr) {
    ADD_FAILURE() << "no RETURN node found in query: " << query;
  }
  return returnExpression;
}

bool objectHasObjectSplice(AstNode const* object) {
  EXPECT_EQ(NODE_TYPE_OBJECT, object->type);
  for (size_t i = 0; i < object->numMembers(); ++i) {
    if (object->getMember(i)->type == NODE_TYPE_OBJECT_SPLICE) {
      return true;
    }
  }
  return false;
}

TEST_F(AstNodeTest, constantLetObjectSpliceIsFoldedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "LET x = {foo:1, bar:2, baz:3} RETURN {...x, foo:2}");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_TRUE(objectNode->isConstant());
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  expectObjectInt(objectNode, "foo", 2);
  expectObjectInt(objectNode, "bar", 2);
  expectObjectInt(objectNode, "baz", 3);
}

TEST_F(AstNodeTest, nonConstantLetObjectSpliceIsNotFoldedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "FOR i IN 1..1 LET x = { a: i } RETURN { ...x, b: 2 }");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_FALSE(objectNode->isConstant());
  EXPECT_TRUE(objectHasObjectSplice(objectNode));
}

TEST_F(AstNodeTest, objectLiteralSpliceIsFlattenedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "FOR i IN 1..10 RETURN { ...{ a: i }, b: 2 }");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  EXPECT_FALSE(objectNode->isConstant());

  AstNode const* aValue = getObjectElementValue(objectNode, "a");
  ASSERT_NE(nullptr, aValue);
  EXPECT_EQ(NODE_TYPE_REFERENCE, aValue->type);
  expectObjectInt(objectNode, "b", 2);
}

TEST_F(AstNodeTest, constantObjectLiteralSpliceIsFlattenedDuringOptimization) {
  AstNode const* objectNode =
      parseAndOptimizeReturnExpression(_server, "RETURN { ...{ a: 1 }, b: 2 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  EXPECT_TRUE(objectNode->isConstant());
  expectObjectInt(objectNode, "a", 1);
  expectObjectInt(objectNode, "b", 2);
}

TEST_F(AstNodeTest, objectLiteralSplicePreservesMemberOrder) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server,
      "FOR i IN 1..10 FOR j IN 1..10 RETURN { x: 1, ...{ a: i }, y: 2 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  ASSERT_EQ(3, objectNode->numMembers());
  expectObjectInt(objectNode, "x", 1);
  expectObjectInt(objectNode, "y", 2);
  AstNode const* aValue = getObjectElementValue(objectNode, "a");
  ASSERT_NE(nullptr, aValue);
  EXPECT_EQ(NODE_TYPE_REFERENCE, aValue->type);
}

TEST_F(AstNodeTest,
       multipleObjectLiteralSplicesAreFlattenedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server,
      "FOR i IN 1..10 FOR j IN 1..10 RETURN { ...{ a: i }, ...{ b: j } }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  ASSERT_EQ(2, objectNode->numMembers());
}

TEST_F(AstNodeTest, nestedObjectLiteralSplicesAreFlattenedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server,
      "FOR i IN 1..10 RETURN { ...{ a: i, c: { ...{ x: 1 }, y: 2 } }, b: 2 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_FALSE(objectHasObjectSplice(objectNode));

  AstNode const* cObject = expectObjectObject(objectNode, "c");
  ASSERT_NE(nullptr, cObject);
  EXPECT_FALSE(objectHasObjectSplice(cObject));
  expectObjectInt(cObject, "x", 1);
  expectObjectInt(cObject, "y", 2);
  expectObjectInt(objectNode, "b", 2);
}

TEST_F(AstNodeTest, objectLiteralSpliceWithCalculatedKeyIsFlattened) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server,
      R"aql(FOR i IN 1..10 RETURN { ...{ a: i }, [ CONCAT("he","llo") ] : "world" })aql");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  EXPECT_FALSE(objectNode->isConstant());
  ASSERT_EQ(2, objectNode->numMembers());
  EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, objectNode->getMember(0)->type);
  EXPECT_EQ(NODE_TYPE_CALCULATED_OBJECT_ELEMENT,
            objectNode->getMember(1)->type);
}

TEST_F(AstNodeTest, variableObjectSpliceIsNotFlattenedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "FOR i IN 1..10 RETURN { ...i, b: 2 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_TRUE(objectHasObjectSplice(objectNode));
}

TEST_F(AstNodeTest, functionObjectSpliceIsNotFlattenedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "FOR i IN 1..10 RETURN { ...NOOPT({ a: i }), b: 2 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_TRUE(objectHasObjectSplice(objectNode));
}

TEST_F(AstNodeTest, multipleConstantObjectSplicesAreFoldedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "LET a = { u: 1 } LET b = { v: 2 } RETURN { ...a, ...b, w: 3 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_TRUE(objectNode->isConstant());
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  expectObjectInt(objectNode, "u", 1);
  expectObjectInt(objectNode, "v", 2);
  expectObjectInt(objectNode, "w", 3);
}

TEST_F(AstNodeTest, constantObjectSpliceOverwritePrecedenceIsFolded) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "LET o = { a: 1, b: 2 } RETURN { a: 9, ...o, b: 3 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_TRUE(objectNode->isConstant());
  expectObjectInt(objectNode, "a", 1);
  expectObjectInt(objectNode, "b", 3);
}

TEST_F(AstNodeTest, nestedConstantObjectSpliceIsFoldedDuringOptimization) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server, "LET x = { a: 1 } RETURN { ...{ ...x, b: 2 }, c: 3 }");
  ASSERT_NE(nullptr, objectNode);
  EXPECT_TRUE(objectNode->isConstant());
  EXPECT_FALSE(objectHasObjectSplice(objectNode));
  expectObjectInt(objectNode, "a", 1);
  expectObjectInt(objectNode, "b", 2);
  expectObjectInt(objectNode, "c", 3);
}

TEST_F(AstNodeTest,
       objectWithCalculatedKeyAndConstantSplicesIsPartiallyFolded) {
  AstNode const* objectNode = parseAndOptimizeReturnExpression(
      _server,
      R"aql(RETURN { ...{ a:1, b:2 }, [ CONCAT("he","llo") ] : "world", c:{ ...{ x:10, y:20 }, z:30 } })aql");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_FALSE(objectNode->isConstant());
  EXPECT_FALSE(objectHasObjectSplice(objectNode));

  expectObjectInt(objectNode, "a", 1);
  expectObjectInt(objectNode, "b", 2);

  bool hasCalculated = false;
  for (size_t i = 0; i < objectNode->numMembers(); ++i) {
    if (objectNode->getMember(i)->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
      hasCalculated = true;
      break;
    }
  }
  EXPECT_TRUE(hasCalculated);

  AstNode const* cObject = expectObjectObject(objectNode, "c");
  ASSERT_NE(nullptr, cObject);
  EXPECT_TRUE(cObject->isConstant());
  EXPECT_FALSE(objectHasObjectSplice(cObject));
  expectObjectInt(cObject, "x", 10);
  expectObjectInt(cObject, "y", 20);
  expectObjectInt(cObject, "z", 30);
}

TEST_F(AstNodeTest, constantObjectWithSpliceCanMaterializeToVPack) {
  auto queryContext = StandaloneCalculation::buildQueryContext(
      _server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  Ast ast(*queryContext);
  QueryString queryString(
      std::string_view("RETURN { ...{ a: 1, b: 2 }, c: 3 }"));
  Parser parser(*queryContext, ast, queryString);
  parser.parse();

  AstNode* objectNode = nullptr;
  AstNode const* rootNode = ast.root();
  for (size_t i = 0; i < rootNode->numMembers(); ++i) {
    AstNode const* node = rootNode->getMember(i);
    if (node->type == NODE_TYPE_RETURN) {
      objectNode = node->getMember(0);
      break;
    }
  }
  ASSERT_NE(nullptr, objectNode);
  EXPECT_TRUE(objectNode->isConstant());

  VPackBuilder builder;
  objectNode->toVelocyPackValue(builder);
  VPackSlice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(1, slice.get("a").getInt());
  EXPECT_EQ(2, slice.get("b").getInt());
  EXPECT_EQ(3, slice.get("c").getInt());
}

TEST_F(AstNodeTest, objectWithInlineNullSpliceIsNotConstant) {
  AstNode const* objectNode =
      parseReturnExpression(_server, "RETURN { a: 1, ...null, c: 3 }");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_FALSE(objectNode->isConstant());
  EXPECT_TRUE(objectHasObjectSplice(objectNode));
}

TEST_F(AstNodeTest, objectWithConstantObjectSpliceIsConstant) {
  AstNode const* objectNode =
      parseReturnExpression(_server, "RETURN { ...{ a: 1, b: 2 }, c: 3 }");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_TRUE(objectNode->isConstant());

  ASSERT_EQ(2, objectNode->numMembers());

  AstNode const* spliceNode = objectNode->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT_SPLICE, spliceNode->type);
  ASSERT_EQ(1, spliceNode->numMembers());

  AstNode const* splicedObject = spliceNode->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT, splicedObject->type);
  EXPECT_TRUE(splicedObject->isConstant());
  expectObjectInt(splicedObject, "a", 1);
  expectObjectInt(splicedObject, "b", 2);

  expectObjectInt(objectNode, "c", 3);
}

TEST_F(AstNodeTest, objectWithNonConstantObjectSpliceIsNotConstant) {
  AstNode const* objectNode = parseReturnExpression(
      _server, "FOR i IN 1..10 RETURN { ...{ a: i }, b: 2 }");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_FALSE(objectNode->isConstant());

  ASSERT_EQ(2, objectNode->numMembers());

  AstNode const* spliceNode = objectNode->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT_SPLICE, spliceNode->type);
  ASSERT_EQ(1, spliceNode->numMembers());

  AstNode const* splicedObject = spliceNode->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT, splicedObject->type);
  EXPECT_FALSE(splicedObject->isConstant());

  AstNode const* aValue = getObjectElementValue(splicedObject, "a");
  ASSERT_NE(nullptr, aValue);
  EXPECT_EQ(NODE_TYPE_REFERENCE, aValue->type);

  expectObjectInt(objectNode, "b", 2);
}

TEST_F(AstNodeTest, complexObjectWithSpliceAndCalculatedKeyIsNotConstant) {
  AstNode const* objectNode = parseReturnExpression(
      _server,
      R"aql(RETURN { ...{ a:1, b:2 }, [ CONCAT("he","llo") ] : "world", c:{ ...{ x:10, y:20 }, z:30 } })aql");
  ASSERT_NE(nullptr, objectNode);
  ASSERT_EQ(NODE_TYPE_OBJECT, objectNode->type);
  EXPECT_FALSE(objectNode->isConstant());

  ASSERT_EQ(3, objectNode->numMembers());

  AstNode const* spliceNode = objectNode->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT_SPLICE, spliceNode->type);
  AstNode const* abObject = spliceNode->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT, abObject->type);
  EXPECT_TRUE(abObject->isConstant());
  expectObjectInt(abObject, "a", 1);
  expectObjectInt(abObject, "b", 2);

  AstNode const* helloElement = objectNode->getMember(1);
  ASSERT_EQ(NODE_TYPE_CALCULATED_OBJECT_ELEMENT, helloElement->type);
  ASSERT_EQ(2, helloElement->numMembers());
  ASSERT_EQ(NODE_TYPE_FCALL, helloElement->getMember(0)->type);

  AstNode const* worldValue = helloElement->getMember(1);
  ASSERT_EQ(NODE_TYPE_VALUE, worldValue->type);
  EXPECT_EQ("world", worldValue->getStringView());

  AstNode const* cObject = expectObjectObject(objectNode, "c");
  ASSERT_NE(nullptr, cObject);
  EXPECT_TRUE(cObject->isConstant());
  ASSERT_EQ(2, cObject->numMembers());

  AstNode const* xySplice = cObject->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT_SPLICE, xySplice->type);
  AstNode const* xyObject = xySplice->getMember(0);
  ASSERT_EQ(NODE_TYPE_OBJECT, xyObject->type);
  EXPECT_TRUE(xyObject->isConstant());
  expectObjectInt(xyObject, "x", 10);
  expectObjectInt(xyObject, "y", 20);
  expectObjectInt(cObject, "z", 30);
}

TEST_F(AstNodeTest, toVelocyPackNull) {
  // handle verbose and non-verbose cases in one go
  bool values[] = {true, false};
  for (bool verbose : values) {
    _builder.clear();
    _builder.add(VPackValue(VPackValueType::Null));

    auto validate = [](AstNode const* root) {
      EXPECT_EQ(NODE_TYPE_VALUE, root->type);
      EXPECT_TRUE(root->isNullValue());
    };

    // convert Builder contents to AstNode and call toVelocyPack
    // on the root AstNode
    toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                       verbose);

    // validate the resulting VelocyPack
    VPackSlice s = _builder.slice();

    EXPECT_TRUE(s.isObject());
    EXPECT_EQ("value", s.get("type").stringView());
    EXPECT_TRUE(s.get("value").isNull());

    EXPECT_TRUE(s.get("raw").isNone());

    if (verbose) {
      // create an AstNode from the VelocyPack containing the raw
      // serialized values. this tests whether we can read back the
      // compact serialization format for values
      AstNode* root = _ast->createNode(s);
      validate(root);
    }
  }
}

TEST_F(AstNodeTest, toVelocyPackNumber) {
  // handle verbose and non-verbose cases in one go
  bool values[] = {true, false};
  for (bool verbose : values) {
    _builder.clear();
    _builder.add(VPackValue(123));

    auto validate = [](AstNode const* root) {
      EXPECT_EQ(NODE_TYPE_VALUE, root->type);
      EXPECT_TRUE(root->isIntValue());
      EXPECT_EQ(123, root->getIntValue());
    };

    // convert Builder contents to AstNode and call toVelocyPack
    // on the root AstNode
    toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                       verbose);

    // validate the resulting VelocyPack
    VPackSlice s = _builder.slice();

    EXPECT_TRUE(s.isObject());
    EXPECT_EQ("value", s.get("type").stringView());
    EXPECT_EQ(123, s.get("value").getUInt());

    EXPECT_TRUE(s.get("raw").isNone());

    if (verbose) {
      // create an AstNode from the VelocyPack containing the raw
      // serialized values. this tests whether we can read back the
      // compact serialization format for values
      AstNode* root = _ast->createNode(s);
      validate(root);
    }
  }
}

TEST_F(AstNodeTest, toVelocyPackString) {
  // handle verbose and non-verbose cases in one go
  bool values[] = {true, false};
  for (bool verbose : values) {
    _builder.clear();
    _builder.add(VPackValue("foobarbaz"));

    auto validate = [](AstNode const* root) {
      EXPECT_EQ(NODE_TYPE_VALUE, root->type);
      EXPECT_TRUE(root->isStringValue());
      EXPECT_EQ("foobarbaz", root->getStringView());
    };

    // convert Builder contents to AstNode and call toVelocyPack
    // on the root AstNode
    toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                       verbose);

    // validate the resulting VelocyPack
    VPackSlice s = _builder.slice();

    EXPECT_TRUE(s.isObject());
    EXPECT_EQ("value", s.get("type").stringView());
    EXPECT_EQ("foobarbaz", s.get("value").stringView());

    EXPECT_TRUE(s.get("raw").isNone());

    if (verbose) {
      // create an AstNode from the VelocyPack containing the raw
      // serialized values. this tests whether we can read back the
      // compact serialization format for values
      AstNode* root = _ast->createNode(s);
      validate(root);
    }
  }
}

TEST_F(AstNodeTest, toVelocyPackArrayNonVerbose) {
  _builder.openArray();
  _builder.add(VPackValue(1));
  _builder.add(VPackValue(2));
  _builder.add(VPackValue("foo"));
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_ARRAY, root->type);
    EXPECT_EQ(3, root->numMembers());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(0)->type);
    EXPECT_EQ(1, root->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(1)->type);
    EXPECT_EQ(2, root->getMember(1)->getIntValue());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->type);
    EXPECT_EQ("foo", root->getMember(2)->getStringView());
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ false);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("array", s.get("type").stringView());
  EXPECT_TRUE(s.get("subNodes").isArray());
  EXPECT_EQ(3, s.get("subNodes").length());

  EXPECT_TRUE(s.get("raw").isNone());

  EXPECT_EQ("value", s.get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(1, s.get("subNodes").at(0).get("value").getUInt());
  EXPECT_EQ("value", s.get("subNodes").at(1).get("type").stringView());
  EXPECT_EQ(2, s.get("subNodes").at(1).get("value").getUInt());
  EXPECT_EQ("value", s.get("subNodes").at(2).get("type").stringView());
  EXPECT_EQ("foo", s.get("subNodes").at(2).get("value").stringView());
}

TEST_F(AstNodeTest, toVelocyPackArrayVerbose) {
  _builder.openArray();
  _builder.add(VPackValue(1));
  _builder.add(VPackValue(2));
  _builder.add(VPackValue("foo"));
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_ARRAY, root->type);
    EXPECT_EQ(3, root->numMembers());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(0)->type);
    EXPECT_EQ(1, root->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(1)->type);
    EXPECT_EQ(2, root->getMember(1)->getIntValue());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->type);
    EXPECT_EQ("foo", root->getMember(2)->getStringView());
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ true);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("array", s.get("type").stringView());
  EXPECT_TRUE(s.get("raw").isArray());
  EXPECT_EQ(3, s.get("raw").length());

  EXPECT_TRUE(s.get("subNodes").isNone());

  EXPECT_EQ(1, s.get("raw").at(0).getUInt());
  EXPECT_EQ(2, s.get("raw").at(1).getUInt());
  EXPECT_EQ("foo", s.get("raw").at(2).stringView());

  // create an AstNode from the VelocyPack containing the raw
  // serialized values. this tests whether we can read back the
  // compact serialization format for values
  AstNode* root = _ast->createNode(s);
  validate(root);
}

TEST_F(AstNodeTest, toVelocyPackNestedArrayNonVerbose) {
  _builder.openArray();
  _builder.add(VPackValue(1));
  _builder.add(VPackValue(2));
  _builder.openArray();
  _builder.add(VPackValue("foo"));
  _builder.add(VPackValue("bar"));
  _builder.close();
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_ARRAY, root->type);
    EXPECT_EQ(3, root->numMembers());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(0)->type);
    EXPECT_EQ(1, root->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(1)->type);
    EXPECT_EQ(2, root->getMember(1)->getIntValue());
    EXPECT_EQ(NODE_TYPE_ARRAY, root->getMember(2)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->getMember(0)->type);
    EXPECT_EQ("foo", root->getMember(2)->getMember(0)->getStringView());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->getMember(1)->type);
    EXPECT_EQ("bar", root->getMember(2)->getMember(1)->getStringView());
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ false);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("array", s.get("type").stringView());
  EXPECT_TRUE(s.get("subNodes").isArray());
  EXPECT_EQ(3, s.get("subNodes").length());

  EXPECT_TRUE(s.get("raw").isNone());

  EXPECT_EQ("value", s.get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(1, s.get("subNodes").at(0).get("value").getUInt());
  EXPECT_EQ("value", s.get("subNodes").at(1).get("type").stringView());
  EXPECT_EQ(2, s.get("subNodes").at(1).get("value").getUInt());

  EXPECT_EQ("array", s.get("subNodes").at(2).get("type").stringView());
  EXPECT_TRUE(s.get("subNodes").at(2).get("subNodes").isArray());
  EXPECT_EQ(2, s.get("subNodes").at(2).get("subNodes").length());
  EXPECT_EQ(
      "value",
      s.get("subNodes").at(2).get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(
      "foo",
      s.get("subNodes").at(2).get("subNodes").at(0).get("value").stringView());
  EXPECT_EQ(
      "value",
      s.get("subNodes").at(2).get("subNodes").at(1).get("type").stringView());
  EXPECT_EQ(
      "bar",
      s.get("subNodes").at(2).get("subNodes").at(1).get("value").stringView());
}

TEST_F(AstNodeTest, toVelocyPackNestedArrayVerbose) {
  _builder.openArray();
  _builder.add(VPackValue(1));
  _builder.add(VPackValue(2));
  _builder.openArray();
  _builder.add(VPackValue("foo"));
  _builder.add(VPackValue("bar"));
  _builder.close();
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_ARRAY, root->type);
    EXPECT_EQ(3, root->numMembers());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(0)->type);
    EXPECT_EQ(1, root->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(1)->type);
    EXPECT_EQ(2, root->getMember(1)->getIntValue());
    EXPECT_EQ(NODE_TYPE_ARRAY, root->getMember(2)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->getMember(0)->type);
    EXPECT_EQ("foo", root->getMember(2)->getMember(0)->getStringView());
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->getMember(1)->type);
    EXPECT_EQ("bar", root->getMember(2)->getMember(1)->getStringView());
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ true);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("array", s.get("type").stringView());
  EXPECT_TRUE(s.get("raw").isArray());
  EXPECT_EQ(3, s.get("raw").length());

  EXPECT_TRUE(s.get("subNodes").isNone());

  EXPECT_EQ(1, s.get("raw").at(0).getUInt());
  EXPECT_EQ(2, s.get("raw").at(1).getUInt());

  EXPECT_TRUE(s.get("raw").at(2).isArray());
  EXPECT_EQ(2, s.get("raw").at(2).length());
  EXPECT_EQ("foo", s.get("raw").at(2).at(0).stringView());
  EXPECT_EQ("bar", s.get("raw").at(2).at(1).stringView());

  // create an AstNode from the VelocyPack containing the raw
  // serialized values. this tests whether we can read back the
  // compact serialization format for values
  AstNode* root = _ast->createNode(s);
  validate(root);
}

TEST_F(AstNodeTest, toVelocyPackObjectNonVerbose) {
  _builder.openObject();
  _builder.add("foo", VPackValue(1));
  _builder.add("bar", VPackValue(2));
  _builder.add("baz", VPackValue("foo"));
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_OBJECT, root->type);
    EXPECT_EQ(3, root->numMembers());
    expectObjectInt(root, "foo", 1);
    expectObjectInt(root, "bar", 2);
    expectObjectString(root, "baz", "foo");
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ false);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("object", s.get("type").stringView());
  EXPECT_TRUE(s.get("subNodes").isArray());
  EXPECT_EQ(3, s.get("subNodes").length());

  EXPECT_TRUE(s.get("raw").isNone());

  EXPECT_EQ("object element", s.get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ("foo", s.get("subNodes").at(0).get("name").stringView());
  EXPECT_EQ(
      "value",
      s.get("subNodes").at(0).get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(
      1, s.get("subNodes").at(0).get("subNodes").at(0).get("value").getUInt());
  EXPECT_EQ("object element", s.get("subNodes").at(1).get("type").stringView());
  EXPECT_EQ("bar", s.get("subNodes").at(1).get("name").stringView());
  EXPECT_EQ(
      "value",
      s.get("subNodes").at(1).get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(
      2, s.get("subNodes").at(1).get("subNodes").at(0).get("value").getUInt());
  EXPECT_EQ("object element", s.get("subNodes").at(2).get("type").stringView());
  EXPECT_EQ("baz", s.get("subNodes").at(2).get("name").stringView());
  EXPECT_EQ(
      "value",
      s.get("subNodes").at(2).get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(
      "foo",
      s.get("subNodes").at(2).get("subNodes").at(0).get("value").stringView());
}

TEST_F(AstNodeTest, toVelocyPackObjectVerbose) {
  _builder.openObject();
  _builder.add("foo", VPackValue(1));
  _builder.add("bar", VPackValue(2));
  _builder.add("baz", VPackValue("foo"));
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_OBJECT, root->type);
    EXPECT_EQ(3, root->numMembers());
    expectObjectInt(root, "foo", 1);
    expectObjectInt(root, "bar", 2);
    expectObjectString(root, "baz", "foo");
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ true);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("object", s.get("type").stringView());
  EXPECT_TRUE(s.get("raw").isObject());
  EXPECT_EQ(3, s.get("raw").length());

  EXPECT_TRUE(s.get("subNodes").isNone());

  EXPECT_EQ(1, s.get("raw").get("foo").getUInt());
  EXPECT_EQ(2, s.get("raw").get("bar").getUInt());
  EXPECT_EQ("foo", s.get("raw").get("baz").stringView());

  // create an AstNode from the VelocyPack containing the raw
  // serialized values. this tests whether we can read back the
  // compact serialization format for values
  AstNode* root = _ast->createNode(s);
  validate(root);
}

TEST_F(AstNodeTest, toVelocyPackNestedObjectNonVerbose) {
  _builder.openObject();
  _builder.add("foo", VPackValue(1));
  _builder.add("bar", VPackValue(2));
  _builder.add("baz", VPackValue(VPackValueType::Object));
  _builder.add("qux", VPackValue(true));
  _builder.add("quetzal", VPackValue(VPackValueType::Object));
  _builder.add("bark", VPackValue(VPackValueType::Array));
  _builder.add(VPackValue(666));
  _builder.close();
  _builder.close();
  _builder.close();
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_OBJECT, root->type);
    EXPECT_EQ(3, root->numMembers());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(0)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(0)->getMember(0)->type);
    EXPECT_EQ("foo", root->getMember(0)->getStringView());
    EXPECT_EQ(1, root->getMember(0)->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(1)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(1)->getMember(0)->type);
    EXPECT_EQ("bar", root->getMember(1)->getStringView());
    EXPECT_EQ(2, root->getMember(1)->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(2)->type);
    EXPECT_EQ(NODE_TYPE_OBJECT, root->getMember(2)->getMember(0)->type);
    EXPECT_EQ("baz", root->getMember(2)->getStringView());
    EXPECT_EQ(NODE_TYPE_OBJECT, root->getMember(2)->getMember(0)->type);
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT,
              root->getMember(2)->getMember(0)->getMember(0)->type);
    EXPECT_EQ("qux",
              root->getMember(2)->getMember(0)->getMember(0)->getStringView());
    EXPECT_EQ(
        NODE_TYPE_VALUE,
        root->getMember(2)->getMember(0)->getMember(0)->getMember(0)->type);
    EXPECT_TRUE(root->getMember(2)
                    ->getMember(0)
                    ->getMember(0)
                    ->getMember(0)
                    ->isBoolValue());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT,
              root->getMember(2)->getMember(0)->getMember(1)->type);
    EXPECT_EQ("quetzal",
              root->getMember(2)->getMember(0)->getMember(1)->getStringView());
    EXPECT_EQ(
        NODE_TYPE_OBJECT,
        root->getMember(2)->getMember(0)->getMember(1)->getMember(0)->type);
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(2)
                                            ->getMember(0)
                                            ->getMember(1)
                                            ->getMember(0)
                                            ->getMember(0)
                                            ->type);
    EXPECT_EQ("bark", root->getMember(2)
                          ->getMember(0)
                          ->getMember(1)
                          ->getMember(0)
                          ->getMember(0)
                          ->getStringView());

    EXPECT_EQ(NODE_TYPE_ARRAY, root->getMember(2)
                                   ->getMember(0)
                                   ->getMember(1)
                                   ->getMember(0)
                                   ->getMember(0)
                                   ->getMember(0)
                                   ->type);

    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)
                                   ->getMember(0)
                                   ->getMember(1)
                                   ->getMember(0)
                                   ->getMember(0)
                                   ->getMember(0)
                                   ->getMember(0)
                                   ->type);

    EXPECT_EQ(666, root->getMember(2)
                       ->getMember(0)
                       ->getMember(1)
                       ->getMember(0)
                       ->getMember(0)
                       ->getMember(0)
                       ->getMember(0)
                       ->getIntValue());
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ false);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("object", s.get("type").stringView());
  EXPECT_TRUE(s.get("subNodes").isArray());
  EXPECT_EQ(3, s.get("subNodes").length());

  EXPECT_TRUE(s.get("raw").isNone());

  EXPECT_EQ("object element", s.get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ("foo", s.get("subNodes").at(0).get("name").stringView());
  EXPECT_EQ(
      "value",
      s.get("subNodes").at(0).get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(
      1, s.get("subNodes").at(0).get("subNodes").at(0).get("value").getUInt());
  EXPECT_EQ("object element", s.get("subNodes").at(1).get("type").stringView());
  EXPECT_EQ("bar", s.get("subNodes").at(1).get("name").stringView());
  EXPECT_EQ(
      "value",
      s.get("subNodes").at(1).get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ(
      2, s.get("subNodes").at(1).get("subNodes").at(0).get("value").getUInt());
  EXPECT_EQ("object element", s.get("subNodes").at(2).get("type").stringView());
  EXPECT_EQ("baz", s.get("subNodes").at(2).get("name").stringView());
  EXPECT_EQ(
      "object",
      s.get("subNodes").at(2).get("subNodes").at(0).get("type").stringView());
  EXPECT_EQ("object element", s.get("subNodes")
                                  .at(2)
                                  .get("subNodes")
                                  .at(0)
                                  .get("subNodes")
                                  .at(0)
                                  .get("type")
                                  .stringView());
  EXPECT_EQ("qux", s.get("subNodes")
                       .at(2)
                       .get("subNodes")
                       .at(0)
                       .get("subNodes")
                       .at(0)
                       .get("name")
                       .stringView());
  EXPECT_EQ("value", s.get("subNodes")
                         .at(2)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(0)
                         .get("type")
                         .stringView());
  EXPECT_EQ(true, s.get("subNodes")
                      .at(2)
                      .get("subNodes")
                      .at(0)
                      .get("subNodes")
                      .at(0)
                      .get("subNodes")
                      .at(0)
                      .get("value")
                      .getBoolean());

  EXPECT_EQ("object element", s.get("subNodes")
                                  .at(2)
                                  .get("subNodes")
                                  .at(0)
                                  .get("subNodes")
                                  .at(1)
                                  .get("type")
                                  .stringView());
  EXPECT_EQ("quetzal", s.get("subNodes")
                           .at(2)
                           .get("subNodes")
                           .at(0)
                           .get("subNodes")
                           .at(1)
                           .get("name")
                           .stringView());
  EXPECT_EQ("object", s.get("subNodes")
                          .at(2)
                          .get("subNodes")
                          .at(0)
                          .get("subNodes")
                          .at(1)
                          .get("subNodes")
                          .at(0)
                          .get("type")
                          .stringView());

  EXPECT_EQ("object element", s.get("subNodes")
                                  .at(2)
                                  .get("subNodes")
                                  .at(0)
                                  .get("subNodes")
                                  .at(1)
                                  .get("subNodes")
                                  .at(0)
                                  .get("subNodes")
                                  .at(0)
                                  .get("type")
                                  .stringView());
  EXPECT_EQ("bark", s.get("subNodes")
                        .at(2)
                        .get("subNodes")
                        .at(0)
                        .get("subNodes")
                        .at(1)
                        .get("subNodes")
                        .at(0)
                        .get("subNodes")
                        .at(0)
                        .get("name")
                        .stringView());

  EXPECT_EQ("array", s.get("subNodes")
                         .at(2)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(1)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(0)
                         .get("type")
                         .stringView());

  EXPECT_EQ("value", s.get("subNodes")
                         .at(2)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(1)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(0)
                         .get("subNodes")
                         .at(0)
                         .get("type")
                         .stringView());

  EXPECT_EQ(666, s.get("subNodes")
                     .at(2)
                     .get("subNodes")
                     .at(0)
                     .get("subNodes")
                     .at(1)
                     .get("subNodes")
                     .at(0)
                     .get("subNodes")
                     .at(0)
                     .get("subNodes")
                     .at(0)
                     .get("subNodes")
                     .at(0)
                     .get("value")
                     .getUInt());
}

TEST_F(AstNodeTest, toVelocyPackNestedObjectVerbose) {
  _builder.openObject();
  _builder.add("foo", VPackValue(1));
  _builder.add("bar", VPackValue(2));
  _builder.add("baz", VPackValue(VPackValueType::Object));
  _builder.add("qux", VPackValue(true));
  _builder.add("quetzal", VPackValue(VPackValueType::Object));
  _builder.add("bark", VPackValue(VPackValueType::Array));
  _builder.add(VPackValue(666));
  _builder.close();
  _builder.close();
  _builder.close();
  _builder.close();

  auto validate = [](AstNode const* root) {
    EXPECT_EQ(NODE_TYPE_OBJECT, root->type);
    EXPECT_EQ(3, root->numMembers());
    expectObjectInt(root, "foo", 1);
    expectObjectInt(root, "bar", 2);

    AstNode const* baz = expectObjectObject(root, "baz");
    ASSERT_NE(nullptr, baz);
    EXPECT_EQ(2, baz->numMembers());

    AstNode const* qux = getObjectElementValue(baz, "qux");
    ASSERT_NE(nullptr, qux);
    ASSERT_EQ(NODE_TYPE_VALUE, qux->type);
    EXPECT_TRUE(qux->isBoolValue());

    AstNode const* quetzal = expectObjectObject(baz, "quetzal");
    ASSERT_NE(nullptr, quetzal);
    EXPECT_EQ(1, quetzal->numMembers());

    AstNode const* bark = getObjectElementValue(quetzal, "bark");
    ASSERT_NE(nullptr, bark);
    ASSERT_EQ(NODE_TYPE_ARRAY, bark->type);
    ASSERT_EQ(1, bark->numMembers());
    ASSERT_EQ(NODE_TYPE_VALUE, bark->getMember(0)->type);
    EXPECT_EQ(666, bark->getMember(0)->getIntValue());
  };

  // convert Builder contents to AstNode and call toVelocyPack
  // on the root AstNode
  toVelocyPackHelper([&validate](AstNode const* root) { validate(root); },
                     /*verbose*/ true);

  // validate the resulting VelocyPack
  VPackSlice s = _builder.slice();

  EXPECT_TRUE(s.isObject());
  EXPECT_EQ("object", s.get("type").stringView());
  EXPECT_TRUE(s.get("raw").isObject());
  EXPECT_EQ(3, s.get("raw").length());

  EXPECT_TRUE(s.get("subNodes").isNone());

  EXPECT_EQ(1, s.get("raw").get("foo").getUInt());
  EXPECT_EQ(2, s.get("raw").get("bar").getUInt());
  EXPECT_TRUE(s.get("raw").get("baz").isObject());
  EXPECT_TRUE(s.get("raw").get("baz").get("subNodes").isNone());
  EXPECT_TRUE(s.get("raw").get("baz").get("qux").isTrue());
  EXPECT_TRUE(s.get("raw").get("baz").get("quetzal").isObject());
  EXPECT_TRUE(s.get("raw").get("baz").get("quetzal").get("subNodes").isNone());
  EXPECT_TRUE(s.get("raw").get("baz").get("quetzal").get("bark").isArray());
  EXPECT_EQ(666,
            s.get("raw").get("baz").get("quetzal").get("bark").at(0).getUInt());

  // create an AstNode from the VelocyPack containing the raw
  // serialized values. this tests whether we can read back the
  // compact serialization format for values
  AstNode* root = _ast->createNode(s);
  validate(root);
}

class CompareAstNodesTest : public ::testing::Test {
 public:
  CompareAstNodesTest()
      : _server{}, _query{_server.createFakeQuery()}, _ast{_query->ast()} {}

 protected:
  tests::mocks::MockAqlServer _server;
  std::shared_ptr<Query> _query;
  Ast* _ast;

  Variable* makeVar(std::string_view name) {
    return _ast->variables()->createVariable(name, /*isUserDefined*/ true);
  }

  AstNode* createRefNode(Variable* v) { return _ast->createNodeReference(v); }

  AstNode* attr(AstNode* base, std::string_view name) {
    return _ast->createNodeAttributeAccess(base, name);
  }

  AstNode* intVal(int64_t v) { return _ast->createNodeValueInt(v); }

  AstNode* strVal(std::string_view s) {
    return _ast->createNodeValueString(s.data(), s.size());
  }

  AstNode* createBinaryOp(AstNodeType t, AstNode* lhs, AstNode* rhs) {
    return _ast->createNodeBinaryOperator(t, lhs, rhs);
  }

  AstNode* naryOp(AstNodeType t, std::initializer_list<AstNode*> children) {
    AstNode* node = _ast->createNodeNaryOperator(t);
    for (auto* c : children) {
      node->addMember(c);
    }
    return node;
  }

  // Build `lhs IN [elems...]`
  AstNode* createInOp(AstNode* lhs, std::initializer_list<AstNode*> elems) {
    AstNode* arr = _ast->createNodeArray(elems.size());
    for (auto* e : elems) {
      arr->addMember(e);
    }
    return createBinaryOp(NODE_TYPE_OPERATOR_BINARY_IN, lhs, arr);
  }

  AstNode* createNinOp(AstNode* lhs, std::initializer_list<AstNode*> elems) {
    AstNode* arr = _ast->createNodeArray(elems.size());
    for (auto* e : elems) {
      arr->addMember(e);
    }
    return createBinaryOp(NODE_TYPE_OPERATOR_BINARY_NIN, lhs, arr);
  }

  AstNode* fcall(std::string_view funcName,
                 std::initializer_list<AstNode*> args) {
    AstNode* argsNode = _ast->createNodeArray(args.size());
    for (auto* a : args) {
      argsNode->addMember(a);
    }
    return _ast->createNodeFunctionCall(funcName, argsNode,
                                        /*allowInternalFunctions*/ false);
  }

  // Creates a NODE_TYPE_FCALL_USER node via VPack deserialization instead of
  // createNodeFunctionCall, which requires V8, and we just want to test the
  // comparator of the user functions here.
  AstNode* fcallUser(std::string_view name) {
    VPackBuilder b;
    b.openObject();
    b.add("typeID", VPackValue(static_cast<int>(NODE_TYPE_FCALL_USER)));
    b.add("name", VPackValue(name));
    b.close();
    return _ast->createNode(b.slice());
  }

  // <true> only works for compile-time constants, and test nodes use loop
  // variables.
  int compare(AstNode const* lhs, AstNode const* rhs, bool utf8 = false) {
    return compareAstNodes<false>(lhs, rhs, utf8);
  }
};

// --- constant value comparisons
// -----------------------------------------------

TEST_F(CompareAstNodesTest, constantInts) {
  EXPECT_EQ(0, compare(intVal(0), intVal(0)));
  EXPECT_LT(compare(intVal(1), intVal(2)), 0);
  EXPECT_GT(compare(intVal(2), intVal(1)), 0);
}

TEST_F(CompareAstNodesTest, constantStrings) {
  EXPECT_EQ(0, compare(strVal("foo"), strVal("foo")));
  EXPECT_LT(compare(strVal("bar"), strVal("foo")), 0);
  EXPECT_GT(compare(strVal("foo"), strVal("bar")), 0);
}

TEST_F(CompareAstNodesTest, nullLessThanBool) {
  auto* n = _ast->createNodeValueNull();
  auto* b = _ast->createNodeValueBool(false);
  EXPECT_LT(compare(n, b), 0);
  EXPECT_GT(compare(b, n), 0);
}

// --- structural vs constant type ordering
// -------------------------------------

// String literals sort before structural expressions in the ordering.
TEST_F(CompareAstNodesTest, stringLiteralBeforeStructural) {
  auto* x = makeVar("x");
  EXPECT_LT(compare(strVal("z"), createRefNode(x)), 0);
  EXPECT_GT(compare(createRefNode(x), strVal("z")), 0);
}

// --- REFERENCE nodes
// ----------------------------------------------------------

TEST_F(CompareAstNodesTest, referencesSameVariable) {
  auto* x = makeVar("x");
  EXPECT_EQ(0, compare(createRefNode(x), createRefNode(x)));
}

TEST_F(CompareAstNodesTest, referencesDifferentVariablesOrderedById) {
  // Variables get monotonically increasing ids, so creation order == id order.
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  ASSERT_LT(a->id, b->id);
  EXPECT_LT(compare(createRefNode(a), createRefNode(b)), 0);
  EXPECT_GT(compare(createRefNode(b), createRefNode(a)), 0);
}

// Same variable name in different Variable objects → different ids → not equal.
TEST_F(CompareAstNodesTest, referencesDistinctVariablesSameName) {
  auto* a1 = makeVar("a");
  auto* a2 = makeVar("a");  // different object, different id
  EXPECT_NE(a1->id, a2->id);
  EXPECT_NE(0, compare(createRefNode(a1), createRefNode(a2)));
}

// --- ATTRIBUTE_ACCESS nodes
// ---------------------------------------------------

TEST_F(CompareAstNodesTest, attributeAccessSameBaseAndName) {
  auto* x = makeVar("x");
  EXPECT_EQ(0, compare(attr(createRefNode(x), "name"),
                       attr(createRefNode(x), "name")));
}

TEST_F(CompareAstNodesTest, attributeAccessDifferentNameOrderedLexically) {
  auto* x = makeVar("x");
  EXPECT_LT(
      compare(attr(createRefNode(x), "age"), attr(createRefNode(x), "name")),
      0);
  EXPECT_GT(
      compare(attr(createRefNode(x), "name"), attr(createRefNode(x), "age")),
      0);
}

TEST_F(CompareAstNodesTest, attributeAccessDifferentBase) {
  auto* x = makeVar("x");
  auto* y = makeVar("y");
  ASSERT_LT(x->id, y->id);
  // Same attr name "k"; bases differ by variable id.
  int cmp = compare(attr(createRefNode(x), "k"), attr(createRefNode(y), "k"));
  EXPECT_LT(cmp, 0);
}

TEST_F(CompareAstNodesTest, nestedAttributeAccessEqual) {
  auto* x = makeVar("x");
  auto* lhs = attr(attr(createRefNode(x), "a"), "b");
  auto* rhs = attr(attr(createRefNode(x), "a"), "b");
  EXPECT_EQ(0, compare(lhs, rhs));
}

TEST_F(CompareAstNodesTest, nestedAttributeAccessDifferentLeaf) {
  auto* x = makeVar("x");
  EXPECT_NE(0, compare(attr(attr(createRefNode(x), "a"), "b"),
                       attr(attr(createRefNode(x), "a"), "c")));
}

// --- commutative binary operators (EQ / NE)
// -----------------------------------

TEST_F(CompareAstNodesTest, equalityCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_EQ, createRefNode(a),
                            createRefNode(b));
  auto* ba = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_EQ, createRefNode(b),
                            createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, inequalityCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_NE, createRefNode(a),
                            createRefNode(b));
  auto* ba = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_NE, createRefNode(b),
                            createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, lessThanNotCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_LT, createRefNode(a),
                            createRefNode(b));
  auto* ba = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_LT, createRefNode(b),
                            createRefNode(a));
  EXPECT_NE(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, additionCommutative) {
  auto* plus12 =
      createBinaryOp(NODE_TYPE_OPERATOR_BINARY_PLUS, intVal(1), intVal(2));
  auto* plus21 =
      createBinaryOp(NODE_TYPE_OPERATOR_BINARY_PLUS, intVal(2), intVal(1));
  EXPECT_EQ(0, compare(plus12, plus21));
}

TEST_F(CompareAstNodesTest, binaryAndCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_AND, createRefNode(a),
                            createRefNode(b));
  auto* ba = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_AND, createRefNode(b),
                            createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, binaryOrCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_OR, createRefNode(a),
                            createRefNode(b));
  auto* ba = createBinaryOp(NODE_TYPE_OPERATOR_BINARY_OR, createRefNode(b),
                            createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, multiplicationCommutative) {
  auto* m12 =
      createBinaryOp(NODE_TYPE_OPERATOR_BINARY_TIMES, intVal(1), intVal(2));
  auto* m21 =
      createBinaryOp(NODE_TYPE_OPERATOR_BINARY_TIMES, intVal(2), intVal(1));
  EXPECT_EQ(0, compare(m12, m21));
}

TEST_F(CompareAstNodesTest, equalityDifferentRhsNotEqual) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* c = makeVar("c");
  EXPECT_NE(0, compare(createBinaryOp(NODE_TYPE_OPERATOR_BINARY_EQ,
                                      createRefNode(a), createRefNode(b)),
                       createBinaryOp(NODE_TYPE_OPERATOR_BINARY_EQ,
                                      createRefNode(a), createRefNode(c))));
}

// --- FCALL nodes
// --------------------------------------------------------------

TEST_F(CompareAstNodesTest, fcallSameFunction) {
  auto* x = makeVar("x");
  EXPECT_EQ(0, compare(fcall("LENGTH", {createRefNode(x)}),
                       fcall("LENGTH", {createRefNode(x)})));
}

TEST_F(CompareAstNodesTest, fcallOrderedByName) {
  auto* x = makeVar("x");
  EXPECT_LT(compare(fcall("LENGTH", {createRefNode(x)}),
                    fcall("UPPER", {createRefNode(x)})),
            0);
  EXPECT_GT(compare(fcall("UPPER", {createRefNode(x)}),
                    fcall("LENGTH", {createRefNode(x)})),
            0);
}

TEST_F(CompareAstNodesTest, fcallDifferentArgs) {
  auto* x = makeVar("x");
  auto* y = makeVar("y");
  ASSERT_LT(x->id, y->id);
  EXPECT_LT(compare(fcall("LENGTH", {createRefNode(x)}),
                    fcall("LENGTH", {createRefNode(y)})),
            0);
}

TEST_F(CompareAstNodesTest, fcallDifferentArgumentCount) {
  auto* x = makeVar("x");
  auto* y = makeVar("y");
  EXPECT_NE(0, compare(fcall("CONCAT", {createRefNode(x)}),
                       fcall("CONCAT", {createRefNode(x), createRefNode(y)})));
}

TEST_F(CompareAstNodesTest, fcallUserSameNameEqual) {
  EXPECT_EQ(0, compare(fcallUser("my::func"), fcallUser("my::func")));
}

TEST_F(CompareAstNodesTest, fcallUserDifferentNamesNotEqual) {
  EXPECT_NE(0, compare(fcallUser("my::func1"), fcallUser("my::func2")));
}

TEST_F(CompareAstNodesTest, fcallNonDeterministicNeverEqual) {
  // Two different RAND() nodes must not compare as equal. Also verify
  // anti-symmetry: compare(a,b) and compare(b,a) must have opposite signs.
  auto* r1 = fcall("RAND", {});
  auto* r2 = fcall("RAND", {});
  int const cmp = compare(r1, r2);
  EXPECT_NE(0, cmp);
  EXPECT_EQ(-cmp, compare(r2, r1));
}

// --- NARY operators
// -----------------------------------------------------------

TEST_F(CompareAstNodesTest, naryAndOrderIndependent) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* c = makeVar("c");
  auto* abc = naryOp(NODE_TYPE_OPERATOR_NARY_AND,
                     {createRefNode(a), createRefNode(b), createRefNode(c)});
  auto* cab = naryOp(NODE_TYPE_OPERATOR_NARY_AND,
                     {createRefNode(c), createRefNode(a), createRefNode(b)});
  EXPECT_EQ(0, compare(abc, cab));
}

TEST_F(CompareAstNodesTest, naryAndDifferentMembersNotEqual) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* c = makeVar("c");
  auto* ab =
      naryOp(NODE_TYPE_OPERATOR_NARY_AND, {createRefNode(a), createRefNode(b)});
  auto* ac =
      naryOp(NODE_TYPE_OPERATOR_NARY_AND, {createRefNode(a), createRefNode(c)});
  EXPECT_NE(0, compare(ab, ac));
}

TEST_F(CompareAstNodesTest, naryAndDifferentCountsNotEqual) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* c = makeVar("c");
  auto* ab =
      naryOp(NODE_TYPE_OPERATOR_NARY_AND, {createRefNode(a), createRefNode(b)});
  auto* abc = naryOp(NODE_TYPE_OPERATOR_NARY_AND,
                     {createRefNode(a), createRefNode(b), createRefNode(c)});
  EXPECT_NE(0, compare(ab, abc));
}

TEST_F(CompareAstNodesTest, naryOrOrderIndependent) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab =
      naryOp(NODE_TYPE_OPERATOR_NARY_OR, {createRefNode(a), createRefNode(b)});
  auto* ba =
      naryOp(NODE_TYPE_OPERATOR_NARY_OR, {createRefNode(b), createRefNode(a)});
  EXPECT_EQ(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, naryOrDifferentCountsNotEqual) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* c = makeVar("c");
  auto* ab =
      naryOp(NODE_TYPE_OPERATOR_NARY_OR, {createRefNode(a), createRefNode(b)});
  auto* abc = naryOp(NODE_TYPE_OPERATOR_NARY_OR,
                     {createRefNode(a), createRefNode(b), createRefNode(c)});
  EXPECT_NE(0, compare(ab, abc));
}

// --- IN / NIN array element order independence
// ---------------------------------

TEST_F(CompareAstNodesTest, inArrayElementOrderIndependent) {
  auto* x = makeVar("x");
  auto* in12 = createInOp(createRefNode(x), {intVal(1), intVal(2)});
  auto* in21 = createInOp(createRefNode(x), {intVal(2), intVal(1)});
  EXPECT_EQ(0, compare(in12, in21));
}

TEST_F(CompareAstNodesTest, ninArrayElementOrderIndependent) {
  auto* x = makeVar("x");
  auto* nin12 = createNinOp(createRefNode(x), {intVal(1), intVal(2)});
  auto* nin21 = createNinOp(createRefNode(x), {intVal(2), intVal(1)});
  EXPECT_EQ(0, compare(nin12, nin21));
}

TEST_F(CompareAstNodesTest, inDifferentOperandsNotEqual) {
  auto* x = makeVar("x");
  auto* y = makeVar("y");
  auto* inX = createInOp(createRefNode(x), {intVal(1)});
  auto* inY = createInOp(createRefNode(y), {intVal(1)});
  EXPECT_NE(0, compare(inX, inY));
}

TEST_F(CompareAstNodesTest, inDifferentArraySizeNotEqual) {
  auto* x = makeVar("x");
  auto* in1 = createInOp(createRefNode(x), {intVal(1)});
  auto* in12 = createInOp(createRefNode(x), {intVal(1), intVal(2)});
  EXPECT_NE(0, compare(in1, in12));
}

TEST_F(CompareAstNodesTest, inAndNinNotEqual) {
  auto* x = makeVar("x");
  EXPECT_NE(0, compare(createInOp(createRefNode(x), {intVal(1)}),
                       createNinOp(createRefNode(x), {intVal(1)})));
}

// --- bind parameters
// ----------------------------------------------------------

TEST_F(CompareAstNodesTest, bindParameterSameNameEqual) {
  auto* p1 = _ast->createNodeParameter("x");
  auto* p2 = _ast->createNodeParameter("x");
  EXPECT_EQ(0, compare(p1, p2));
}

TEST_F(CompareAstNodesTest, bindParameterDifferentNamesNotEqual) {
  auto* p1 = _ast->createNodeParameter("x");
  auto* p2 = _ast->createNodeParameter("y");
  EXPECT_NE(0, compare(p1, p2));
}

TEST_F(CompareAstNodesTest, datasourceParameterSameNameEqual) {
  auto* p1 = _ast->createNodeParameterDatasource("coll");
  auto* p2 = _ast->createNodeParameterDatasource("coll");
  EXPECT_EQ(0, compare(p1, p2));
}

TEST_F(CompareAstNodesTest, datasourceParameterDifferentNamesNotEqual) {
  auto* p1 = _ast->createNodeParameterDatasource("coll1");
  auto* p2 = _ast->createNodeParameterDatasource("coll2");
  EXPECT_NE(0, compare(p1, p2));
}

// --- quantifiers
// -------------------------------------------------------------

TEST_F(CompareAstNodesTest, quantifierSameKindEqual) {
  auto* all1 = _ast->createNodeQuantifier(Quantifier::Type::kAll);
  auto* all2 = _ast->createNodeQuantifier(Quantifier::Type::kAll);
  EXPECT_EQ(0, compare(all1, all2));
}

TEST_F(CompareAstNodesTest, quantifierDifferentKindsNotEqual) {
  auto* all = _ast->createNodeQuantifier(Quantifier::Type::kAll);
  auto* none = _ast->createNodeQuantifier(Quantifier::Type::kNone);
  EXPECT_NE(0, compare(all, none));
}

TEST_F(CompareAstNodesTest, quantifierAtLeastSameThresholdEqual) {
  auto* al2a =
      _ast->createNodeQuantifier(Quantifier::Type::kAtLeast, intVal(2));
  auto* al2b =
      _ast->createNodeQuantifier(Quantifier::Type::kAtLeast, intVal(2));
  EXPECT_EQ(0, compare(al2a, al2b));
}

TEST_F(CompareAstNodesTest, quantifierAtLeastDifferentThresholdNotEqual) {
  auto* al2 = _ast->createNodeQuantifier(Quantifier::Type::kAtLeast, intVal(2));
  auto* al3 = _ast->createNodeQuantifier(Quantifier::Type::kAtLeast, intVal(3));
  EXPECT_NE(0, compare(al2, al3));
}

TEST_F(CompareAstNodesTest, quantifierAtLeastNotEqualAll) {
  auto* al2 = _ast->createNodeQuantifier(Quantifier::Type::kAtLeast, intVal(2));
  auto* all = _ast->createNodeQuantifier(Quantifier::Type::kAll);
  EXPECT_NE(0, compare(al2, all));
}

TEST_F(CompareAstNodesTest, quantifierAtLeastDifferentNonConstantThreshold) {
  // non-constant thresholds (variable refs) fall through to
  // compareChildrenInOrder
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  ASSERT_LT(a->id, b->id);
  auto* alA =
      _ast->createNodeQuantifier(Quantifier::Type::kAtLeast, createRefNode(a));
  auto* alB =
      _ast->createNodeQuantifier(Quantifier::Type::kAtLeast, createRefNode(b));
  EXPECT_LT(compare(alA, alB), 0);
  EXPECT_GT(compare(alB, alA), 0);
}

// --- compareUtf8 flag propagation
// -------------------------------------------------

TEST_F(CompareAstNodesTest, stringCompareUtf8FlagPropagates) {
  EXPECT_EQ(0, compare(strVal("abc"), strVal("abc"), /*utf8=*/true));
  EXPECT_LT(compare(strVal("abc"), strVal("abd"), /*utf8=*/true), 0);
  EXPECT_GT(compare(strVal("abd"), strVal("abc"), /*utf8=*/true), 0);
}

// NFC U+00E9 (0xC3 0xA9) vs NFD e + combining accent (0x65 0xCC 0x81):
// byte comparison → not equal; ICU comparison → equal
TEST_F(CompareAstNodesTest, nfcAndNfdDistinctWithoutUtf8) {
  auto* nfc = strVal("caf\xc3\xa9");
  auto* nfd = strVal("cafe\xcc\x81");
  EXPECT_NE(0, compare(nfc, nfd, /*utf8=*/false));
  EXPECT_EQ(0, compare(nfc, nfd, /*utf8=*/true));
}

TEST_F(CompareAstNodesTest, naryAndWithNfcNfdStringsUsesByteCompare) {
  auto* nfc = strVal("caf\xc3\xa9");
  auto* nfd = strVal("cafe\xcc\x81");
  auto* andNfc = naryOp(NODE_TYPE_OPERATOR_NARY_AND, {nfc});
  auto* andNfd = naryOp(NODE_TYPE_OPERATOR_NARY_AND, {nfd});
  EXPECT_NE(0, compare(andNfc, andNfd, /*utf8=*/false));
  EXPECT_EQ(0, compare(andNfc, andNfd, /*utf8=*/true));
}

// --- IN / NIN with non-constant (structural) elements
// -------------------------

TEST_F(CompareAstNodesTest, inNonConstantElementsOrderIndependent) {
  auto* x = makeVar("x");
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  ASSERT_LT(a->id, b->id);
  auto* inAB =
      createInOp(createRefNode(x), {createRefNode(a), createRefNode(b)});
  auto* inBA =
      createInOp(createRefNode(x), {createRefNode(b), createRefNode(a)});
  EXPECT_EQ(0, compare(inAB, inBA));
}

TEST_F(CompareAstNodesTest, ninNonConstantElementsOrderIndependent) {
  auto* x = makeVar("x");
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ninAB =
      createNinOp(createRefNode(x), {createRefNode(a), createRefNode(b)});
  auto* ninBA =
      createNinOp(createRefNode(x), {createRefNode(b), createRefNode(a)});
  EXPECT_EQ(0, compare(ninAB, ninBA));
}

// test string and custom types, as both have rank 3 in valueTypeOrder, and we
// need to make sure they're distinguished to produce the correct comparison
// result.
TEST_F(CompareAstNodesTest, inStringElementsSortBeforeStructural) {
  auto* x = makeVar("x");
  auto* p = _ast->createNodeParameter("p");
  auto* in1 = createInOp(createRefNode(x), {strVal("abc"), p});
  auto* in2 = createInOp(createRefNode(x), {p, strVal("abc")});
  EXPECT_EQ(0, compare(in1, in2));
}

TEST_F(CompareAstNodesTest, inLiteralArrayVsBindParameterNotEqual) {
  // When one side is a literal array and the other is a bind parameter, they
  // have different RHS node types and must not compare as equal.
  auto* x = makeVar("x");
  auto* p = _ast->createNodeParameter("arr");
  auto* inLiteral = createInOp(createRefNode(x), {intVal(1), intVal(2)});
  auto* inBind =
      createBinaryOp(NODE_TYPE_OPERATOR_BINARY_IN, createRefNode(x), p);
  EXPECT_NE(0, compare(inLiteral, inBind));
}

// --- SUBQUERY nodes
// -----------------------------------------------------------

// Two distinct subquery nodes are not equal even with the same structure;
// equality is pointer identity only.
TEST_F(CompareAstNodesTest, subqueryDistinctNodesNotEqual) {
  auto* s1 = _ast->createNodeSubquery();
  auto* s2 = _ast->createNodeSubquery();
  EXPECT_NE(s1, s2);
  EXPECT_NE(0, compare(s1, s2));
}

TEST_F(CompareAstNodesTest, subquerySamePointerEqual) {
  auto* s = _ast->createNodeSubquery();
  EXPECT_EQ(0, compare(s, s));
}

// --- structural nodes of different AstNodeType
// --------------------------------

TEST_F(CompareAstNodesTest, differentStructuralTypesOrderedByEnum) {
  static_assert(NODE_TYPE_ATTRIBUTE_ACCESS < NODE_TYPE_REFERENCE,
                "test relies on ATTRIBUTE_ACCESS sorting before REFERENCE");
  auto* x = makeVar("x");
  AstNode* refNode = createRefNode(x);
  AstNode* attrNode = attr(createRefNode(x), "f");
  EXPECT_LT(compare(attrNode, refNode), 0);
  EXPECT_GT(compare(refNode, attrNode), 0);
}

}  // namespace
