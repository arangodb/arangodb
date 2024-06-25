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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "EdgeConditionBuilder.h"

#include <velocypack/Iterator.h>

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

EdgeConditionBuilder::EdgeConditionBuilder(Ast* ast,
                                           EdgeConditionBuilder const& other)
    : _fromCondition(nullptr),
      _toCondition(nullptr),
      _modCondition(nullptr),
      _containsCondition(false),
      _resourceMonitor(ast->query().resourceMonitor()) {
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

EdgeConditionBuilder::EdgeConditionBuilder(
    AstNode* modCondition, arangodb::ResourceMonitor& resourceMonitor)
    : _fromCondition(nullptr),
      _toCondition(nullptr),
      _modCondition(modCondition),
      _containsCondition(false),
      _resourceMonitor(resourceMonitor) {
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
    auto changeNode =
        _modCondition->getMemberUnchecked(_modCondition->numMembers() - 1);
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

arangodb::ResourceMonitor& EdgeConditionBuilder::resourceMonitor() {
  return _resourceMonitor;
}

void EdgeConditionBuilder::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  auto replace = [&](AstNode*& condition) {
    if (condition != nullptr) {
      condition = Ast::replaceVariables(condition, replacements, true);
    }
  };

  replace(_fromCondition);
  replace(_toCondition);
  replace(_modCondition);
}

void EdgeConditionBuilder::replaceAttributeAccess(
    Ast* ast, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable) {
  auto replace = [&](AstNode*& condition) {
    if (condition != nullptr) {
      condition = Ast::replaceAttributeAccess(ast, condition, searchVariable,
                                              attribute, replaceVariable);
    }
  };

  replace(_fromCondition);
  replace(_toCondition);
  replace(_modCondition);
}
