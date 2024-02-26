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
#include "EdgeConditionBuilderContainer.h"

#include <velocypack/Iterator.h>

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

EdgeConditionBuilderContainer::EdgeConditionBuilderContainer(
    arangodb::ResourceMonitor& resourceMonitor)
    : EdgeConditionBuilder(nullptr, resourceMonitor), _varGen(resourceMonitor) {
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

AstNode* EdgeConditionBuilderContainer::createAttributeAccess(
    std::string const& attr) {
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
