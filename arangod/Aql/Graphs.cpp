////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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


#include "Graphs.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/OptimizerUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"
#include "Graph/Providers/BaseProviderOptions.h"

#include <velocypack/Iterator.h>

using namespace arangodb::basics;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
void calculateMemberToUpdate(std::string const& memberString,
                             std::optional<size_t>& memberToUpdate,
                             Variable const* tmpVariable,
                             AstNode* indexCondition) {
  std::pair<Variable const*, std::vector<AttributeName>>
      pathCmp;
  for (size_t x = 0; x < indexCondition->numMembers(); ++x) {
    // We search through the nary-and and look for EQ - _from/_to
    auto eq = indexCondition->getMemberUnchecked(x);
    if (eq->type != AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // No equality. Skip
      continue;
    }
    TRI_ASSERT(eq->numMembers() == 2);
    // It is sufficient to only check member one.
    // We build the condition this way.
    auto mem = eq->getMemberUnchecked(0);
    if (mem->isAttributeAccessForVariable(pathCmp, true)) {
      if (pathCmp.first != tmpVariable) {
        continue;
      }
      if (pathCmp.second.size() == 1 &&
          pathCmp.second[0].name == memberString) {
        memberToUpdate = x;
        break;
      }
      continue;
    }
  }
}

auto generateExpression(ExecutionPlan const* plan, Variable const* variable,
                        AstNode* remainderCondition,
                        AstNode* indexCondition)
    -> std::unique_ptr<Expression> {
  ::arangodb::containers::HashSet<size_t> toRemove;
  auto ast = plan->getAst();
  Condition::collectOverlappingMembers(plan, variable, remainderCondition,
                                            indexCondition, toRemove, nullptr,
                                            false);
  size_t n = remainderCondition->numMembers();

  if (n != toRemove.size()) {
    // Slow path need to explicitly remove nodes.
    for (; n > 0; --n) {
      // Now n is one more than the idx we actually check
      if (toRemove.find(n - 1) != toRemove.end()) {
        // This index has to be removed.
        remainderCondition->removeMemberUnchecked(n - 1);
      }
    }
    return std::make_unique<Expression>(ast, remainderCondition);
  }
  return nullptr;
}
}  // namespace

/// @brief Prepares an edge condition builder
/// this builder just contains the basic
/// from and to conditions.
EdgeConditionBuilder::EdgeConditionBuilder(Ast* ast, Variable const* variable, AstNode const* exchangeableIdNode)
    : _fromCondition(nullptr),
      _toCondition(nullptr),
      _modCondition(ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND)),
      _containsCondition(false) {
  // Prepare _from and _to conditions
  auto ref = ast->createNodeReference(variable);
  {
    auto const* access = ast->createNodeAttributeAccess(
        ref, StaticStrings::FromString);
    _fromCondition = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                   access, exchangeableIdNode);
  }
  {
    auto const* access = ast->createNodeAttributeAccess(
        ref, StaticStrings::ToString);
    _toCondition = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                   access, exchangeableIdNode);
  }
}

EdgeConditionBuilder::EdgeConditionBuilder(Ast* ast,
                                           EdgeConditionBuilder const& other)
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
  if (other._fromCondition != nullptr) {
    _fromCondition = other._fromCondition->clone(ast);
  }
  if (other._toCondition != nullptr) {
    _toCondition = other._toCondition->clone(ast);
  }
}

EdgeConditionBuilder::EdgeConditionBuilder(Ast* ast, Variable const* variable,
                                           AstNode const* exchangeableIdNode,
                                           VPackSlice slice)
    : EdgeConditionBuilder(ast, variable, exchangeableIdNode) {
  // Now fill in the information from slice
  auto list = slice.get("globalEdgeConditions");
  if (list.isArray()) {
    // Inject all globalEdgeConditions
    for (auto const& cond : VPackArrayIterator(list)) {
      _modCondition->addMember(ast->createNode(cond));
    }
  }

  list = slice.get("edgeConditions");
  if (list.isObject()) {
    for (auto const& cond : VPackObjectIterator(list)) {
      auto depth = StringUtils::uint64(cond.key.stringView());
      auto& conds = _depthConditions[depth];
      // NOTE: We "leak" the n_ary_and node here, it will be part
      // of the AST and will be cleaned up at the end of the query, but
      // will never be used again.
      // All it's members are reused.
      auto conditionsAndNode = ast->createNode(cond.value);
      for (size_t i = 0; i < conditionsAndNode->numMembers(); ++i) {
        conds.emplace_back(conditionsAndNode->getMemberUnchecked(i));
      }
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
  _modCondition->addMember(part);
}

// Add a condition on the edges that specific to the given depth
void EdgeConditionBuilder::addConditionForDepth(AstNode const* condition, uint64_t depth) {
  // This API on purpose takes all Conditions, the Optimizer has to know what can be optimized.
  _depthConditions[depth].emplace_back(condition);
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
    auto toChange = _modCondition->numMembers() - 1;
    auto changeNode =
        _modCondition->getMemberUnchecked(toChange);
    TRI_ASSERT(changeNode == _fromCondition || changeNode == _toCondition);
#endif
    _modCondition->changeMember(toChange, cond);
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

// Get the complete condition for outbound edges
// Note: Caller will get a clone, and is allowed to modify it
AstNode* EdgeConditionBuilder::getOutboundCondition(Ast* ast) const {
  TRI_ASSERT(_fromCondition != nullptr);
  auto baseCondition = _modCondition->clone(ast);
  // This is a bit confusing, if someone used the non-const API
  // the _modCondition contains _from or _to as last member, so we need to drop it first
  // otherwise we can just append _from or _to
  if (_containsCondition) {
    TRI_ASSERT(baseCondition->numMembers() > 0);
    baseCondition->removeMemberUnchecked(baseCondition->numMembers() - 1);
  }
  baseCondition->addMember(_fromCondition->clone(ast));
  return baseCondition;
}

// Get the complete condition for inbound edges
// Note: Caller will get a clone, and is allowed to modify it
AstNode* EdgeConditionBuilder::getInboundCondition(Ast* ast) const {
  TRI_ASSERT(_toCondition != nullptr);
  auto baseCondition = _modCondition->clone(ast);
  // This is a bit confusing, if someone used the non-const API
  // the _modCondition contains _from or _to as last member, so we need to drop it first
  // otherwise we can just append _from or _to
  if (_containsCondition) {
    TRI_ASSERT(baseCondition->numMembers() > 0);
    baseCondition->removeMemberUnchecked(baseCondition->numMembers() - 1);
  }
  baseCondition->addMember(_toCondition->clone(ast));
  return baseCondition;
}


// Get the complete condition for outbound edges
AstNode* EdgeConditionBuilder::getOutboundConditionForDepth(uint64_t depth, Ast* ast) const {
  auto cond = getOutboundCondition(ast);
  if (_depthConditions.contains(depth)) {
    for (auto const* subCondition : _depthConditions.at(depth)) {
      cond->addMember(subCondition->clone(ast));
    }
  }
  return cond;
}

// Get the complete condition for inbound edges
AstNode* EdgeConditionBuilder::getInboundConditionForDepth(uint64_t depth, Ast* ast) const {
  auto cond = getInboundCondition(ast);
  if (_depthConditions.contains(depth)) {
    for (auto const* subCondition : _depthConditions.at(depth)) {
      cond->addMember(subCondition->clone(ast));
    }
  }
  return cond;
}

std::pair<
    std::vector<IndexAccessor>,
    std::unordered_map<uint64_t, std::vector<IndexAccessor>>>
EdgeConditionBuilder::buildIndexAccessors(
    ExecutionPlan const* plan, Variable const* tmpVar,
    std::unordered_map<VariableId, VarInfo> const& varInfo,
    std::vector<std::pair<Collection*, TRI_edge_direction_e>> const&
        collections) const {
  auto ast = plan->getAst();
  std::vector<IndexAccessor> indexAccessors{};
  constexpr bool onlyEdgeIndexes = false;
  // arbitrary value for "number of edges in collection" used here. the
  // actual value does not matter much. 1000 has historically worked fine.
  constexpr size_t itemsInCollection = 1000;

  for (size_t i = 0; i < collections.size(); ++i) {
    auto& [edgeCollection, dir] = collections[i];
    TRI_ASSERT(dir == TRI_EDGE_IN || dir == TRI_EDGE_OUT);

    // TODO Check if we can reduce one clone by now.
    AstNode* condition =
        (dir == TRI_EDGE_IN) ? getInboundCondition(ast) : getOutboundCondition(ast);
    AstNode* indexCondition = condition->clone(ast);
    std::shared_ptr<Index> indexToUse;

    bool res = utils::getBestIndexHandleForFilterCondition(
        *edgeCollection, indexCondition, tmpVar, itemsInCollection,
        IndexHint(), indexToUse, onlyEdgeIndexes);
    if (!res) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "expected edge index not found");
    }

    std::optional<size_t> memberToUpdate{std::nullopt};
    ::calculateMemberToUpdate(dir == TRI_EDGE_IN ? StaticStrings::ToString
                                                 : StaticStrings::FromString,
                              memberToUpdate, tmpVar, indexCondition);

    AstNode* remainderCondition = condition->clone(ast);
    std::unique_ptr<Expression> expression =
        ::generateExpression(plan, tmpVar, remainderCondition, indexCondition);

    auto container = utils::extractNonConstPartsOfIndexCondition(
        ast, varInfo, false, false, indexCondition, tmpVar);
    indexAccessors.emplace_back(std::move(indexToUse), indexCondition,
                                memberToUpdate, std::move(expression),
                                std::move(container), i, dir);
  }

  std::unordered_map<uint64_t,
                     std::vector<IndexAccessor>> depthIndexAccessors{};
  for (auto const& [depth, conditions] : _depthConditions) {
    std::vector<IndexAccessor> localIndexAccessors{};
    for (size_t i = 0; i < collections.size(); ++i) {
      auto& [edgeCollection, dir] = collections[i];
      TRI_ASSERT(dir == TRI_EDGE_IN || dir == TRI_EDGE_OUT);

      AstNode* condition = (dir == TRI_EDGE_IN)
                               ? getInboundConditionForDepth(depth, ast)
                               : getOutboundConditionForDepth(depth, ast);
      AstNode* indexCondition = condition->clone(ast);
      std::shared_ptr<Index> indexToUse;

      bool res = utils::getBestIndexHandleForFilterCondition(
          *edgeCollection, indexCondition, tmpVar, itemsInCollection,
          IndexHint(), indexToUse, onlyEdgeIndexes);
      if (!res) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "expected edge index not found");
      }

      std::optional<size_t> memberToUpdate{std::nullopt};
      ::calculateMemberToUpdate(dir == TRI_EDGE_IN ? StaticStrings::ToString
                                                   : StaticStrings::FromString,
                                memberToUpdate, tmpVar, indexCondition);

      AstNode* remainderCondition = condition->clone(ast);
      std::unique_ptr<Expression> expression =
          ::generateExpression(plan, tmpVar, remainderCondition, indexCondition);

      auto container = utils::extractNonConstPartsOfIndexCondition(
          ast, varInfo, false, false, indexCondition, tmpVar);
      localIndexAccessors.emplace_back(std::move(indexToUse), indexCondition,
                                  memberToUpdate, std::move(expression),
                                  std::move(container), i, dir);
    }
    depthIndexAccessors.emplace(depth, std::move(localIndexAccessors));
  }

  return std::make_pair<
      std::vector<IndexAccessor>,
      std::unordered_map<uint64_t,
                         std::vector<IndexAccessor>>>(
      std::move(indexAccessors), std::move(depthIndexAccessors));
}

void EdgeConditionBuilder::toVelocyPackCompat_39(arangodb::velocypack::Builder& builder, unsigned flags) const {
  TRI_ASSERT(builder.isOpenObject());
  auto globalMembers = _modCondition->numMembers();
  if (_containsCondition) {
    TRI_ASSERT(globalMembers > 0);
    // Last member is _from/_to and shall not be produced!
    globalMembers -= 1;
  }
  if (globalMembers > 0) {
    builder.add(VPackValue("globalEdgeConditions"));
    VPackArrayBuilder guard(&builder);
    for (size_t i = 0; i < globalMembers; ++i) {
      _modCondition->getMemberUnchecked(i)->toVelocyPack(builder, flags);
    }
  }
  if (!_depthConditions.empty()) {
    builder.add(VPackValue("edgeConditions"));
    VPackObjectBuilder guard(&builder);
    for (auto& it : _depthConditions) {
      builder.add(VPackValue(basics::StringUtils::itoa(it.first)));
      // Temporary link everything to one AND node:
      // This node shall go out of scope and free it's memory
      AstNode andCond(AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
      for (auto const& cond : it.second) {
        // On purpose do not clone anything here.
        // We are not going to modify anything and the andCond will go out of scope
        andCond.addMember(cond);
      }
      andCond.toVelocyPack(builder, flags);
    }
  }

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
