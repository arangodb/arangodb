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
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Logger/LogMacros.h"
#include "Mocks/Servers.h"

#include "gtest/gtest.h"

#include <functional>

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
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(0)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(0)->getMember(0)->type);
    EXPECT_EQ("foo", root->getMember(0)->getStringView());
    EXPECT_EQ(1, root->getMember(0)->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(1)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(1)->getMember(0)->type);
    EXPECT_EQ("bar", root->getMember(1)->getStringView());
    EXPECT_EQ(2, root->getMember(1)->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(2)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->getMember(0)->type);
    EXPECT_EQ("foo", root->getMember(2)->getMember(0)->getStringView());
    EXPECT_EQ("baz", root->getMember(2)->getStringView());
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
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(0)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(0)->getMember(0)->type);
    EXPECT_EQ("foo", root->getMember(0)->getStringView());
    EXPECT_EQ(1, root->getMember(0)->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(1)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(1)->getMember(0)->type);
    EXPECT_EQ("bar", root->getMember(1)->getStringView());
    EXPECT_EQ(2, root->getMember(1)->getMember(0)->getIntValue());
    EXPECT_EQ(NODE_TYPE_OBJECT_ELEMENT, root->getMember(2)->type);
    EXPECT_EQ(NODE_TYPE_VALUE, root->getMember(2)->getMember(0)->type);
    EXPECT_EQ("foo", root->getMember(2)->getMember(0)->getStringView());
    EXPECT_EQ("baz", root->getMember(2)->getStringView());
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

  AstNode* binaryOp(AstNodeType t, AstNode* lhs, AstNode* rhs) {
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
  AstNode* inOp(AstNode* lhs, std::initializer_list<AstNode*> elems) {
    AstNode* arr = _ast->createNodeArray(elems.size());
    for (auto* e : elems) {
      arr->addMember(e);
    }
    return binaryOp(NODE_TYPE_OPERATOR_BINARY_IN, lhs, arr);
  }

  AstNode* ninOp(AstNode* lhs, std::initializer_list<AstNode*> elems) {
    AstNode* arr = _ast->createNodeArray(elems.size());
    for (auto* e : elems) {
      arr->addMember(e);
    }
    return binaryOp(NODE_TYPE_OPERATOR_BINARY_NIN, lhs, arr);
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

  // <true> would crash resolving attribute accesses with a variable base.
  int compare(AstNode const* lhs, AstNode const* rhs, bool utf8 = false) {
    return compareAstNodes<false>(lhs, rhs, utf8);
  }
};

// --- constant value comparisons
// -----------------------------------------------

TEST_F(CompareAstNodesTest, constantInts) {
  EXPECT_EQ(0, compare(intVal(0), intVal(0)));
  EXPECT_EQ(-1, compare(intVal(1), intVal(2)));
  EXPECT_EQ(1, compare(intVal(2), intVal(1)));
}

TEST_F(CompareAstNodesTest, constantStrings) {
  EXPECT_EQ(0, compare(strVal("foo"), strVal("foo")));
  EXPECT_EQ(-1, compare(strVal("bar"), strVal("foo")));
  EXPECT_EQ(1, compare(strVal("foo"), strVal("bar")));
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
  // Variables are assigned monotonically increasing ids; the one created first
  // has the lower id and must compare as less.
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
  // "age" < "name" lexicographically
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

// --- commutative binary operators (EQ / NE)
// -----------------------------------

TEST_F(CompareAstNodesTest, equalityCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  // `a == b` and `b == a` must compare as equal.
  auto* ab = binaryOp(NODE_TYPE_OPERATOR_BINARY_EQ, createRefNode(a),
                      createRefNode(b));
  auto* ba = binaryOp(NODE_TYPE_OPERATOR_BINARY_EQ, createRefNode(b),
                      createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, inequalityCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = binaryOp(NODE_TYPE_OPERATOR_BINARY_NE, createRefNode(a),
                      createRefNode(b));
  auto* ba = binaryOp(NODE_TYPE_OPERATOR_BINARY_NE, createRefNode(b),
                      createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

// LT is not commutative: `a < b` ≠ `b < a` (unless a == b, which can't happen
// for two distinct variables).
TEST_F(CompareAstNodesTest, lessThanNotCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = binaryOp(NODE_TYPE_OPERATOR_BINARY_LT, createRefNode(a),
                      createRefNode(b));
  auto* ba = binaryOp(NODE_TYPE_OPERATOR_BINARY_LT, createRefNode(b),
                      createRefNode(a));
  EXPECT_NE(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, additionCommutative) {
  // `1 + 2` and `2 + 1` must compare as equal.
  auto* plus12 = binaryOp(NODE_TYPE_OPERATOR_BINARY_PLUS, intVal(1), intVal(2));
  auto* plus21 = binaryOp(NODE_TYPE_OPERATOR_BINARY_PLUS, intVal(2), intVal(1));
  EXPECT_EQ(0, compare(plus12, plus21));
}

TEST_F(CompareAstNodesTest, binaryAndCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = binaryOp(NODE_TYPE_OPERATOR_BINARY_AND, createRefNode(a),
                      createRefNode(b));
  auto* ba = binaryOp(NODE_TYPE_OPERATOR_BINARY_AND, createRefNode(b),
                      createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

// --- FCALL nodes
// --------------------------------------------------------------

TEST_F(CompareAstNodesTest, fcallSameFunction) {
  auto* x = makeVar("x");
  EXPECT_EQ(0, compare(fcall("LENGTH", {createRefNode(x)}),
                       fcall("LENGTH", {createRefNode(x)})));
}

// Different functions with the same arg must not compare as equal.
TEST_F(CompareAstNodesTest, fcallOrderedByName) {
  auto* x = makeVar("x");
  // "LENGTH" < "UPPER"
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

// --- NARY operators
// -----------------------------------------------------------

TEST_F(CompareAstNodesTest, naryAndOrderIndependent) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* c = makeVar("c");
  // (a AND b AND c) compared with (c AND a AND b) must be 0.
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

TEST_F(CompareAstNodesTest, naryDifferentCountsNotEqual) {
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

// --- IN / NIN array element order independence
// ---------------------------------

TEST_F(CompareAstNodesTest, inArrayElementOrderIndependent) {
  auto* x = makeVar("x");
  // `x IN [1, 2]` vs `x IN [2, 1]` — must compare as equal.
  auto* in12 = inOp(createRefNode(x), {intVal(1), intVal(2)});
  auto* in21 = inOp(createRefNode(x), {intVal(2), intVal(1)});
  EXPECT_EQ(0, compare(in12, in21));
}

TEST_F(CompareAstNodesTest, ninArrayElementOrderIndependent) {
  auto* x = makeVar("x");
  auto* nin12 = ninOp(createRefNode(x), {intVal(1), intVal(2)});
  auto* nin21 = ninOp(createRefNode(x), {intVal(2), intVal(1)});
  EXPECT_EQ(0, compare(nin12, nin21));
}

TEST_F(CompareAstNodesTest, inDifferentOperandsNotEqual) {
  auto* x = makeVar("x");
  auto* y = makeVar("y");
  auto* inX = inOp(createRefNode(x), {intVal(1)});
  auto* inY = inOp(createRefNode(y), {intVal(1)});
  EXPECT_NE(0, compare(inX, inY));
}

TEST_F(CompareAstNodesTest, inDifferentArraySizeNotEqual) {
  auto* x = makeVar("x");
  auto* in1 = inOp(createRefNode(x), {intVal(1)});
  auto* in12 = inOp(createRefNode(x), {intVal(1), intVal(2)});
  EXPECT_NE(0, compare(in1, in12));
}

// BINARY_OR and TIMES are also in the commutative list — verify they're
// covered.
TEST_F(CompareAstNodesTest, binaryOrCommutative) {
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ab = binaryOp(NODE_TYPE_OPERATOR_BINARY_OR, createRefNode(a),
                      createRefNode(b));
  auto* ba = binaryOp(NODE_TYPE_OPERATOR_BINARY_OR, createRefNode(b),
                      createRefNode(a));
  EXPECT_EQ(0, compare(ab, ba));
}

TEST_F(CompareAstNodesTest, multiplicationCommutative) {
  // `1 * 2` and `2 * 1` must compare as equal.
  auto* m12 = binaryOp(NODE_TYPE_OPERATOR_BINARY_TIMES, intVal(1), intVal(2));
  auto* m21 = binaryOp(NODE_TYPE_OPERATOR_BINARY_TIMES, intVal(2), intVal(1));
  EXPECT_EQ(0, compare(m12, m21));
}

// Nested attribute access: doc.x.y vs doc.x.y
TEST_F(CompareAstNodesTest, nestedAttributeAccessEqual) {
  auto* x = makeVar("x");
  auto* lhs = attr(attr(createRefNode(x), "a"), "b");
  auto* rhs = attr(attr(createRefNode(x), "a"), "b");
  EXPECT_EQ(0, compare(lhs, rhs));
}

TEST_F(CompareAstNodesTest, nestedAttributeAccessDifferentLeaf) {
  auto* x = makeVar("x");
  // doc.a.b vs doc.a.c — differ at the outer attribute name.
  EXPECT_NE(0, compare(attr(attr(createRefNode(x), "a"), "b"),
                       attr(attr(createRefNode(x), "a"), "c")));
}

// FCALL with same name but different argument count must not compare as equal.
TEST_F(CompareAstNodesTest, fcallDifferentArgumentCount) {
  auto* x = makeVar("x");
  auto* y = makeVar("y");
  EXPECT_NE(0, compare(fcall("CONCAT", {createRefNode(x)}),
                       fcall("CONCAT", {createRefNode(x), createRefNode(y)})));
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

// --- compareUtf8 flag propagation
// -------------------------------------------------

TEST_F(CompareAstNodesTest, stringCompareUtf8FlagPropagates) {
  EXPECT_EQ(0, compare(strVal("abc"), strVal("abc"), /*utf8=*/true));
  EXPECT_LT(compare(strVal("abc"), strVal("abd"), /*utf8=*/true), 0);
  EXPECT_GT(compare(strVal("abd"), strVal("abc"), /*utf8=*/true), 0);
}

// --- IN / NIN with non-constant (structural) elements
// -------------------------

TEST_F(CompareAstNodesTest, inNonConstantElementsOrderIndependent) {
  auto* x = makeVar("x");
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  ASSERT_LT(a->id, b->id);
  auto* inAB = inOp(createRefNode(x), {createRefNode(a), createRefNode(b)});
  auto* inBA = inOp(createRefNode(x), {createRefNode(b), createRefNode(a)});
  EXPECT_EQ(0, compare(inAB, inBA));
}

TEST_F(CompareAstNodesTest, ninNonConstantElementsOrderIndependent) {
  auto* x = makeVar("x");
  auto* a = makeVar("a");
  auto* b = makeVar("b");
  auto* ninAB = ninOp(createRefNode(x), {createRefNode(a), createRefNode(b)});
  auto* ninBA = ninOp(createRefNode(x), {createRefNode(b), createRefNode(a)});
  EXPECT_EQ(0, compare(ninAB, ninBA));
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
