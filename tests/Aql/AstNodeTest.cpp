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

}  // namespace
