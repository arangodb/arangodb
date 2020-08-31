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
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/Iterator.h>

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"
#include "Graphs.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

EdgeConditionBuilder::EdgeConditionBuilder(Ast* ast, EdgeConditionBuilder const& other)
    : _fromCondition(nullptr),
      _toCondition(nullptr),
      _modCondition(nullptr),
      _containsCondition(false) {
  if (other._modCondition != nullptr) {
    TRI_ASSERT(other._modCondition->type == NODE_TYPE_OPERATOR_NARY_AND);

    _modCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    TRI_ASSERT(_modCondition != nullptr);

    size_t n = other._modCondition->numMembers();
    if (other._containsCondition) {
      TRI_ASSERT(n > 0);
      n = n - 1;
    }
    _modCondition->reserve(n);
    for (size_t i = 0; i < n; ++i) {
      _modCondition->addMember(other._modCondition->getMember(i));
    }
  }
}

EdgeConditionBuilder::EdgeConditionBuilder(AstNode* modCondition)
    : _fromCondition(nullptr),
      _toCondition(nullptr),
      _modCondition(modCondition),
      _containsCondition(false) {
  if (_modCondition != nullptr) {
    TRI_ASSERT(_modCondition->type == NODE_TYPE_OPERATOR_NARY_AND);
  }
}

void EdgeConditionBuilder::addConditionPart(AstNode const* part) {
  TRI_ASSERT(_modCondition != nullptr);
  TRI_ASSERT(_modCondition->type == NODE_TYPE_OPERATOR_NARY_AND);
  TRI_ASSERT(!_containsCondition);
  // The ordering is only maintained before we request a specific
  // condition
  _modCondition->addMember(part);
}

void EdgeConditionBuilder::swapSides(AstNode* cond) {
  TRI_ASSERT(cond != nullptr);
  TRI_ASSERT(cond == _fromCondition || cond == _toCondition);
  TRI_ASSERT(cond->type == NODE_TYPE_OPERATOR_BINARY_EQ);
  if (_containsCondition) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // If used correctly this class guarantuees that the last element
    // of the nary-and is the _from or _to part and is exchangable.
    TRI_ASSERT(_modCondition->numMembers() > 0);
    auto changeNode = _modCondition->getMemberUnchecked(_modCondition->numMembers() - 1);
    TRI_ASSERT(changeNode == _fromCondition || changeNode == _toCondition);
#endif
    _modCondition->changeMember(_modCondition->numMembers() - 1, cond);
  } else {
    _modCondition->addMember(cond);
    _containsCondition = true;
  }
  TRI_ASSERT(_modCondition->numMembers() > 0);
}

AstNode* EdgeConditionBuilder::getOutboundCondition() {
  if (_fromCondition == nullptr) {
    buildFromCondition();
  }
  TRI_ASSERT(_fromCondition != nullptr);
  swapSides(_fromCondition);
  return _modCondition;
}

AstNode* EdgeConditionBuilder::getInboundCondition() {
  if (_toCondition == nullptr) {
    buildToCondition();
  }
  TRI_ASSERT(_toCondition != nullptr);
  swapSides(_toCondition);
  return _modCondition;
}

EdgeConditionBuilderContainer::EdgeConditionBuilderContainer()
    : EdgeConditionBuilder(nullptr) {
  auto node = std::make_unique<AstNode>(NODE_TYPE_OPERATOR_NARY_AND);
  _astNodes.emplace_back(node.get());
  _modCondition = node.release();

  auto comp = std::make_unique<AstNode>(NODE_TYPE_VALUE);
  comp->setValueType(VALUE_TYPE_STRING);
  comp->setStringValue("", 0);
  _astNodes.emplace_back(comp.get());
  _compareNode = comp.release();

  _var = _varGen.createTemporaryVariable();

  auto varNode = std::make_unique<AstNode>(NODE_TYPE_REFERENCE);
  varNode->setData(_var);
  _astNodes.emplace_back(varNode.get());
  _varNode = varNode.release();
}

EdgeConditionBuilderContainer::~EdgeConditionBuilderContainer() {
  // we have to clean up the AstNodes
  for (auto it : _astNodes) {
    delete it;
  }
  _astNodes.clear();
}

AstNode* EdgeConditionBuilderContainer::createEqCheck(AstNode const* access) {
  auto node = std::make_unique<AstNode>(NODE_TYPE_OPERATOR_BINARY_EQ);
  node->reserve(2);
  node->addMember(access);
  node->addMember(_compareNode);
  _astNodes.emplace_back(node.get());
  return node.release();
}

AstNode* EdgeConditionBuilderContainer::createAttributeAccess(std::string const& attr) {
  auto node = std::make_unique<AstNode>(NODE_TYPE_ATTRIBUTE_ACCESS);
  node->addMember(_varNode);
  node->setStringValue(attr.c_str(), attr.length());
  _astNodes.emplace_back(node.get());
  return node.release();
}

void EdgeConditionBuilderContainer::buildFromCondition() {
  TRI_ASSERT(_fromCondition == nullptr);
  auto access = createAttributeAccess(StaticStrings::FromString);
  _fromCondition = createEqCheck(access);
}

void EdgeConditionBuilderContainer::buildToCondition() {
  TRI_ASSERT(_toCondition == nullptr);
  auto access = createAttributeAccess(StaticStrings::ToString);
  _toCondition = createEqCheck(access);
}

Variable const* EdgeConditionBuilderContainer::getVariable() const {
  return _var;
}

void EdgeConditionBuilderContainer::setVertexId(std::string const& id) {
  _compareNode->setStringValue(id.c_str(), id.length());
}
