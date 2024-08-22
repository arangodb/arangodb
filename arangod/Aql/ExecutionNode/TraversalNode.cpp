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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "TraversalNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/GraphNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/TraversalExecutor.h"
#include "Aql/Expression.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Variable.h"
#include "Basics/StringUtils.h"
#include "Basics/tryEmplaceHelper.h"
#include "Cluster/ServerState.h"
#include "Graph/BaseOptions.h"
#include "Graph/Graph.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/TraverserOptions.h"
#include "Graph/Types/UniquenessLevel.h"
#include "Graph/algorithm-aliases.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ticks.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Iterator.h>

#include <memory>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::graph;
using namespace arangodb::traverser;

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn)
    : EdgeConditionBuilder(tn->_plan->getAst()->createNodeNaryOperator(
                               NODE_TYPE_OPERATOR_NARY_AND),
                           tn->_plan->getAst()->query().resourceMonitor()),
      _tn(tn) {}

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn, arangodb::velocypack::Slice condition)
    : EdgeConditionBuilder(tn->_plan->getAst()->createNode(condition),
                           tn->_plan->getAst()->query().resourceMonitor()),
      _tn(tn) {}

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn, TraversalEdgeConditionBuilder const* other)
    : EdgeConditionBuilder(tn->_plan->getAst(), *other), _tn(tn) {
  _fromCondition = other->_fromCondition;
  _toCondition = other->_toCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::buildFromCondition() {
  // TODO Move computation in here.
  _fromCondition = _tn->_fromCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::buildToCondition() {
  // TODO Move computation in here.
  _toCondition = _tn->_toCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::toVelocyPack(
    VPackBuilder& builder, bool verbose) {
  if (_containsCondition) {
    _modCondition->removeMemberUnchecked(_modCondition->numMembers() - 1);
    _containsCondition = false;
  }
  _modCondition->toVelocyPack(builder, verbose);
}

TraversalNode::TraversalNode(ExecutionPlan* plan, ExecutionNodeId id,
                             TRI_vocbase_t* vocbase, AstNode const* direction,
                             AstNode const* start, AstNode const* graph,
                             std::unique_ptr<Expression> pruneExpression,
                             std::unique_ptr<BaseOptions> options)
    : GraphNode(plan, id, vocbase, direction, graph, std::move(options)),
      _pathOutVariable(nullptr),
      _inVariable(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr),
      _pruneExpression(std::move(pruneExpression)) {
  TRI_ASSERT(start != nullptr);

  auto ast = _plan->getAst();
  // Let us build the conditions on _from and _to. Just in case we need them.
  {
    auto const* access = ast->createNodeAttributeAccess(
        _tmpObjVarNode, StaticStrings::FromString);
    _fromCondition = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                   access, _tmpIdNode);
  }
  TRI_ASSERT(_fromCondition != nullptr);
  TRI_ASSERT(_fromCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);

  {
    auto const* access =
        ast->createNodeAttributeAccess(_tmpObjVarNode, StaticStrings::ToString);
    _toCondition = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 access, _tmpIdNode);
  }
  TRI_ASSERT(_toCondition != nullptr);
  TRI_ASSERT(_toCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);

  // Parse start node
  switch (start->type) {
    case NODE_TYPE_REFERENCE:
      _inVariable = static_cast<Variable*>(start->getData());
      _vertexId = "";
      break;
    case NODE_TYPE_VALUE:
      if (start->value.type != VALUE_TYPE_STRING) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                       "invalid start vertex. Must either be "
                                       "an _id string or an object with _id.");
      }
      _inVariable = nullptr;
      _vertexId = start->getString();
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                     "invalid start vertex. Must either be an "
                                     "_id string or an object with _id.");
  }

  if (_pruneExpression) {
    _pruneExpression->variables(_pruneVariables);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif
  validateCollections();
}

/// @brief Internal constructor to clone the node.
TraversalNode::TraversalNode(
    ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
    std::vector<Collection*> const& edgeColls,
    std::vector<Collection*> const& vertexColls, Variable const* inVariable,
    std::string vertexId, TRI_edge_direction_e defaultDirection,
    std::vector<TRI_edge_direction_e> const& directions,
    std::unique_ptr<graph::BaseOptions> options, graph::Graph const* graph)
    : GraphNode(plan, id, vocbase, edgeColls, vertexColls, defaultDirection,
                directions, std::move(options), graph),
      _pathOutVariable(nullptr),
      _inVariable(inVariable),
      _vertexId(std::move(vertexId)),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  validateCollections();
}

TraversalNode::TraversalNode(ExecutionPlan* plan,
                             arangodb::velocypack::Slice base)
    : GraphNode(plan, base),
      _pathOutVariable(nullptr),
      _inVariable(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  // In Vertex
  if (base.hasKey("inVariable")) {
    _inVariable = Variable::varFromVPack(plan->getAst(), base, "inVariable");
  } else {
    VPackSlice v = base.get("vertexId");
    if (!v.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "start vertex must be a string");
    }
    _vertexId = v.copyString();
    if (_vertexId.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "start vertex mustn't be empty");
    }
  }

  if (base.hasKey("condition")) {
    VPackSlice condition = base.get("condition");
    if (!condition.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "condition must be an object");
    }
    _condition = Condition::fromVPack(plan, condition);
  }
  auto list = base.get("conditionVariables");

  if (list.isArray()) {
    for (VPackSlice v : VPackArrayIterator(list)) {
      _conditionVariables.emplace(
          _plan->getAst()->variables()->createVariable(v));
    }
  }

  // Out variables
  if (base.hasKey("pathOutVariable")) {
    _pathOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "pathOutVariable");
  }

  // Filter Condition Parts
  TRI_ASSERT(base.hasKey("fromCondition"));
  _fromCondition = plan->getAst()->createNode(base.get("fromCondition"));

  TRI_ASSERT(base.hasKey("toCondition"));
  _toCondition = plan->getAst()->createNode(base.get("toCondition"));

  list = base.get("globalEdgeConditions");
  if (list.isArray()) {
    for (auto const& cond : VPackArrayIterator(list)) {
      _globalEdgeConditions.emplace_back(plan->getAst()->createNode(cond));
    }
  }

  list = base.get("globalVertexConditions");
  if (list.isArray()) {
    for (auto const& cond : VPackArrayIterator(list)) {
      _globalVertexConditions.emplace_back(plan->getAst()->createNode(cond));
    }
  }

  list = base.get("vertexConditions");
  if (list.isObject()) {
    for (auto const& cond : VPackObjectIterator(list)) {
      std::string key = cond.key.copyString();
      _vertexConditions.try_emplace(StringUtils::uint64(key),
                                    plan->getAst()->createNode(cond.value));
    }
  }

  list = base.get("edgeConditions");
  if (list.isObject()) {
    for (auto const& cond : VPackObjectIterator(list)) {
      std::string key = cond.key.copyString();
      auto ecbuilder =
          std::make_unique<TraversalEdgeConditionBuilder>(this, cond.value);
      _edgeConditions.try_emplace(StringUtils::uint64(key),
                                  std::move(ecbuilder));
    }
  }

  list = base.get("expression");
  if (!list.isNone()) {
    _pruneExpression = std::make_unique<aql::Expression>(plan->getAst(), base);
    TRI_ASSERT(base.hasKey("pruneVariables"));
    list = base.get("pruneVariables");
    TRI_ASSERT(list.isArray());
    for (auto const& varinfo : VPackArrayIterator(list)) {
      _pruneVariables.emplace(
          plan->getAst()->variables()->createVariable(varinfo));
    }
  }

  list = base.get("postFilter");
  if (list.isObject()) {
    TRI_ASSERT(list.hasKey("expression"));
    TRI_ASSERT(list.hasKey("variables"));
    _postFilterExpression =
        std::make_unique<aql::Expression>(plan->getAst(), list);
    list = list.get("variables");
    TRI_ASSERT(list.isArray());
    for (auto const& varinfo : VPackArrayIterator(list)) {
      _postFilterVariables.emplace(
          plan->getAst()->variables()->createVariable(varinfo));
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif
  validateCollections();
}

// This constructor is only used from LocalTraversalNode, and GraphNode
// is virtually inherited; thus its constructor is never called from here.
TraversalNode::TraversalNode(ExecutionPlan& plan, TraversalNode const& other,
                             bool const allowAlreadyBuiltCopy)
    : GraphNode(GraphNode::THIS_THROWS_WHEN_CALLED{}),
      _pathOutVariable(nullptr),
      _inVariable(other._inVariable),
      _vertexId(other._vertexId),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  if (!allowAlreadyBuiltCopy) {
    TRI_ASSERT(!other._optionsBuilt);
  }
  other.traversalCloneHelper(plan, *this);
  validateCollections();
}

TraversalNode::~TraversalNode() = default;

/// @brief return the path out variable
Variable const* TraversalNode::pathOutVariable() const {
  return _pathOutVariable;
}

/// @brief set the path out variable
void TraversalNode::setPathOutput(Variable const* outVar) {
  if (outVar == nullptr) {
    markUnusedConditionVariable(_pathOutVariable);
  }
  _pathOutVariable = outVar;
}

/// @brief return the in variable
Variable const* TraversalNode::inVariable() const { return _inVariable; }

std::string TraversalNode::getStartVertex() const { return _vertexId; }

void TraversalNode::setInVariable(Variable const* inVariable) {
  _inVariable = inVariable;
  _vertexId = "";
}

void TraversalNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  // this is an important assertion: if options are already built,
  // we would need to carry out the replacements in several other
  // places as well
  // we are explicitly not trying to replace inside the _options
  // LookupInfos
  TRI_ASSERT(!_optionsBuilt);

  _inVariable = Variable::replace(_inVariable, replacements);

  if (_pruneExpression != nullptr) {
    // need to replace variables in the prune expression
    VarSet variables;
    _pruneExpression->variables(variables);
    for (auto const& it : variables) {
      if (replacements.contains(it->id)) {
        _pruneExpression->replaceVariables(replacements);

        // and need to recalculate the set of variables used
        // by the prune expression
        variables.clear();
        _pruneExpression->variables(variables);
        _pruneVariables = std::move(variables);
        break;
      }
    }
  }

  if (_condition) {
    _condition->replaceVariables(replacements);
  }

  if (_postFilterExpression != nullptr) {
    _postFilterExpression->replaceVariables(replacements);
  }

  // determine new set of variables used by post filters
  _postFilterVariables.clear();
  for (auto& it : _postFilterConditions) {
    it = Ast::replaceVariables(const_cast<AstNode*>(it), replacements, true);
    // repopulate _postFilterVariables
    Ast::getReferencedVariables(it, _postFilterVariables);
  }

  for (auto& it : _globalEdgeConditions) {
    it = Ast::replaceVariables(const_cast<AstNode*>(it), replacements, true);
  }

  for (auto& it : _globalVertexConditions) {
    it = Ast::replaceVariables(const_cast<AstNode*>(it), replacements, true);
  }

  for (auto& it : _edgeConditions) {
    if (it.second != nullptr) {
      it.second->replaceVariables(replacements);
    }
  }

  for (auto& it : _vertexConditions) {
    it.second = Ast::replaceVariables(it.second, replacements, true);
  }

  if (_fromCondition != nullptr) {
    _fromCondition = Ast::replaceVariables(_fromCondition, replacements, true);
  }

  if (_toCondition != nullptr) {
    _toCondition = Ast::replaceVariables(_toCondition, replacements, true);
  }
}

void TraversalNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t /*index*/) {
  // this is an important assertion: if options are already built,
  // we would need to carry out the replacements in several other
  // places as well
  // we are explicitly not trying to replace inside the _options
  // LookupInfos
  TRI_ASSERT(!_optionsBuilt);

  if (_inVariable != nullptr && searchVariable == _inVariable &&
      attribute.size() == 1 && attribute[0] == StaticStrings::IdString) {
    _inVariable = replaceVariable;
  }

  if (_pruneExpression != nullptr) {
    _pruneExpression->replaceAttributeAccess(searchVariable, attribute,
                                             replaceVariable);
    // determine new set of variables used by prune expression
    VarSet variables;
    _pruneExpression->variables(variables);
    _pruneVariables = std::move(variables);
  }

  if (_condition && self != this) {
    _condition->replaceAttributeAccess(searchVariable, attribute,
                                       replaceVariable);
  }

  if (_postFilterExpression != nullptr) {
    _postFilterExpression->replaceAttributeAccess(searchVariable, attribute,
                                                  replaceVariable);
  }

  // determine new set of variables used by post filters
  _postFilterVariables.clear();
  for (auto& it : _postFilterConditions) {
    it =
        Ast::replaceAttributeAccess(_plan->getAst(), const_cast<AstNode*>(it),
                                    searchVariable, attribute, replaceVariable);
    // repopulate _postFilterVariables
    Ast::getReferencedVariables(it, _postFilterVariables);
  }

  for (auto& it : _globalEdgeConditions) {
    it =
        Ast::replaceAttributeAccess(_plan->getAst(), const_cast<AstNode*>(it),
                                    searchVariable, attribute, replaceVariable);
  }

  for (auto& it : _globalVertexConditions) {
    it =
        Ast::replaceAttributeAccess(_plan->getAst(), const_cast<AstNode*>(it),
                                    searchVariable, attribute, replaceVariable);
  }

  for (auto& it : _edgeConditions) {
    if (it.second != nullptr) {
      it.second->replaceAttributeAccess(_plan->getAst(), searchVariable,
                                        attribute, replaceVariable);
    }
  }

  for (auto& it : _vertexConditions) {
    it.second = Ast::replaceAttributeAccess(
        _plan->getAst(), it.second, searchVariable, attribute, replaceVariable);
  }

  if (_fromCondition != nullptr) {
    _fromCondition =
        Ast::replaceAttributeAccess(_plan->getAst(), _fromCondition,
                                    searchVariable, attribute, replaceVariable);
  }

  if (_toCondition != nullptr) {
    _toCondition =
        Ast::replaceAttributeAccess(_plan->getAst(), _toCondition,
                                    searchVariable, attribute, replaceVariable);
  }
}

/// @brief getVariablesUsedHere
void TraversalNode::getVariablesUsedHere(VarSet& result) const {
  for (auto const& condVar : _conditionVariables) {
    if (condVar != pathOutVariable() && condVar != getTemporaryVariable()) {
      result.emplace(condVar);
    }
  }

  for (auto const& pruneVar : _pruneVariables) {
    if (pruneVar != vertexOutVariable() && pruneVar != edgeOutVariable() &&
        pruneVar != pathOutVariable()) {
      result.emplace(pruneVar);
    }
  }

  for (auto const& postVar : _postFilterVariables) {
    if (postVar != vertexOutVariable() && postVar != edgeOutVariable() &&
        postVar != pathOutVariable()) {
      result.emplace(postVar);
    }
  }

  if (usesInVariable()) {
    result.emplace(_inVariable);
  }
}

/// @brief getVariablesSetHere
std::vector<Variable const*> TraversalNode::getVariablesSetHere() const {
  std::vector<Variable const*> vars;
  if (isVertexOutVariableAccessed()) {
    vars.emplace_back(vertexOutVariable());
  }
  if (isEdgeOutVariableAccessed()) {
    vars.emplace_back(edgeOutVariable());
  }
  if (isPathOutVariableAccessed()) {
    vars.emplace_back(pathOutVariable());
  }
  return vars;
}

/// @brief check whether an access is inside the specified range
bool TraversalNode::isInRange(uint64_t depth, bool isEdge) const {
  auto opts = static_cast<TraverserOptions*>(options());
  if (isEdge) {
    return (depth < opts->maxDepth);
  }
  return (depth <= opts->maxDepth);
}

void TraversalNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  GraphNode::doToVelocyPack(nodes, flags);
  // In variable
  if (usesInVariable()) {
    nodes.add(VPackValue("inVariable"));
    inVariable()->toVelocyPack(nodes);
  } else {
    nodes.add("vertexId", VPackValue(_vertexId));
  }

  // Condition
  if (_condition != nullptr) {
    nodes.add(VPackValue("condition"));
    _condition->toVelocyPack(nodes, flags != 0);
  }

  if (!_conditionVariables.empty()) {
    nodes.add(VPackValue("conditionVariables"));
    nodes.openArray();
    for (auto const& it : _conditionVariables) {
      it->toVelocyPack(nodes);
    }
    nodes.close();
  }

  // Out variables
  if (isPathOutVariableAccessed()) {
    nodes.add(VPackValue("pathOutVariable"));
    pathOutVariable()->toVelocyPack(nodes);
  }
  if (isVertexOutVariableAccessed()) {
    nodes.add(VPackValue("vertexOutVariable"));
    vertexOutVariable()->toVelocyPack(nodes);
  }
  if (isEdgeOutVariableAccessed()) {
    nodes.add(VPackValue("edgeOutVariable"));
    edgeOutVariable()->toVelocyPack(nodes);
  }

  // Traversal Filter Conditions
  TRI_ASSERT(_fromCondition != nullptr);
  nodes.add(VPackValue("fromCondition"));
  _fromCondition->toVelocyPack(nodes, flags);

  TRI_ASSERT(_toCondition != nullptr);
  nodes.add(VPackValue("toCondition"));
  _toCondition->toVelocyPack(nodes, flags);

  if (!_globalEdgeConditions.empty()) {
    nodes.add(VPackValue("globalEdgeConditions"));
    nodes.openArray();
    for (auto const& it : _globalEdgeConditions) {
      it->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (!_globalVertexConditions.empty()) {
    nodes.add(VPackValue("globalVertexConditions"));
    nodes.openArray();
    for (auto const& it : _globalVertexConditions) {
      it->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (!_vertexConditions.empty()) {
    nodes.add(VPackValue("vertexConditions"));
    nodes.openObject();
    for (auto const& it : _vertexConditions) {
      nodes.add(VPackValue(basics::StringUtils::itoa(it.first)));
      it.second->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (!_edgeConditions.empty()) {
    nodes.add(VPackValue("edgeConditions"));
    nodes.openObject();
    for (auto& it : _edgeConditions) {
      nodes.add(VPackValue(basics::StringUtils::itoa(it.first)));
      it.second->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (_pruneExpression != nullptr) {
    // The Expression constructor expects only this name
    nodes.add(VPackValue("expression"));
    _pruneExpression->toVelocyPack(nodes, flags);
    nodes.add(VPackValue("pruneVariables"));
    nodes.openArray();
    for (auto const& var : _pruneVariables) {
      var->toVelocyPack(nodes);
    }
    nodes.close();
  }

  {
    auto pFilter = postFilterExpression();
    if (pFilter != nullptr) {
      nodes.add(VPackValue("postFilter"));
      VPackObjectBuilder postFilterGuard(&nodes);

      // The Expression constructor expects only this name
      nodes.add(VPackValue("expression"));
      pFilter->toVelocyPack(nodes, flags);

      nodes.add(VPackValue("variables"));
      VPackArrayBuilder postFilterVariablesGuard(&nodes);
      for (auto const& var : _postFilterVariables) {
        var->toVelocyPack(nodes, Variable::WithConstantValue{});
      }
    }
  }
}

std::vector<IndexAccessor> TraversalNode::buildIndexAccessor(
    TraversalEdgeConditionBuilder& conditionBuilder) const {
  std::vector<IndexAccessor> indexAccessors{};
  auto ast = _plan->getAst();
  size_t numEdgeColls = _edgeColls.size();

  auto calculateMemberToUpdate = [&](std::string const& memberString,
                                     std::optional<size_t>& memberToUpdate,
                                     aql::AstNode* indexCondition) {
    std::pair<aql::Variable const*, std::vector<basics::AttributeName>> pathCmp;
    for (size_t x = 0; x < indexCondition->numMembers(); ++x) {
      // We search through the nary-and and look for EQ - _from/_to
      auto eq = indexCondition->getMemberUnchecked(x);
      if (eq->type !=
          arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
        // No equality. Skip
        continue;
      }
      TRI_ASSERT(eq->numMembers() == 2);
      // It is sufficient to only check member one.
      // We build the condition this way.
      auto mem = eq->getMemberUnchecked(0);
      if (mem->isAttributeAccessForVariable(pathCmp, true)) {
        if (pathCmp.first != _tmpObjVariable) {
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
  };

  auto generateExpression =
      [&](aql::AstNode* remainderCondition,
          aql::AstNode* indexCondition) -> std::unique_ptr<aql::Expression> {
    containers::HashSet<size_t> toRemove;
    aql::Condition::collectOverlappingMembers(
        _plan, options()->tmpVar(), remainderCondition, indexCondition,
        toRemove, nullptr, false);
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
      return std::make_unique<aql::Expression>(_plan->getAst(),
                                               remainderCondition);
    }
    return nullptr;
  };

  for (size_t i = 0; i < numEdgeColls; ++i) {
    auto dir = _directions[i];
    TRI_ASSERT(dir == TRI_EDGE_IN || dir == TRI_EDGE_OUT);

    aql::AstNode* condition = (dir == TRI_EDGE_IN)
                                  ? conditionBuilder.getInboundCondition()
                                  : conditionBuilder.getOutboundCondition();
    aql::AstNode* indexCondition = condition->clone(ast);
    std::shared_ptr<Index> indexToUse;

    // arbitrary value for "number of edges in collection" used here. the
    // actual value does not matter much. 1000 has historically worked fine.
    constexpr size_t itemsInCollection = 1000;

    // use most specific index hint here
    auto indexHint =
        hint().getFromNested(dir == TRI_EDGE_IN ? "inbound" : "outbound",
                             _edgeColls[i]->name(), IndexHint::BaseDepth);

    auto& trx = plan()->getAst()->query().trxForOptimization();
    bool res = aql::utils::getBestIndexHandleForFilterCondition(
        trx, *_edgeColls[i], indexCondition, options()->tmpVar(),
        itemsInCollection, indexHint, indexToUse, ReadOwnWrites::no,
        /*onlyEdgeIndexes*/ false);
    if (!res) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "expected index not found for traversal");
    }

    std::optional<size_t> memberToUpdate{std::nullopt};
    calculateMemberToUpdate(dir == TRI_EDGE_IN ? StaticStrings::ToString
                                               : StaticStrings::FromString,
                            memberToUpdate, indexCondition);

    aql::AstNode* remainderCondition = condition->clone(ast);
    std::unique_ptr<aql::Expression> expression =
        generateExpression(remainderCondition, indexCondition);

    auto container = aql::utils::extractNonConstPartsOfIndexCondition(
        ast, getRegisterPlan()->varInfo, false, nullptr, indexCondition,
        options()->tmpVar());
    indexAccessors.emplace_back(std::move(indexToUse), indexCondition,
                                memberToUpdate, std::move(expression),
                                std::move(container), i, dir);
  }

  return indexAccessors;
}

std::vector<arangodb::graph::IndexAccessor> TraversalNode::buildUsedIndexes()
    const {
  TraversalEdgeConditionBuilder globalEdgeConditionBuilder(this);

  for (auto& it : _globalEdgeConditions) {
    globalEdgeConditionBuilder.addConditionPart(it);
  }

  return buildIndexAccessor(globalEdgeConditionBuilder);
}

std::unordered_map<uint64_t, std::vector<IndexAccessor>>
TraversalNode::buildUsedDepthBasedIndexes() const {
  std::unordered_map<uint64_t, std::vector<IndexAccessor>> result{};
  for (auto const& [depth, builder] : _edgeConditions) {
    TRI_ASSERT(builder != nullptr);
    result.emplace(depth, buildIndexAccessor(*builder));
  }

  return result;
}

std::unique_ptr<ExecutionBlock> TraversalNode::createBlock(
    ExecutionEngine& engine,
    std::vector<std::pair<Variable const*, RegisterId>>&&
        filterConditionVariables,
    std::function<void(std::shared_ptr<aql::PruneExpressionEvaluator>&)> const&
        checkPruneAvailability,
    std::function<void(std::shared_ptr<aql::PruneExpressionEvaluator>&)> const&
        checkPostFilterAvailability,
    std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                       TraversalExecutorInfosHelper::OutputNameHash> const&
        outputRegisterMapping,
    RegisterId inputRegister, RegisterInfos registerInfos,
    std::unordered_map<ServerID, aql::EngineId> const* engines,
    bool isSmart) const {
  TraverserOptions* opts = this->options();

  arangodb::graph::OneSidedEnumeratorOptions options{opts->minDepth,
                                                     opts->maxDepth};
  /*
   * PathValidator Disjoint Helper (TODO [GraphRefactor]: Copy from createBlock)
   * Clean this up as soon we clean up the whole TraversalNode as well.
   */
  auto generateDisjointPathValidatorOptions = [&]() -> std::pair<bool, bool> {
    bool isSatLeader = false;
    if (isDisjoint()) {
      isSatLeader = opts->isSatelliteLeader();
    }
    return {isDisjoint(), isSatLeader};
  };
  auto isDisjointIsSat = generateDisjointPathValidatorOptions();

  PathValidatorOptions validatorOptions{
      opts->tmpVar(), opts->getExpressionCtx(), isDisjointIsSat.first,
      isDisjointIsSat.second, isClusterOneShardRuleEnabled()};

  // Prune Section
  if (pruneExpression() != nullptr) {
    std::shared_ptr<aql::PruneExpressionEvaluator> pruneEvaluator;
    checkPruneAvailability(pruneEvaluator);
    validatorOptions.setPruneEvaluator(std::move(pruneEvaluator));
  }

  // Post-filter section
  if (postFilterExpression() != nullptr) {
    std::shared_ptr<aql::PruneExpressionEvaluator> postFilterEvaluator;
    checkPostFilterAvailability(postFilterEvaluator);
    validatorOptions.setPostFilterEvaluator(std::move(postFilterEvaluator));
  }

  // Vertex Expressions Section
  // I. Set the list of allowed collections
  validatorOptions.addAllowedVertexCollections(opts->vertexCollections);

  // II. Global prune expression
  if (opts->_baseVertexExpression != nullptr) {
    auto baseVertexExpression =
        opts->_baseVertexExpression->clone(_plan->getAst());
    validatorOptions.setAllVerticesExpression(std::move(baseVertexExpression));
  }

  // III. Depth-based prune expressions
  for (auto const& vertexExpressionPerDepth : opts->_vertexExpressions) {
    auto depth = vertexExpressionPerDepth.first;
    auto expression = vertexExpressionPerDepth.second->clone(_plan->getAst());
    validatorOptions.setVertexExpression(depth, std::move(expression));
  }

  if (ServerState::instance()->isCoordinator()) {
    auto clusterBaseProviderOptions =
        getClusterBaseProviderOptions(opts, filterConditionVariables);

    auto executorInfos = TraversalExecutorInfos(  // todo add a parameter:
                                                  // SingleServer, Cluster...
        outputRegisterMapping, getStartVertex(), inputRegister,
        plan()->getAst(), opts->uniqueVertices, opts->uniqueEdges, opts->mode,
        opts->defaultWeight, opts->weightAttribute, opts->query(),
        std::move(validatorOptions), std::move(options),
        std::move(clusterBaseProviderOptions), isSmart);

    return std::make_unique<ExecutionBlockImpl<TraversalExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else {
    TRI_ASSERT(!isSmart);
    auto singleServerBaseProviderOptions =
        getSingleServerBaseProviderOptions(opts, filterConditionVariables);
    auto executorInfos = TraversalExecutorInfos(
        outputRegisterMapping, getStartVertex(), inputRegister,
        plan()->getAst(), opts->uniqueVertices, opts->uniqueEdges, opts->mode,
        opts->defaultWeight, opts->weightAttribute, opts->query(),
        std::move(validatorOptions), std::move(options),
        std::move(singleServerBaseProviderOptions), isSmart);

    return std::make_unique<ExecutionBlockImpl<TraversalExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
}

ClusterBaseProviderOptions TraversalNode::getClusterBaseProviderOptions(
    TraverserOptions* opts,
    std::vector<std::pair<Variable const*, RegisterId>> const&
        filterConditionVariables) const {
  auto traverserCache = std::make_shared<RefactoredClusterTraverserCache>(
      opts->query().resourceMonitor());
  std::unordered_set<uint64_t> availableDepthsSpecificConditions;
  availableDepthsSpecificConditions.reserve(opts->_depthLookupInfo.size());
  for (auto const& [depth, _] : opts->_depthLookupInfo) {
    availableDepthsSpecificConditions.emplace(depth);
  }
  return {traverserCache,
          engines(),
          false,
          opts->produceVertices(),
          &opts->getExpressionCtx(),
          filterConditionVariables,
          std::move(availableDepthsSpecificConditions)};
}

SingleServerBaseProviderOptions
TraversalNode::getSingleServerBaseProviderOptions(
    TraverserOptions* opts,
    std::vector<std::pair<Variable const*, RegisterId>> const&
        filterConditionVariables) const {
  std::pair<std::vector<IndexAccessor>,
            std::unordered_map<uint64_t, std::vector<IndexAccessor>>>
      usedIndexes{};
  usedIndexes.first = buildUsedIndexes();
  usedIndexes.second = buildUsedDepthBasedIndexes();
  return {opts->tmpVar(),
          std::move(usedIndexes),
          opts->getExpressionCtx(),
          filterConditionVariables,
          opts->collectionToShard(),
          opts->getVertexProjections(),
          opts->getEdgeProjections(),
          opts->produceVertices(),
          opts->useCache()};
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> TraversalNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  auto inputRegisters = RegIdSet{};
  auto& varInfo = getRegisterPlan()->varInfo;
  RegisterId inputRegister{RegisterId::maxRegisterId};
  if (usesInVariable()) {
    auto it = varInfo.find(inVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    inputRegisters.emplace(it->second.registerId);
    inputRegister = it->second.registerId;
    TRI_ASSERT(getStartVertex().empty());
  }
  auto outputRegisters = RegIdSet{};
  std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                     TraversalExecutorInfosHelper::OutputNameHash>
      outputRegisterMapping;

  if (isVertexOutVariableUsedLater()) {
    auto it = varInfo.find(vertexOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId.isValid());
    outputRegisters.emplace(it->second.registerId);
    outputRegisterMapping.try_emplace(
        TraversalExecutorInfosHelper::OutputName::VERTEX,
        it->second.registerId);
  }
  if (isEdgeOutVariableUsedLater()) {
    auto it = varInfo.find(edgeOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId.isValid());
    outputRegisters.emplace(it->second.registerId);
    outputRegisterMapping.try_emplace(
        TraversalExecutorInfosHelper::OutputName::EDGE, it->second.registerId);
  }
  if (isPathOutVariableUsedLater()) {
    auto it = varInfo.find(pathOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId.isValid());
    outputRegisters.emplace(it->second.registerId);
    outputRegisterMapping.try_emplace(
        TraversalExecutorInfosHelper::OutputName::PATH, it->second.registerId);
  }
  TraverserOptions* opts = this->options();

  /*
   * PRUNE SECTION
   */
  auto checkPruneAvailability =
      [&](std::shared_ptr<aql::PruneExpressionEvaluator>& evaluator) {
        std::vector<Variable const*> pruneVars;
        getPruneVariables(pruneVars);
        std::vector<RegisterId> pruneRegs;
        // Create List for _pruneVars
        pruneRegs.reserve(pruneVars.size());
        size_t vertexRegIdx = std::numeric_limits<std::size_t>::max();
        size_t edgeRegIdx = std::numeric_limits<std::size_t>::max();
        size_t pathRegIdx = std::numeric_limits<std::size_t>::max();
        for (auto const v : pruneVars) {
          if (v == vertexOutVariable()) {
            vertexRegIdx = pruneRegs.size();
            pruneRegs.emplace_back(RegisterPlan::MaxRegisterId);
          } else if (v == edgeOutVariable()) {
            edgeRegIdx = pruneRegs.size();
            pruneRegs.emplace_back(RegisterPlan::MaxRegisterId);
          } else if (v == pathOutVariable()) {
            pathRegIdx = pruneRegs.size();
            pruneRegs.emplace_back(RegisterPlan::MaxRegisterId);
          } else {
            auto it = varInfo.find(v->id);
            TRI_ASSERT(it != varInfo.end());
            pruneRegs.emplace_back(it->second.registerId);
          }
        }

        auto expr = opts->createPruneEvaluator(
            std::move(pruneVars), std::move(pruneRegs), vertexRegIdx,
            edgeRegIdx, pathRegIdx, pruneExpression());
        evaluator = std::move(expr);
      };

  auto checkPostFilterAvailability =
      [&](std::shared_ptr<aql::PruneExpressionEvaluator>& evaluator) {
        std::vector<Variable const*> postFilterVars;
        getPostFilterVariables(postFilterVars);
        std::vector<RegisterId> postFilterRegs;
        // Create List for _pruneVars
        postFilterRegs.reserve(postFilterVars.size());
        size_t vertexRegIdx = std::numeric_limits<std::size_t>::max();
        size_t edgeRegIdx = std::numeric_limits<std::size_t>::max();
        for (auto const v : postFilterVars) {
          if (v == vertexOutVariable()) {
            vertexRegIdx = postFilterRegs.size();
            postFilterRegs.emplace_back(RegisterPlan::MaxRegisterId);
          } else if (v == edgeOutVariable()) {
            edgeRegIdx = postFilterRegs.size();
            postFilterRegs.emplace_back(RegisterPlan::MaxRegisterId);
          } else if (v == pathOutVariable()) {
            TRI_ASSERT(false);
          } else {
            auto it = varInfo.find(v->id);
            TRI_ASSERT(it != varInfo.end());
            postFilterRegs.emplace_back(it->second.registerId);
          }
        }

        auto expr = opts->createPostFilterEvaluator(
            std::move(postFilterVars), std::move(postFilterRegs), vertexRegIdx,
            edgeRegIdx, postFilterExpression());
        evaluator = std::move(expr);
      };

  if (postFilterExpression() != nullptr) {
    std::vector<Variable const*> postFilterVars;
    getPostFilterVariables(postFilterVars);
    std::vector<RegisterId> postFilterRegs;
    // Create List for _pruneVars
    postFilterRegs.reserve(postFilterVars.size());
    size_t vertexRegIdx = std::numeric_limits<std::size_t>::max();
    size_t edgeRegIdx = std::numeric_limits<std::size_t>::max();
    for (auto const v : postFilterVars) {
      if (v == vertexOutVariable()) {
        vertexRegIdx = postFilterRegs.size();
        postFilterRegs.emplace_back(RegisterPlan::MaxRegisterId);
      } else if (v == edgeOutVariable()) {
        edgeRegIdx = postFilterRegs.size();
        postFilterRegs.emplace_back(RegisterPlan::MaxRegisterId);
      } else {
        // We can never optimize the path variable here. We can just do a normal
        // post filtering then. This should only avoid building path to aql.
        TRI_ASSERT(v != pathOutVariable());
        auto it = varInfo.find(v->id);
        TRI_ASSERT(it != varInfo.end());
        postFilterRegs.emplace_back(it->second.registerId);
      }
    }
    // We need to select at least one of vertex or edge for this filter.
    TRI_ASSERT(vertexRegIdx != std::numeric_limits<std::size_t>::max() ||
               edgeRegIdx != std::numeric_limits<std::size_t>::max());

    opts->activatePostFilter(std::move(postFilterVars),
                             std::move(postFilterRegs), vertexRegIdx,
                             edgeRegIdx, postFilterExpression());
  }

  // Optimized condition
  std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables;
  filterConditionVariables.reserve(_conditionVariables.size());

  for (auto const& it : _conditionVariables) {
    if (it != _tmpObjVariable) {
      auto idIt = varInfo.find(it->id);
      TRI_ASSERT(idIt != varInfo.end());
      filterConditionVariables.emplace_back(
          std::make_pair(it, idIt->second.registerId));
      inputRegisters.emplace(idIt->second.registerId);
    }
  }

  auto registerInfos = createRegisterInfos(std::move(inputRegisters),
                                           std::move(outputRegisters));

  if (arangodb::ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    /*
     * SmartGraph Traverser
     */
    if (isSmart() && !isDisjoint()) {
      return createBlock(engine, std::move(filterConditionVariables),
                         checkPruneAvailability, checkPostFilterAvailability,
                         outputRegisterMapping, inputRegister,
                         std::move(registerInfos), engines(), true /*isSmart*/);

    } else {
#endif
      /*
       * Default Cluster Traverser
       */
      return createBlock(engine, std::move(filterConditionVariables),
                         checkPruneAvailability, checkPostFilterAvailability,
                         outputRegisterMapping, inputRegister,
                         std::move(registerInfos), engines());

#ifdef USE_ENTERPRISE
    }
#endif
  }

  if (isDisjoint()) {
    // TODO [GraphRefactor]: maybe remove
    opts->setDisjoint();
  }
  /*
   * Default SingleServer Traverser
   */

  // We need to prepare the variable accesses before we ask the index nodes.
  initializeIndexConditions();

  return createBlock(engine, std::move(filterConditionVariables),
                     checkPruneAvailability, checkPostFilterAvailability,
                     outputRegisterMapping, inputRegister,
                     std::move(registerInfos), nullptr);
}

/// @brief clone ExecutionNode recursively
ExecutionNode* TraversalNode::clone(ExecutionPlan* plan,
                                    bool withDependencies) const {
  auto* oldOpts = options();
  std::unique_ptr<BaseOptions> tmp = std::make_unique<TraverserOptions>(
      *oldOpts, /*allowAlreadyBuiltCopy*/ true);
  auto c = std::make_unique<TraversalNode>(
      plan, _id, _vocbase, _edgeColls, _vertexColls, _inVariable, _vertexId,
      _defaultDirection, _directions, std::move(tmp), _graphObj);

  traversalCloneHelper(*plan, *c);

  if (_optionsBuilt) {
    c->prepareOptions();
  }

  return cloneHelper(std::move(c), withDependencies);
}

void TraversalNode::traversalCloneHelper(ExecutionPlan& plan,
                                         TraversalNode& c) const {
  graphCloneHelper(plan, c);
  if (isVertexOutVariableAccessed()) {
    TRI_ASSERT(_vertexOutVariable != nullptr);
    c.setVertexOutput(_vertexOutVariable);
  }

  if (isEdgeOutVariableAccessed()) {
    TRI_ASSERT(_edgeOutVariable != nullptr);
    c.setEdgeOutput(_edgeOutVariable);
  }

  if (isPathOutVariableAccessed()) {
    TRI_ASSERT(_pathOutVariable != nullptr);
    c.setPathOutput(_pathOutVariable);
  }

  c._conditionVariables.reserve(_conditionVariables.size());
  for (auto const& it : _conditionVariables) {
    c._conditionVariables.emplace(it);
  }

  if (_pruneExpression) {
    c._pruneExpression = _pruneExpression->clone(plan.getAst(), true);
    c._pruneVariables.reserve(_pruneVariables.size());
    for (auto const& it : _pruneVariables) {
      c._pruneVariables.emplace(it);
    }
  }

  for (auto const& it : _postFilterConditions) {
    c.registerPostFilterCondition(it->clone(plan.getAst()));
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif

  // Temporary Filter Objects
  c._tmpObjVariable = _tmpObjVariable;
  c._tmpObjVarNode = _tmpObjVarNode;
  c._tmpIdNode = _tmpIdNode;

  // Filter Condition Parts
  c._fromCondition = _fromCondition->clone(_plan->getAst());
  c._toCondition = _toCondition->clone(_plan->getAst());
  for (auto const& it : _globalEdgeConditions) {
    c._globalEdgeConditions.emplace_back(it->clone(_plan->getAst()));
  }
  for (auto const& it : _globalVertexConditions) {
    c._globalVertexConditions.emplace_back(it->clone(_plan->getAst()));
  }

  for (auto const& it : _edgeConditions) {
    // Copy the builder
    auto ecBuilder =
        std::make_unique<TraversalEdgeConditionBuilder>(this, it.second.get());
    c._edgeConditions.try_emplace(it.first, std::move(ecBuilder));
  }

  for (auto const& it : _vertexConditions) {
    c._vertexConditions.try_emplace(it.first,
                                    it.second->clone(_plan->getAst()));
  }

  if (_condition) {
    c.setCondition(_condition->clone());
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  c.checkConditionsDefined();
  // validate that we copied index hints correctly
  TRI_ASSERT(c.hint().isSet() == hint().isSet());
#endif
}

void TraversalNode::prepareOptions() {
  if (_optionsBuilt) {
    return;
  }
  TRI_ASSERT(!_optionsBuilt);
  _options->setVariable(_tmpObjVariable);

  size_t numEdgeColls = _edgeColls.size();
  TraversalEdgeConditionBuilder globalEdgeConditionBuilder(this);

  for (auto& it : _globalEdgeConditions) {
    globalEdgeConditionBuilder.addConditionPart(it);
  }

  Ast* ast = _plan->getAst();

  // Compute Edge Indexes. First default indexes:
  for (size_t i = 0; i < numEdgeColls; ++i) {
    auto dir = _directions[i];
    switch (dir) {
      case TRI_EDGE_IN:
        _options->addLookupInfo(
            _plan, _edgeColls[i]->name(), StaticStrings::ToString,
            globalEdgeConditionBuilder.getInboundCondition()->clone(ast),
            /*onlyEdgeIndexes*/ false, dir);
        break;
      case TRI_EDGE_OUT:
        _options->addLookupInfo(
            _plan, _edgeColls[i]->name(), StaticStrings::FromString,
            globalEdgeConditionBuilder.getOutboundCondition()->clone(ast),
            /*onlyEdgeIndexes*/ false, dir);
        break;
      case TRI_EDGE_ANY:
        TRI_ASSERT(false);
        break;
    }
  }

  TraverserOptions* opts = this->TraversalNode::options();
  TRI_ASSERT(opts != nullptr);
  for (auto& it : _edgeConditions) {
    uint64_t depth = it.first;
    // We probably have to adopt minDepth. We cannot fulfill a condition of
    // larger depth anyway
    auto& builder = it.second;

    for (auto& it2 : _globalEdgeConditions) {
      builder->addConditionPart(it2);
    }

    for (size_t i = 0; i < numEdgeColls; ++i) {
      auto dir = _directions[i];
      // TODO we can optimize here. indexCondition and Expression could be
      // made non-overlapping.
      switch (dir) {
        case TRI_EDGE_IN:
          opts->addDepthLookupInfo(
              _plan, _edgeColls[i]->name(), StaticStrings::ToString,
              builder->getInboundCondition()->clone(ast), depth, dir);
          break;
        case TRI_EDGE_OUT:
          opts->addDepthLookupInfo(
              _plan, _edgeColls[i]->name(), StaticStrings::FromString,
              builder->getOutboundCondition()->clone(ast), depth, dir);
          break;
        case TRI_EDGE_ANY:
          TRI_ASSERT(false);
          break;
      }
    }
  }

  for (auto& it : _vertexConditions) {
    // We inject the base conditions as well here.
    for (auto const& jt : _globalVertexConditions) {
      it.second->addMember(jt);
    }
    opts->_vertexExpressions.try_emplace(
        it.first, arangodb::lazyConstruct(
                      [&] { return new Expression(ast, it.second); }));
  }
  if (!_globalVertexConditions.empty()) {
    auto cond =
        _plan->getAst()->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    for (auto const& it : _globalVertexConditions) {
      cond->addMember(it);
    }
    opts->_baseVertexExpression = std::make_unique<Expression>(ast, cond);
  }
  // If we use the path output the cache should activate document
  // caching otherwise it is not worth it.
  if (ServerState::instance()->isCoordinator()) {
    _options->activateCache(false, engines());
  } else {
    _options->activateCache(false, nullptr);
  }
  _optionsBuilt = true;
}

/// @brief remember the condition to execute for early traversal abortion.
void TraversalNode::setCondition(
    std::unique_ptr<arangodb::aql::Condition> condition) {
  VarSet varsUsedByCondition;

  Ast::getReferencedVariables(condition->root(), varsUsedByCondition);

  for (auto const& oneVar : varsUsedByCondition) {
    if ((_vertexOutVariable == nullptr ||
         oneVar->id != _vertexOutVariable->id) &&
        (_edgeOutVariable == nullptr || oneVar->id != _edgeOutVariable->id) &&
        (_pathOutVariable == nullptr || oneVar->id != _pathOutVariable->id) &&
        (_inVariable == nullptr || oneVar->id != _inVariable->id) &&
        (!_optimizedOutVariables.contains(oneVar->id))) {
      _conditionVariables.emplace(oneVar);
    }
  }

  _condition = std::move(condition);
}

void TraversalNode::registerCondition(bool isConditionOnEdge,
                                      uint64_t conditionLevel,
                                      AstNode const* condition) {
  Ast::getReferencedVariables(condition, _conditionVariables);
  if (isConditionOnEdge) {
    auto [it, emplaced] = _edgeConditions.try_emplace(
        conditionLevel,
        arangodb::lazyConstruct(
            [&]() -> std::unique_ptr<TraversalEdgeConditionBuilder> {
              auto builder =
                  std::make_unique<TraversalEdgeConditionBuilder>(this);
              builder->addConditionPart(condition);
              return builder;
            }));
    if (!emplaced) {
      it->second->addConditionPart(condition);
    }
  } else {
    auto [it, emplaced] = _vertexConditions.try_emplace(
        conditionLevel, arangodb::lazyConstruct([&] {
          auto cond = _plan->getAst()->createNodeNaryOperator(
              NODE_TYPE_OPERATOR_NARY_AND);
          cond->addMember(condition);
          return cond;
        }));
    if (!emplaced) {
      it->second->addMember(condition);
    }
  }
}

void TraversalNode::registerGlobalCondition(bool isConditionOnEdge,
                                            AstNode const* condition) {
  Ast::getReferencedVariables(condition, _conditionVariables);
  if (isConditionOnEdge) {
    _globalEdgeConditions.emplace_back(condition);
  } else {
    _globalVertexConditions.emplace_back(condition);
  }
}

void TraversalNode::registerPostFilterCondition(AstNode const* condition) {
  // We cannot modify the postFilterExpression after it was build
  TRI_ASSERT(_postFilterExpression == nullptr);
  TRI_ASSERT(condition != nullptr);
  TRI_ASSERT(!condition->willUseV8());
  TRI_ASSERT(!ServerState::instance()->isRunningInCluster() ||
             condition->canRunOnDBServer(vocbase()->isOneShard()));
  _postFilterConditions.emplace_back(condition);
  Ast::getReferencedVariables(condition, _postFilterVariables);
}

void TraversalNode::getConditionVariables(
    std::vector<Variable const*>& res) const {
  for (auto const& it : _conditionVariables) {
    if (it != _tmpObjVariable) {
      res.emplace_back(it);
    }
  }
}

void TraversalNode::getPruneVariables(std::vector<Variable const*>& res) const {
  if (_pruneExpression) {
    for (auto const& it : _pruneVariables) {
      res.emplace_back(it);
    }
  }
}

void TraversalNode::getPostFilterVariables(
    std::vector<Variable const*>& res) const {
  for (auto const& it : _postFilterVariables) {
    res.emplace_back(it);
  }
}

bool TraversalNode::isPathOutVariableUsedLater() const {
  return _pathOutVariable != nullptr &&
         (options()->producePathsVertices() || options()->producePathsEdges() ||
          options()->producePathsWeights());
}

bool TraversalNode::isPathOutVariableAccessed() const {
  if (_pathOutVariable != nullptr) {
    return isPathOutVariableUsedLater() ||
           _pruneVariables.contains(_pathOutVariable);
  } else {
    return false;
  }
}

bool TraversalNode::isEdgeOutVariableAccessed() const {
  if (_edgeOutVariable != nullptr) {
    return isEdgeOutVariableUsedLater() ||
           _pruneVariables.contains(_edgeOutVariable);
  } else {
    return false;
  }
}

bool TraversalNode::isVertexOutVariableAccessed() const {
  if (_vertexOutVariable != nullptr) {
    return isVertexOutVariableUsedLater() ||
           _pruneVariables.contains(_vertexOutVariable);
  } else {
    return false;
  }
}

auto TraversalNode::options() const -> TraverserOptions* {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* opts = dynamic_cast<TraverserOptions*>(GraphNode::options());
  TRI_ASSERT((GraphNode::options() == nullptr) == (opts == nullptr));
#else
  auto* opts = static_cast<TraverserOptions*>(GraphNode::options());
#endif
  return opts;
}

Expression* TraversalNode::postFilterExpression() const {
  if (!_postFilterExpression && !_postFilterConditions.empty()) {
    auto ast = _plan->getAst();
    AstNode* ands = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    for (auto it : _postFilterConditions) {
      ands->addMember(it);
    }
    _postFilterExpression = std::make_unique<Expression>(ast, ands);
  }
  return _postFilterExpression.get();
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void TraversalNode::checkConditionsDefined() const {
  TRI_ASSERT(_tmpObjVariable != nullptr);
  TRI_ASSERT(_tmpObjVarNode != nullptr);
  TRI_ASSERT(_tmpIdNode != nullptr);

  TRI_ASSERT(_fromCondition != nullptr);
  TRI_ASSERT(_fromCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);

  TRI_ASSERT(_toCondition != nullptr);
  TRI_ASSERT(_toCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);
}
#endif

void TraversalNode::validateCollections() const {
  if (!ServerState::instance()->isSingleServerOrCoordinator()) {
    // validation must happen on single/coordinator.
    // DB servers only see shard names here
    return;
  }
  auto* g = graph();
  if (g == nullptr) {
    // list of edge collections
    for (auto const& it : options()->edgeCollections) {
      if (_edgeAliases.contains(it)) {
        // SmartGraph edge collection. its real name is contained
        // in the aliases list
        continue;
      }
      if (std::find_if(_edgeColls.begin(), _edgeColls.end(),
                       [&it](aql::Collection const* c) {
                         return c->name() == it;
                       }) == _edgeColls.end()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST,
            absl::StrCat(
                "edge collection '", it,
                "' used in 'edgeCollections' option is not part of the "
                "specified edge collection list"));
      }
    }
  } else {
    // named graph
    {
      auto colls = g->vertexCollections();
      for (auto const& it : options()->vertexCollections) {
        if (!colls.contains(it)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST,
              absl::StrCat(
                  "vertex collection '", it,
                  "' used in 'vertexCollections' option is not part of "
                  "the specified graph"));
        }
      }
    }

    {
      auto colls = g->edgeCollections();
      for (auto const& it : options()->edgeCollections) {
        if (!colls.contains(it)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST,
              absl::StrCat(
                  "edge collection '", it,
                  "' used in 'edgeCollections' option is not part of the "
                  "specified graph"));
        }
      }
    }
  }
}

size_t TraversalNode::getMemoryUsedBytes() const { return sizeof(*this); }
