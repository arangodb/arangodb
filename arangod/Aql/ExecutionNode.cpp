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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionNode.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Ast.h"
#include "Aql/AsyncExecutor.h"
#include "Aql/CalculationExecutor.h"
#include "Aql/ClusterNodes.h"
#include "Aql/CollectNode.h"
#include "Aql/Collection.h"
#include "Aql/ConstFetcher.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/EnumeratePathsNode.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/FilterExecutor.h"
#include "Aql/Function.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/IdExecutor.h"
#include "Aql/IndexNode.h"
#include "Aql/JoinNode.h"
#include "Aql/LimitExecutor.h"
#include "Aql/MaterializeExecutor.h"
#include "Aql/ModificationNodes.h"
#include "Aql/MultipleRemoteModificationNode.h"
#include "Aql/MutexNode.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/NodeFinder.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/Range.h"
#include "Aql/RegisterPlan.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/ShortestPathNode.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"
#include "Aql/SubqueryEndExecutionNode.h"
#include "Aql/SubqueryStartExecutionNode.h"
#include "Aql/TraversalNode.h"
#include "Aql/WalkerWorker.h"
#include "Aql/WindowNode.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-compiler.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/CountCache.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace materialize;

namespace {

/// @brief NodeType to string mapping
std::unordered_map<int, std::string const> const typeNames{
    {static_cast<int>(ExecutionNode::SINGLETON), "SingletonNode"},
    {static_cast<int>(ExecutionNode::ENUMERATE_COLLECTION),
     "EnumerateCollectionNode"},
    {static_cast<int>(ExecutionNode::ENUMERATE_LIST), "EnumerateListNode"},
    {static_cast<int>(ExecutionNode::INDEX), "IndexNode"},
    {static_cast<int>(ExecutionNode::LIMIT), "LimitNode"},
    {static_cast<int>(ExecutionNode::CALCULATION), "CalculationNode"},
    {static_cast<int>(ExecutionNode::SUBQUERY), "SubqueryNode"},
    {static_cast<int>(ExecutionNode::FILTER), "FilterNode"},
    {static_cast<int>(ExecutionNode::SORT), "SortNode"},
    {static_cast<int>(ExecutionNode::COLLECT), "CollectNode"},
    {static_cast<int>(ExecutionNode::RETURN), "ReturnNode"},
    {static_cast<int>(ExecutionNode::REMOVE), "RemoveNode"},
    {static_cast<int>(ExecutionNode::INSERT), "InsertNode"},
    {static_cast<int>(ExecutionNode::UPDATE), "UpdateNode"},
    {static_cast<int>(ExecutionNode::REPLACE), "ReplaceNode"},
    {static_cast<int>(ExecutionNode::REMOTE), "RemoteNode"},
    {static_cast<int>(ExecutionNode::SCATTER), "ScatterNode"},
    {static_cast<int>(ExecutionNode::DISTRIBUTE), "DistributeNode"},
    {static_cast<int>(ExecutionNode::GATHER), "GatherNode"},
    {static_cast<int>(ExecutionNode::NORESULTS), "NoResultsNode"},
    {static_cast<int>(ExecutionNode::UPSERT), "UpsertNode"},
    {static_cast<int>(ExecutionNode::TRAVERSAL), "TraversalNode"},
    {static_cast<int>(ExecutionNode::SHORTEST_PATH), "ShortestPathNode"},
    {static_cast<int>(ExecutionNode::ENUMERATE_PATHS), "EnumeratePathsNode"},
    {static_cast<int>(ExecutionNode::REMOTESINGLE),
     "SingleRemoteOperationNode"},
    {static_cast<int>(ExecutionNode::REMOTE_MULTIPLE),
     "MultipleRemoteModificationNode"},
    {static_cast<int>(ExecutionNode::ENUMERATE_IRESEARCH_VIEW),
     "EnumerateViewNode"},
    {static_cast<int>(ExecutionNode::SUBQUERY_START), "SubqueryStartNode"},
    {static_cast<int>(ExecutionNode::SUBQUERY_END), "SubqueryEndNode"},
    {static_cast<int>(ExecutionNode::DISTRIBUTE_CONSUMER),
     "DistributeConsumer"},
    {static_cast<int>(ExecutionNode::MATERIALIZE), "MaterializeNode"},
    {static_cast<int>(ExecutionNode::ASYNC), "AsyncNode"},
    {static_cast<int>(ExecutionNode::MUTEX), "MutexNode"},
    {static_cast<int>(ExecutionNode::WINDOW), "WindowNode"},
    {static_cast<int>(ExecutionNode::OFFSET_INFO_MATERIALIZE),
     "OffsetMaterializeNode"},
    {static_cast<int>(ExecutionNode::JOIN), "JoinNode"},
};

}  // namespace

namespace arangodb::aql {

size_t estimateListLength(ExecutionPlan const* plan, Variable const* var) {
  // Well, what can we say? The length of the list can in general
  // only be determined at runtime... If we were to know that this
  // list is constant, then we could maybe multiply by the length
  // here... For the time being, we assume 100
  size_t length = 100;

  auto setter = plan->getVarSetBy(var->id);

  if (setter != nullptr) {
    if (setter->getType() == ExecutionNode::CALCULATION) {
      // list variable introduced by a calculation
      auto expression =
          ExecutionNode::castTo<CalculationNode*>(setter)->expression();

      if (expression != nullptr) {
        auto node = expression->node();

        if (node->type == NODE_TYPE_ARRAY) {
          // this one is easy
          length = node->numMembers();
        }
        if (node->type == NODE_TYPE_RANGE) {
          auto low = node->getMember(0);
          auto high = node->getMember(1);

          if (low->isConstant() && high->isConstant() &&
              (low->isValueType(VALUE_TYPE_INT) ||
               low->isValueType(VALUE_TYPE_DOUBLE)) &&
              (high->isValueType(VALUE_TYPE_INT) ||
               high->isValueType(VALUE_TYPE_DOUBLE))) {
            // create a temporary range to determine the size
            Range range(low->getIntValue(), high->getIntValue());

            length = range.size();
          }
        }
      }
    } else if (setter->getType() == ExecutionNode::SUBQUERY) {
      // length will be set by the subquery's cost estimator
      CostEstimate subEstimate =
          ExecutionNode::castTo<SubqueryNode const*>(setter)
              ->getSubquery()
              ->getCost();
      length = subEstimate.estimatedNrItems;
    }
  }
  return length;
}

ExecutionNode* createOffsetMaterializeNode(ExecutionPlan*, velocypack::Slice);

#ifndef USE_ENTERPRISE
ExecutionNode* createOffsetMaterializeNode(ExecutionPlan*, velocypack::Slice) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "Function 'OFFSET_INFO' is available in "
                                 "ArangoDB Enterprise Edition only.");
}
#endif
}  // namespace arangodb::aql

/// @brief resolve nodeType to a string.
std::string const& ExecutionNode::getTypeString(NodeType type) {
  auto it = ::typeNames.find(static_cast<int>(type));

  if (it != ::typeNames.end()) {
    return (*it).second;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "missing type in TypeNames");
}

/// @brief returns the type name of the node
std::string const& ExecutionNode::getTypeString() const {
  return getTypeString(getType());
}

void ExecutionNode::validateType(int type) {
  auto it = ::typeNames.find(static_cast<int>(type));

  if (it == ::typeNames.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "unknown TypeID");
  }
}

bool ExecutionNode::isInSubquery() const {
  auto const* current = this;
  while (current != nullptr && current->hasDependency()) {
    current = current->getFirstDependency();
  }
  TRI_ASSERT(current != nullptr);
  return current->id() != ExecutionNodeId{1};
}

/// @brief add a dependency
void ExecutionNode::addDependency(ExecutionNode* ep) {
  TRI_ASSERT(ep != nullptr);
  _dependencies.emplace_back(ep);
  ep->_parents.emplace_back(this);
}

/// @brief add a parent
void ExecutionNode::addParent(ExecutionNode* ep) {
  TRI_ASSERT(ep != nullptr);
  ep->_dependencies.emplace_back(this);
  _parents.emplace_back(ep);
}

void ExecutionNode::getSortElements(SortElementVector& elements,
                                    ExecutionPlan* plan,
                                    arangodb::velocypack::Slice slice,
                                    char const* which) {
  VPackSlice elementsSlice = slice.get("elements");

  if (!elementsSlice.isArray()) {
    std::string error = std::string("unexpected value for ") +
                        std::string(which) + std::string(" elements");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
  }

  elements.reserve(elementsSlice.length());

  for (VPackSlice it : VPackArrayIterator(elementsSlice)) {
    bool ascending = it.get("ascending").getBoolean();
    Variable* v = Variable::varFromVPack(plan->getAst(), it, "inVariable");
    elements.emplace_back(v, ascending);
    // Is there an attribute path?
    VPackSlice path = it.get("path");
    if (path.isArray()) {
      // Get a list of strings out and add to the path:
      auto& element = elements.back();
      for (auto const& it2 : VPackArrayIterator(path)) {
        if (it2.isString()) {
          element.attributePath.emplace_back(it2.copyString());
        }
      }
    }
  }
}

ExecutionNode* ExecutionNode::fromVPackFactory(ExecutionPlan* plan,
                                               velocypack::Slice slice) {
  TRI_ASSERT(slice.get("typeID").isNumber()) << slice.toJson();
  int nodeTypeID = slice.get("typeID").getNumericValue<int>();
  validateType(nodeTypeID);

  NodeType nodeType = static_cast<NodeType>(nodeTypeID);

  switch (nodeType) {
    case SINGLETON:
      return new SingletonNode(plan, slice);
    case ENUMERATE_COLLECTION:
      return new EnumerateCollectionNode(plan, slice);
    case ENUMERATE_LIST:
      return new EnumerateListNode(plan, slice);
    case FILTER:
      return new FilterNode(plan, slice);
    case LIMIT:
      return new LimitNode(plan, slice);
    case CALCULATION:
      return new CalculationNode(plan, slice);
    case SUBQUERY:
      return new SubqueryNode(plan, slice);
    case SORT: {
      SortElementVector elements;
      getSortElements(elements, plan, slice, "SortNode");
      return new SortNode(plan, slice, elements,
                          slice.get("stable").getBoolean());
    }
    case COLLECT: {
      Variable* expressionVariable = Variable::varFromVPack(
          plan->getAst(), slice, "expressionVariable", true);
      Variable* outVariable =
          Variable::varFromVPack(plan->getAst(), slice, "outVariable", true);

      // keepVariables
      std::vector<std::pair<Variable const*, std::string>> keepVariables;
      if (VPackSlice keepVariablesSlice = slice.get("keepVariables");
          keepVariablesSlice.isArray()) {
        for (VPackSlice it : VPackArrayIterator(keepVariablesSlice)) {
          Variable const* variable =
              Variable::varFromVPack(plan->getAst(), it, "variable");
          if (auto name = it.get("name"); name.isString()) {
            keepVariables.emplace_back(
                std::make_pair(variable, name.copyString()));
          } else {
            keepVariables.emplace_back(
                std::make_pair(variable, variable->name));
          }
        }
      }

      // groups
      VPackSlice groupsSlice = slice.get("groups");
      if (!groupsSlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                       "invalid \"groups\" definition");
      }

      std::vector<GroupVarInfo> groupVariables;
      {
        groupVariables.reserve(groupsSlice.length());
        for (VPackSlice it : VPackArrayIterator(groupsSlice)) {
          Variable* outVar =
              Variable::varFromVPack(plan->getAst(), it, "outVariable");
          Variable* inVar =
              Variable::varFromVPack(plan->getAst(), it, "inVariable");

          groupVariables.emplace_back(GroupVarInfo{outVar, inVar});
        }
      }

      // aggregates
      VPackSlice aggregatesSlice = slice.get("aggregates");
      if (!aggregatesSlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                       "invalid \"aggregates\" definition");
      }

      std::vector<AggregateVarInfo> aggregateVariables;
      {
        aggregateVariables.reserve(aggregatesSlice.length());
        for (VPackSlice it : VPackArrayIterator(aggregatesSlice)) {
          Variable* outVar =
              Variable::varFromVPack(plan->getAst(), it, "outVariable");
          Variable* inVar =
              Variable::varFromVPack(plan->getAst(), it, "inVariable", true);

          std::string type = it.get("type").copyString();
          aggregateVariables.emplace_back(
              AggregateVarInfo{outVar, inVar, std::move(type)});
        }
      }

      bool isDistinctCommand = slice.get("isDistinctCommand").getBoolean();

      auto node = new CollectNode(
          plan, slice, expressionVariable, outVariable, keepVariables,
          plan->getAst()->variables()->variables(false), groupVariables,
          aggregateVariables, isDistinctCommand);

      // specialize the node if required
      bool specialized = slice.get("specialized").getBoolean();
      if (specialized) {
        node->specialized();
      }

      return node;
    }
    case INSERT:
      return new InsertNode(plan, slice);
    case REMOVE:
      return new RemoveNode(plan, slice);
    case UPDATE:
      return new UpdateNode(plan, slice);
    case REPLACE:
      return new ReplaceNode(plan, slice);
    case UPSERT:
      return new UpsertNode(plan, slice);
    case RETURN:
      return new ReturnNode(plan, slice);
    case NORESULTS:
      return new NoResultsNode(plan, slice);
    case INDEX:
      return new IndexNode(plan, slice);
    case JOIN:
      return new JoinNode(plan, slice);
    case REMOTE:
      return new RemoteNode(plan, slice);
    case GATHER: {
      SortElementVector elements;
      getSortElements(elements, plan, slice, "GatherNode");
      return new GatherNode(plan, slice, elements);
    }
    case SCATTER:
      return new ScatterNode(plan, slice);
    case DISTRIBUTE:
      return new DistributeNode(plan, slice);
    case TRAVERSAL:
      return new TraversalNode(plan, slice);
    case SHORTEST_PATH:
      return new ShortestPathNode(plan, slice);
    case ENUMERATE_PATHS:
      return new EnumeratePathsNode(plan, slice);
    case REMOTESINGLE:
      return new SingleRemoteOperationNode(plan, slice);
    case REMOTE_MULTIPLE:
      return new MultipleRemoteModificationNode(plan, slice);
    case ENUMERATE_IRESEARCH_VIEW:
      return new iresearch::IResearchViewNode(*plan, slice);
    case SUBQUERY_START:
      return new SubqueryStartNode(plan, slice);
    case SUBQUERY_END:
      return new SubqueryEndNode(plan, slice);
    case DISTRIBUTE_CONSUMER:
      return new DistributeConsumerNode(plan, slice);
    case MATERIALIZE:
      return createMaterializeNode(plan, slice);
    case OFFSET_INFO_MATERIALIZE:
      return aql::createOffsetMaterializeNode(plan, slice);
    case ASYNC:
      return new AsyncNode(plan, slice);
    case MUTEX:
      return new MutexNode(plan, slice);
    case WINDOW: {
      Variable* rangeVar = Variable::varFromVPack(
          plan->getAst(), slice, "rangeVariable", /*optional*/ true);

      // aggregates
      VPackSlice aggregatesSlice = slice.get("aggregates");
      if (!aggregatesSlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                       "invalid \"aggregates\" definition");
      }

      std::vector<AggregateVarInfo> aggregateVariables;
      {
        aggregateVariables.reserve(aggregatesSlice.length());
        for (VPackSlice it : VPackArrayIterator(aggregatesSlice)) {
          Variable* outVar =
              Variable::varFromVPack(plan->getAst(), it, "outVariable");
          Variable* inVar =
              Variable::varFromVPack(plan->getAst(), it, "inVariable", true);

          std::string const type = it.get("type").copyString();
          aggregateVariables.emplace_back(
              AggregateVarInfo{outVar, inVar, type});
        }
      }

      auto type = rangeVar != nullptr ? WindowBounds::Type::Range
                                      : WindowBounds::Type::Row;
      WindowBounds bounds(type, slice);
      return new WindowNode(plan, slice, std::move(bounds), rangeVar,
                            aggregateVariables);
    }
    default: {
      // should not reach this point
      TRI_ASSERT(false);
    }
  }
  return nullptr;
}

/// @brief create an ExecutionNode from VPackSlice
ExecutionNode::ExecutionNode(ExecutionPlan* plan, velocypack::Slice slice)
    : _id(slice.get("id").getNumericValue<size_t>()),
      _depth(slice.get("depth").getNumericValue<unsigned int>()),
      _varUsageValid(true),
      _isInSplicedSubquery(false),
      _plan(plan) {
  TRI_ASSERT(_registerPlan == nullptr);
  _registerPlan = std::make_shared<RegisterPlan>(slice, _depth);

  VPackSlice regsToClearList = slice.get("regsToClear");
  if (!regsToClearList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "\"regsToClear\" needs to be an array");
  }

  _regsToClear.reserve(regsToClearList.length());
  for (VPackSlice it : VPackArrayIterator(regsToClearList)) {
    _regsToClear.insert(RegisterId(it.getNumericValue<RegisterId::value_t>()));
  }

  auto allVars = plan->getAst()->variables();

  VPackSlice varsUsedLaterStackSlice = slice.get("varsUsedLaterStack");

  if (varsUsedLaterStackSlice.isArray()) {
    _varsUsedLaterStack.reserve(varsUsedLaterStackSlice.length());
    for (auto stackEntrySlice : VPackArrayIterator(varsUsedLaterStackSlice)) {
      if (!stackEntrySlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL_AQL,
            "\"varsUsedLaterStack\" needs to contain arrays");
      }
      auto& varsUsedLater = _varsUsedLaterStack.emplace_back();

      varsUsedLater.reserve(stackEntrySlice.length());
      for (auto it : VPackArrayIterator(stackEntrySlice)) {
        Variable oneVarUsedLater(it, plan->getAst()->query().resourceMonitor());
        Variable* oneVariable = allVars->getVariable(oneVarUsedLater.id);

        if (oneVariable == nullptr) {
          std::string errmsg =
              "varsUsedLaterStack: ID not found in all-array: " +
              StringUtils::itoa(oneVarUsedLater.id);
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL, errmsg);
        }
        varsUsedLater.insert(oneVariable);
      }
    }
  } else if (!varsUsedLaterStackSlice.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL_AQL,
        "\"varsUsedLaterStack\" needs to be a non-empty array");
  }

  VPackSlice varsValidStackSlice = slice.get("varsValidStack");

  if (varsValidStackSlice.isArray()) {
    _varsValidStack.reserve(varsValidStackSlice.length());
    for (auto stackEntrySlice : VPackArrayIterator(varsValidStackSlice)) {
      if (!stackEntrySlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL_AQL,
            "\"varsValidStack\" needs to contain arrays");
      }
      auto& varsValid = _varsValidStack.emplace_back();

      varsValid.reserve(stackEntrySlice.length());
      for (VPackSlice it : VPackArrayIterator(stackEntrySlice)) {
        Variable oneVarValid(it, plan->getAst()->query().resourceMonitor());
        Variable* oneVariable = allVars->getVariable(oneVarValid.id);

        if (oneVariable == nullptr) {
          std::string errmsg = "varsValidStack: ID not found in all-array: " +
                               StringUtils::itoa(oneVarValid.id);
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL, errmsg);
        }
        varsValid.insert(oneVariable);
      }
    }
  } else if (!varsValidStackSlice.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL_AQL,
        "\"varsValidStack\" needs to be a non-empty array");
  }

  VPackSlice regsToKeepStackSlice = slice.get("regsToKeepStack");

  if (regsToKeepStackSlice.isArray()) {
    // || regsToKeepStackSlice.length() == 0) {
    _regsToKeepStack.reserve(regsToKeepStackSlice.length());
    for (auto stackEntrySlice : VPackArrayIterator(regsToKeepStackSlice)) {
      if (!stackEntrySlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL_AQL,
            "\"regsToKeepStack\" needs to contain arrays");
      }
      auto& regsToKeep = _regsToKeepStack.emplace_back();

      regsToKeep.reserve(stackEntrySlice.length());
      for (VPackSlice it : VPackArrayIterator(stackEntrySlice)) {
        regsToKeep.insert(
            RegisterId(it.getNumericValue<RegisterId::value_t>()));
      }
    }
  } else if (!regsToKeepStackSlice.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL_AQL,
        "\"regsToKeepStack\" needs to be a non-empty array");
  }

  _isInSplicedSubquery =
      VelocyPackHelper::getBooleanValue(slice, "isInSplicedSubquery", false);

  _isAsyncPrefetchEnabled =
      VelocyPackHelper::getBooleanValue(slice, "isAsyncPrefetchEnabled", false);

  _isCallstackSplitEnabled = VelocyPackHelper::getBooleanValue(
      slice, "isCallstackSplitEnabled", false);

  if (_isAsyncPrefetchEnabled) {
    plan->getAst()->setContainsAsyncPrefetch();
  }
}

ExecutionNode::ExecutionNode(ExecutionPlan& plan, ExecutionNode const& other)
    : ExecutionNode(&plan, other.id()) {
  TRI_ASSERT(&plan == other.plan());
  other.cloneWithoutRegisteringAndDependencies(plan, *this);
}

/// @brief exports this ExecutionNode with all its dependencies to VelocyPack.
/// This function implicitly creates an array and serializes all nodes top-down,
/// i.e., the upmost dependency will be the first, and this node will be the
/// last in the array.
void ExecutionNode::allToVelocyPack(velocypack::Builder& builder,
                                    unsigned flags) const {
  struct NodeSerializer
      : WalkerWorker<ExecutionNode, WalkerUniqueness::Unique> {
    NodeSerializer(VPackBuilder& builder, unsigned flags)
        : builder(builder), flags(flags) {}
    void after(ExecutionNode* n) override { n->toVelocyPack(builder, flags); }
    VPackBuilder& builder;
    unsigned flags;
  } nodeSerializer{builder, flags};

  VPackArrayBuilder guard(&builder);
  const_cast<ExecutionNode*>(this)->flatWalk(nodeSerializer, true);
}

/// @brief execution Node clone utility to be called by derived classes
/// @return pointer to a registered node owned by a plan
ExecutionNode* ExecutionNode::cloneHelper(std::unique_ptr<ExecutionNode> other,
                                          bool withDependencies) const {
  ExecutionPlan* plan = other->plan();

  cloneWithoutRegisteringAndDependencies(*plan, *other);

  auto* registeredNode = plan->registerNode(std::move(other));

  if (withDependencies) {
    cloneDependencies(plan, registeredNode);
  }

  return registeredNode;
}

void ExecutionNode::cloneWithoutRegisteringAndDependencies(
    ExecutionPlan& plan, ExecutionNode& other) const {
  if (&plan == _plan) {
    // same execution plan for source and target
    // now assign a new id to the cloned node, otherwise it will fail
    // upon node registration and/or its meaning is ambiguous
    other.setId(plan.nextId());
  }

  other._regsToClear = _regsToClear;
  other._depth = _depth;
  other._varUsageValid = _varUsageValid;
  other._isInSplicedSubquery = _isInSplicedSubquery;

  // point to current AST -> don't do deep copies.
  other._varsUsedLaterStack = _varsUsedLaterStack;
  other._varsValidStack = _varsValidStack;
  other._registerPlan = _registerPlan;
}

/// @brief helper for cloning, use virtual clone methods for dependencies
void ExecutionNode::cloneDependencies(ExecutionPlan* plan,
                                      ExecutionNode* theClone) const {
  TRI_ASSERT(theClone != nullptr);
  auto it = _dependencies.begin();
  while (it != _dependencies.end()) {
    auto c = (*it)->clone(plan, true);
    TRI_ASSERT(c != nullptr);
    try {
      c->_parents.emplace_back(theClone);
      theClone->_dependencies.emplace_back(c);
    } catch (...) {
      delete c;
      throw;
    }
    ++it;
  }
}

void ExecutionNode::cloneRegisterPlan(ExecutionNode* dependency) {
  TRI_ASSERT(hasDependency());
  TRI_ASSERT(getFirstDependency() == dependency);
  _registerPlan = dependency->getRegisterPlan();
  _depth = dependency->getDepth();
  setVarsUsedLater(dependency->getVarsUsedLaterStack());
  setVarsValid(dependency->getVarsValidStack());
  setVarUsageValid();
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void ExecutionNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& /*replacements*/) {
  // default implementation does nothing
}

void ExecutionNode::replaceAttributeAccess(
    ExecutionNode const* /*self*/, Variable const* /*searchVariable*/,
    std::span<std::string_view> /*attribute*/,
    Variable const* /*replaceVariable*/, size_t /*index*/) {
  // default implementation does nothing
}

bool ExecutionNode::isEqualTo(ExecutionNode const& other) const {
  return ((this->getType() == other.getType()) && (_id == other._id) &&
          (_depth == other._depth) &&
          (isInSplicedSubquery() == other.isInSplicedSubquery()) &&
          (_dependencies.size() == other._dependencies.size()) &&
          (std::equal(_dependencies.begin(), _dependencies.end(),
                      other._dependencies.begin(),
                      [](ExecutionNode* const l, ExecutionNode* const r) {
                        return l->isEqualTo(*r);
                      })));
}

/// @brief invalidate the cost estimation for the node and its dependencies
void ExecutionNode::invalidateCost() {
  struct CostInvalidator : WalkerWorkerBase<ExecutionNode> {
    void after(ExecutionNode* n) override { n->_costEstimate.invalidate(); }
  } invalidator;
  this->walk(invalidator);
}

/// @brief estimate the cost of the node . . .
/// does not recalculate the estimate if already calculated
CostEstimate ExecutionNode::getCost() const {
  if (!_costEstimate.isValid()) {
    // Use a walker to estimate cost of all direct and indirect dependencies.
    // This is necessary to avoid deeply nested recursive calls in
    // estimateCosts which could result in a stack overflow.
    struct CostEstimator : WalkerWorkerBase<ExecutionNode> {
      void after(ExecutionNode* n) override {
        if (!n->_costEstimate.isValid()) {
          n->_costEstimate = n->estimateCost();
        }
      }
    } estimator;
    const_cast<ExecutionNode*>(this)->walk(estimator);
  }
  TRI_ASSERT(_costEstimate.estimatedCost >= 0.0);
  TRI_ASSERT(_costEstimate.isValid());
  return _costEstimate;
}

/// @brief functionality to walk an execution plan recursively
bool ExecutionNode::walk(WalkerWorkerBase<ExecutionNode>& worker) {
  return doWalk(worker, false, FlattenType::NONE);
}

/// @brief functionality to walk an execution plan recursively.
/// This variant of walk(), when visiting a node,
///  - first, when at a SubqueryNode, recurses into its subquery
///  - after that recurses on its dependencies.
/// This is in contrast to walk(), which recurses on the dependencies before
/// recursing into the subquery.
bool ExecutionNode::walkSubqueriesFirst(
    WalkerWorkerBase<ExecutionNode>& worker) {
  return doWalk(worker, true, FlattenType::NONE);
}

bool ExecutionNode::flatWalk(WalkerWorkerBase<ExecutionNode>& worker,
                             bool onlyFlattenAsync) {
  if (onlyFlattenAsync) {
    return doWalk(worker, false, FlattenType::INLINE_ASYNC);
  }
  return doWalk(worker, false, FlattenType::INLINE_ALL);
}

bool ExecutionNode::doWalk(WalkerWorkerBase<ExecutionNode>& worker,
                           bool subQueryFirst, FlattenType flattenType) {
  enum class State { Pending, Processed, InSubQuery };

  using Entry = std::pair<ExecutionNode*, State>;
  // we can be quite generous with the buffer size since the implementation is
  // not recursive.
  constexpr std::size_t NumBufferedEntries = 1000;
  containers::SmallVector<Entry, NumBufferedEntries> nodes;
  nodes.emplace_back(const_cast<ExecutionNode*>(this), State::Pending);

  auto enqueDependencies = [&nodes](std::vector<ExecutionNode*>& deps) {
    // we enqueue the dependencies in reversed order, because we always
    // continue with the _last_ node in the list.
    for (auto it = deps.rbegin(); it != deps.rend(); ++it) {
      nodes.emplace_back(*it, State::Pending);
    }
  };

  constexpr std::size_t NumJumpBackPointers = 10;
  struct ParallelEntry {
    explicit ParallelEntry(ExecutionNode* n) : _node(n), _nextVisit(1) {}

    /// @brief Indicates that we have now visited all parallel paths
    bool isComplete() const {
      return _node->getDependencies().size() <= _nextVisit;
    }

    /// @brief Return the Next child to visit
    ExecutionNode* visitNext() {
      TRI_ASSERT(!isComplete());
      return _node->getDependencies().at(_nextVisit++);
    }

   private:
    ExecutionNode* _node;
    size_t _nextVisit;
  };

  containers::SmallVector<ParallelEntry, NumJumpBackPointers> parallelStarter;

  while (!nodes.empty()) {
    auto& [n, state] = nodes.back();
    switch (state) {
      case State::Pending: {
        if (flattenType != FlattenType::NONE && n->getParents().size() > 1) {
          // This assert is not strictly necessary,
          // by the time this code was implemented only
          // SCATTER, MUTEX and DISTRIBUTE nodes were allowed to have more
          // than one parent, so all others indicated an issue on plan
          TRI_ASSERT(n->getType() == SCATTER || n->getType() == MUTEX ||
                     n->getType() == DISTRIBUTE);
          if (flattenType == FlattenType::INLINE_ALL || n->getType() == MUTEX) {
            ADB_PROD_ASSERT(!parallelStarter.empty());
            // If we are not INLINE_ALL, only flatten the MUTEX, which is the
            // counterpart of ASYNC
            auto& starter = parallelStarter.back();
            if (starter.isComplete()) {
              // All parallel steps complete drop the corresponding parallel
              // starter
              parallelStarter.pop_back();
            } else {
              // Discard this state, we will visit it in the next time around
              // on the parallel branch
              nodes.pop_back();
              // Jump back, and continue visiting the next parallel branch.
              nodes.emplace_back(starter.visitNext(), State::Pending);
              // Just pretend we have not seen this
              // and continue with the next one to visit
              continue;
            }
          }
        }
        if (worker.done(n)) {
          nodes.pop_back();
          break;
        }

        if (worker.before(n)) {
          return true;
        }

        if (subQueryFirst && n->getType() == SUBQUERY) {
          auto p = ExecutionNode::castTo<SubqueryNode*>(n);
          auto subquery = p->getSubquery();

          if (worker.enterSubquery(n, subquery)) {
            state = State::InSubQuery;
            nodes.emplace_back(subquery, State::Pending);
            break;
          }
        }
        state = State::Processed;
        if (flattenType == FlattenType::NONE ||
            n->getDependencies().size() <= 1) {
          enqueDependencies(n->_dependencies);
        } else {
          if (n->getDependencies().size() > 1) {
            // We have a multi-dependency node, that we need to handle

            // This assert is not strictly necessary,
            // by the time this code was implemented only
            // GATHER nodes were allowed to have more than
            // one dependency, so all others indicated an issue on plan
            TRI_ASSERT(n->getType() == GATHER);
            if (flattenType == FlattenType::INLINE_ALL) {
              // Remember where we started the flattening process
              parallelStarter.emplace_back(n);
              // Add the next dependency to continue
              nodes.emplace_back(n->getFirstDependency(), State::Pending);
            } else {
              TRI_ASSERT(flattenType == FlattenType::INLINE_ASYNC);
              auto peek = n->getFirstDependency();
              if (peek->getType() == ASYNC) {
                // Only Async nodes shall be flattened
                // So yes we found the combination we want to falten here

                // Remember where we started the flattening process
                parallelStarter.emplace_back(n);
                // Add the next dependency to continue
                nodes.emplace_back(n->getFirstDependency(), State::Pending);
              } else {
                // Non-Async gather, do not flatten.
                enqueDependencies(n->_dependencies);
              }
            }
          }
        }
        break;
      }

      case State::Processed: {
        if (!subQueryFirst && n->getType() == SUBQUERY) {
          auto p = ExecutionNode::castTo<SubqueryNode*>(n);
          auto subquery = p->getSubquery();

          if (worker.enterSubquery(n, subquery)) {
            state = State::InSubQuery;
            nodes.emplace_back(subquery, State::Pending);
            break;
          }
        }
        worker.after(n);
        nodes.pop_back();
        break;
      }

      case State::InSubQuery: {
        TRI_ASSERT(n->getType() == SUBQUERY);
        auto p = ExecutionNode::castTo<SubqueryNode*>(n);
        worker.leaveSubquery(n, p->getSubquery());
        if (subQueryFirst) {
          state = State::Processed;
          enqueDependencies(n->_dependencies);
        } else {
          worker.after(n);
          nodes.pop_back();
        }
        break;
      }
    }
  }

  return false;
}

/// @brief get the surrounding loop
ExecutionNode const* ExecutionNode::getLoop() const {
  auto node = this;
  while (node != nullptr) {
    if (!node->hasDependency()) {
      return nullptr;
    }

    node = node->getFirstDependency();
    TRI_ASSERT(node != nullptr);

    auto type = node->getType();

    if (type == ENUMERATE_COLLECTION || type == INDEX || type == TRAVERSAL ||
        type == ENUMERATE_LIST || type == SHORTEST_PATH ||
        type == ENUMERATE_PATHS || type == ENUMERATE_IRESEARCH_VIEW) {
      return node;
    }
  }

  return nullptr;
}

/// @brief serialize this ExecutionNode to VelocyPack
void ExecutionNode::toVelocyPack(velocypack::Builder& builder,
                                 unsigned flags) const {
  VPackObjectBuilder objectGuard(&builder);

  builder.add("type", VPackValue(getTypeString()));
  if (flags & ExecutionNode::SERIALIZE_DETAILS) {
    builder.add("typeID", VPackValue(static_cast<int>(getType())));
  }

  {
    VPackArrayBuilder guard(&builder, "dependencies");
    for (auto const& it : _dependencies) {
      builder.add(VPackValue(it->id().id()));
    }
  }
  builder.add("id", VPackValue(id().id()));
  if (flags & ExecutionNode::SERIALIZE_PARENTS) {
    VPackArrayBuilder guard(&builder, "parents");
    for (auto const& it : _parents) {
      builder.add(VPackValue(it->id().id()));
    }
  }
  if (flags & ExecutionNode::SERIALIZE_ESTIMATES) {
    CostEstimate estimate = getCost();
    builder.add("estimatedCost", VPackValue(estimate.estimatedCost));
    builder.add("estimatedNrItems", VPackValue(estimate.estimatedNrItems));
  }

  // SERIALIZE_REGISTER_INFORMATION => SERIALIZE_DETAILS
  TRI_ASSERT(!ExecutionNode::SERIALIZE_REGISTER_INFORMATION ||
             ExecutionNode::SERIALIZE_DETAILS);
  if (flags & (ExecutionNode::SERIALIZE_DETAILS |
               ExecutionNode::SERIALIZE_REGISTER_INFORMATION)) {
    builder.add("depth", VPackValue(_depth));

    if (_registerPlan) {
      _registerPlan->toVelocyPack(builder);
    } else {
      RegisterPlan::toVelocyPackEmpty(builder);
    }

    {
      VPackArrayBuilder guard(&builder, "regsToClear");
      for (auto const& oneRegisterID : _regsToClear) {
        TRI_ASSERT(oneRegisterID.isRegularRegister());
        builder.add(VPackValue(oneRegisterID.value()));
      }
    }

    builder.add(VPackValue("varsUsedLaterStack"));
    {
      VPackArrayBuilder guard(&builder);
      TRI_ASSERT(!_varsUsedLaterStack.empty());
      for (auto const& stackEntry : _varsUsedLaterStack) {
        VPackArrayBuilder stackEntryGuard(&builder);
        for (auto const& oneVar : stackEntry) {
          oneVar->toVelocyPack(builder);
        }
      }
    }

    builder.add(VPackValue("regsToKeepStack"));
    {
      VPackArrayBuilder guard(&builder);
      for (auto const& stackEntry : getRegsToKeepStack()) {
        VPackArrayBuilder stackEntryGuard(&builder);
        for (auto const& reg : stackEntry) {
          TRI_ASSERT(reg.isRegularRegister());
          builder.add(VPackValue(reg.value()));
        }
      }
    }

    builder.add(VPackValue("varsValidStack"));
    {
      VPackArrayBuilder guard(&builder);
      TRI_ASSERT(!_varsValidStack.empty());
      for (auto const& stackEntry : _varsValidStack) {
        VPackArrayBuilder stackEntryGuard(&builder);
        for (auto const& oneVar : stackEntry) {
          oneVar->toVelocyPack(builder);
        }
      }
    }

    if (flags & ExecutionNode::SERIALIZE_REGISTER_INFORMATION) {
      builder.add(VPackValue("unusedRegsStack"));
      auto const& unusedRegsStack = _registerPlan->unusedRegsByNode.at(id());
      {
        VPackArrayBuilder guard(&builder);
        TRI_ASSERT(!unusedRegsStack.empty());
        for (auto const& stackEntry : unusedRegsStack) {
          VPackArrayBuilder stackEntryGuard(&builder);
          for (auto const& reg : stackEntry) {
            TRI_ASSERT(reg.isRegularRegister());
            builder.add(VPackValue(reg.value()));
          }
        }
      }

      builder.add(VPackValue("regVarMapStack"));
      auto const& regVarMapStack = _registerPlan->regVarMapStackByNode.at(id());
      {
        VPackArrayBuilder guard(&builder);
        TRI_ASSERT(!regVarMapStack.empty());
        for (auto const& stackEntry : regVarMapStack) {
          VPackObjectBuilder stackEntryGuard(&builder);
          for (auto const& reg : stackEntry) {
            using std::to_string;
            builder.add(VPackValue(to_string(reg.first.value())));
            reg.second->toVelocyPack(builder);
          }
        }
      }

      builder.add(VPackValue("varsSetHere"));
      {
        VPackArrayBuilder guard(&builder);
        auto const& varsSetHere = getVariablesSetHere();
        for (auto const& oneVar : varsSetHere) {
          oneVar->toVelocyPack(builder);
        }
      }

      builder.add(VPackValue("varsUsedHere"));
      {
        VPackArrayBuilder guard(&builder);
        auto varsUsedHere = VarSet{};
        getVariablesUsedHere(varsUsedHere);
        for (auto const& oneVar : varsUsedHere) {
          oneVar->toVelocyPack(builder);
        }
      }
    }

    builder.add("isInSplicedSubquery", VPackValue(_isInSplicedSubquery));
  }

  // serialize these flags in all modes, but only if they are enabled.
  // this works because they default to false, and helps to keep the output
  // small.
  if (_isAsyncPrefetchEnabled) {
    builder.add("isAsyncPrefetchEnabled", VPackValue(true));
  }
  if (_isCallstackSplitEnabled) {
    builder.add("isCallstackSplitEnabled", VPackValue(true));
  }

  TRI_ASSERT(builder.isOpenObject());
  doToVelocyPack(builder, flags);
}

/// @brief static analysis debugger
#if 0
struct RegisterPlanningDebugger final : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  RegisterPlanningDebugger()
    : indent(0) {
  }

  ~RegisterPlanningDebugger () = default;

  int indent;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    indent++;
    return true;
  }

  void leaveSubquery(ExecutionNode*, ExecutionNode*) override final {
    indent--;
  }

  void after(ExecutionNode* ep) override final {
    for (int i = 0; i < indent; i++) {
      std::cout << " ";
    }
    std::cout << ep->getTypeString() << " ";
    std::cout << "regsUsedHere: ";
    VarSet variablesUsedHere;
    ep->getVariablesUsedHere(variablesUsedHere);
    for (auto const& v : variablesUsedHere) {
      std::cout << ep->getRegisterPlan()->varInfo.find(v->id)->second.registerId
                << " ";
    }
    std::cout << "regsSetHere: ";
    for (auto const& v : ep->getVariablesSetHere()) {
      std::cout << ep->getRegisterPlan()->varInfo.find(v->id)->second.registerId
                << " ";
    }
    std::cout << "regsToClear: ";
    for (auto const& r : ep->getRegsToClear()) {
      std::cout << r << " ";
    }
    std::cout << std::endl;
  }
};

#endif

/// @brief planRegisters
void ExecutionNode::planRegisters(ExplainRegisterPlan explainRegisterPlan) {
  auto v = std::make_shared<RegisterPlan>();
  auto walker = RegisterPlanWalker{v, explainRegisterPlan};
  walk(walker);

  // shrink RegisterPlan by cutting off all unused rightmost registers
  // from each depth. this is a completely optional performance
  // optimization. turning it off should not affect correctness,
  // only performance.
  v->shrink(this);
}

bool ExecutionNode::isInSplicedSubquery() const noexcept {
  return _isInSplicedSubquery;
}

void ExecutionNode::setIsInSplicedSubquery(bool const value) noexcept {
  _isInSplicedSubquery = value;
}

/// @brief replace a dependency, returns true if the pointer was found and
/// replaced, please note that this does not delete oldNode!
bool ExecutionNode::replaceDependency(ExecutionNode* oldNode,
                                      ExecutionNode* newNode) {
  TRI_ASSERT(oldNode != nullptr);
  TRI_ASSERT(newNode != nullptr);

  auto it = _dependencies.begin();

  while (it != _dependencies.end()) {
    if (*it == oldNode) {
      *it = newNode;
      try {
        newNode->_parents.emplace_back(this);
      } catch (...) {
        *it = oldNode;  // roll back
        return false;
      }
      try {
        for (auto it2 = oldNode->_parents.begin();
             it2 != oldNode->_parents.end(); ++it2) {
          if (*it2 == this) {
            oldNode->_parents.erase(it2);
            break;
          }
        }
      } catch (...) {
        // If this happens, we ignore that the _parents of oldNode
        // are not set correctly
      }
      return true;
    }

    ++it;
  }
  return false;
}

/// @brief remove a dependency, returns true if the pointer was found and
/// removed, please note that this does not delete ep!
bool ExecutionNode::removeDependency(ExecutionNode* ep) {
  bool ok = false;
  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    if (*it == ep) {
      try {
        it = _dependencies.erase(it);
      } catch (...) {
        return false;
      }
      ok = true;
      break;
    }
  }

  if (!ok) {
    return false;
  }

  // Now remove us as a parent of the old dependency as well:
  for (auto it = ep->_parents.begin(); it != ep->_parents.end(); ++it) {
    if (*it == this) {
      try {
        ep->_parents.erase(it);
      } catch (...) {
      }
      return true;
    }
  }

  return false;
}

/// @brief remove all dependencies for the given node
void ExecutionNode::removeDependencies() {
  for (auto& x : _dependencies) {
    for (auto it = x->_parents.begin(); it != x->_parents.end();
         /* no hoisting */) {
      if (*it == this) {
        try {
          it = x->_parents.erase(it);
        } catch (...) {
        }
        break;
      } else {
        ++it;
      }
    }
  }
  _dependencies.clear();
}

RegisterId ExecutionNode::variableToRegisterId(Variable const* variable) const {
  return getRegisterPlan()->variableToRegisterId(variable);
}

// This is the general case and will not work if e.g. there is no predecessor.
RegisterInfos ExecutionNode::createRegisterInfos(
    RegIdSet readableInputRegisters, RegIdSet writableOutputRegisters) const {
  auto const nrOutRegs = getNrOutputRegisters();
  auto const nrInRegs = getNrInputRegisters();

  auto regsToKeep = getRegsToKeepStack();
  auto regsToClear = getRegsToClear();

  return RegisterInfos{std::move(readableInputRegisters),
                       std::move(writableOutputRegisters),
                       nrInRegs,
                       nrOutRegs,
                       std::move(regsToClear),
                       std::move(regsToKeep)};
}

RegisterCount ExecutionNode::getNrInputRegisters() const {
  ExecutionNode const* previousNode = getFirstDependency();
  // in case of the SingletonNode, there are no input registers
  TRI_ASSERT(previousNode != nullptr || getType() == ExecutionNode::SINGLETON);
  return previousNode == nullptr
             ? getNrOutputRegisters()
             : getRegisterPlan()->nrRegs[previousNode->getDepth()];
}

auto ExecutionNode::getRegsToKeepStack() const -> RegIdSetStack {
  if (_regsToKeepStack.empty()) {
    return _registerPlan->calcRegsToKeep(_varsUsedLaterStack, _varsValidStack,
                                         getVariablesSetHere());
  }
  return _regsToKeepStack;
}

RegisterCount ExecutionNode::getNrOutputRegisters() const {
  if (getType() == ExecutionNode::RETURN) {
    // Special case for RETURN, which usually writes only 1 register.
    // TODO We should be able to remove this when the new register planning is
    // ready!
    auto returnNode = castTo<ReturnNode const*>(this);
    if (!returnNode->returnInheritedResults()) {
      return 1;
    }
  }

  return getRegisterPlan()->nrRegs[getDepth()];
}

ExecutionNode::ExecutionNode(ExecutionPlan* plan, ExecutionNodeId id)
    : _id(id),
      _depth(0),
      _varUsageValid(false),
      _isInSplicedSubquery(false),
      _plan(plan) {}

ExecutionNodeId ExecutionNode::id() const { return _id; }

void ExecutionNode::removeRegistersGreaterThan(RegisterId maxRegister) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto assertNotContained = [](RegisterId maxRegister,
                               auto const& container) noexcept {
    std::for_each(container.begin(), container.end(),
                  [maxRegister](RegisterId regId) {
                    TRI_ASSERT(regId.value() <= maxRegister.value());
                  });
  };
  assertNotContained(maxRegister, _regsToClear);
  // validate all levels of _regsToKeepStack
  for (auto const& regsToKeep : _regsToKeepStack) {
    assertNotContained(maxRegister, regsToKeep);
  }
#endif

  auto removeRegisters = [maxRegister](auto& container) {
    for (auto it = container.begin(); it != container.end();
         /* no hoisting */) {
      if ((*it) > maxRegister) {
        it = container.erase(it);
      } else {
        ++it;
      }
    }
  };

  auto it = _registerPlan->unusedRegsByNode.find(_id);
  if (it != _registerPlan->unusedRegsByNode.end()) {
    removeRegisters(it->second.back());
  }
}

void ExecutionNode::swapFirstDependency(ExecutionNode* node) {
  TRI_ASSERT(hasDependency());
  _dependencies[0] = node;
}

std::vector<ExecutionNode*> const& ExecutionNode::getDependencies() const {
  return _dependencies;
}

ExecutionNode* ExecutionNode::getFirstDependency() const {
  if (_dependencies.empty()) {
    return nullptr;
  }
  TRI_ASSERT(_dependencies[0] != nullptr);
  return _dependencies[0];
}

bool ExecutionNode::hasDependency() const {
  return (_dependencies.size() == 1);
}

void ExecutionNode::dependencies(std::vector<ExecutionNode*>& result) const {
  for (auto const& it : _dependencies) {
    TRI_ASSERT(it != nullptr);
    result.emplace_back(it);
  }
}

std::vector<ExecutionNode*> ExecutionNode::getParents() const {
  return _parents;
}

bool ExecutionNode::hasParent() const { return (_parents.size() == 1); }

ExecutionNode* ExecutionNode::getFirstParent() const {
  if (_parents.empty()) {
    return nullptr;
  }
  TRI_ASSERT(_parents[0] != nullptr);
  return _parents[0];
}

void ExecutionNode::parents(std::vector<ExecutionNode*>& result) const {
  for (auto const& it : _parents) {
    TRI_ASSERT(it != nullptr);
    result.emplace_back(it);
  }
}

ExecutionNode const* ExecutionNode::getSingleton() const {
  auto node = this;
  do {
    node = node->getFirstDependency();
  } while (node != nullptr && node->getType() != SINGLETON);

  return node;
}

ExecutionNode* ExecutionNode::getSingleton() {
  auto node = this;
  do {
    node = node->getFirstDependency();
  } while (node != nullptr && node->getType() != SINGLETON);

  return node;
}

void ExecutionNode::getDependencyChain(std::vector<ExecutionNode*>& result,
                                       bool includeSelf) {
  auto current = this;
  while (current != nullptr) {
    if (includeSelf || current != this) {
      result.emplace_back(current);
    }
    current = current->getFirstDependency();
  }
}

void ExecutionNode::setParent(ExecutionNode* p) {
  _parents.clear();
  _parents.emplace_back(p);
}

void ExecutionNode::getVariablesUsedHere(VarSet&) const {
  // do nothing!
}

std::vector<Variable const*> ExecutionNode::getVariablesSetHere() const {
  return std::vector<Variable const*>();
}

::arangodb::containers::HashSet<VariableId>
ExecutionNode::getVariableIdsUsedHere() const {
  VarSet vars;
  getVariablesUsedHere(vars);

  ::arangodb::containers::HashSet<VariableId> ids;
  for (auto& it : vars) {
    ids.emplace(it->id);
  }
  return ids;
}

bool ExecutionNode::setsVariable(VarSet const& which) const {
  for (auto const& v : getVariablesSetHere()) {
    if (which.find(v) != which.end()) {
      return true;
    }
  }
  return false;
}

void ExecutionNode::setVarUsageValid() { _varUsageValid = true; }

void ExecutionNode::invalidateVarUsage() {
  _varsUsedLaterStack.clear();
  _varsValidStack.clear();
  _varUsageValid = false;
}

bool ExecutionNode::isDeterministic() { return true; }

bool ExecutionNode::isModificationNode() const {
  // derived classes can change this
  return false;
}

AsyncPrefetchEligibility ExecutionNode::canUseAsyncPrefetching()
    const noexcept {
  // async prefetching is disabled by default.
  // dedicated node types can override this for themselves.
  return AsyncPrefetchEligibility::kDisableForNode;
}

ExecutionPlan const* ExecutionNode::plan() const { return _plan; }

ExecutionPlan* ExecutionNode::plan() { return _plan; }

std::shared_ptr<RegisterPlan> ExecutionNode::getRegisterPlan() const {
  TRI_ASSERT(_registerPlan != nullptr);
  return _registerPlan;
}

int ExecutionNode::getDepth() const { return _depth; }

RegIdSet const& ExecutionNode::getRegsToClear() const {
  if (getType() == RETURN) {
    auto* returnNode = castTo<ReturnNode const*>(this);
    if (!returnNode->returnInheritedResults()) {
      static const decltype(_regsToClear) emptyRegsToClear{};
      return emptyRegsToClear;
    }
  }

  return _regsToClear;
}

bool ExecutionNode::isVarUsedLater(Variable const* variable) const noexcept {
  TRI_ASSERT(_varUsageValid);
  return getVarsUsedLater().contains(variable);
}

bool ExecutionNode::isInInnerLoop() const { return getLoop() != nullptr; }

void ExecutionNode::setId(ExecutionNodeId id) { _id = id; }

void ExecutionNode::setRegsToClear(RegIdSet toClear) {
  _regsToClear = std::move(toClear);
}

void ExecutionNode::setRegsToKeep(RegIdSetStack toKeep) {
  _regsToKeepStack = std::move(toKeep);
}

RegisterId ExecutionNode::variableToRegisterOptionalId(
    Variable const* var) const {
  if (var) {
    return variableToRegisterId(var);
  }
  return RegisterId{RegisterId::maxRegisterId};
}

bool ExecutionNode::isIncreaseDepth() const {
  return isIncreaseDepth(getType());
}

bool ExecutionNode::isIncreaseDepth(ExecutionNode::NodeType type) {
  switch (type) {
    case ENUMERATE_COLLECTION:
    case INDEX:
    case ENUMERATE_LIST:
    case COLLECT:

    case TRAVERSAL:
    case SHORTEST_PATH:
    case ENUMERATE_PATHS:

    case REMOTESINGLE:
    case ENUMERATE_IRESEARCH_VIEW:
    case MATERIALIZE:

    case SUBQUERY_START:
    case SUBQUERY_END:
      return true;

    default:
      return false;
  }
}

bool ExecutionNode::alwaysCopiesRows(NodeType type) {
  // TODO This can be improved. And probably should be renamed.
  //      It is used in the register planning to discern whether we may reuse
  //      an input register immediately as an output register at this very
  //      block.
  switch (type) {
    case ENUMERATE_COLLECTION:
    case ENUMERATE_LIST:
    case FILTER:
    case SORT:
    case COLLECT:
    case INSERT:
    case REMOVE:
    case REPLACE:
    case UPDATE:
    case NORESULTS:
    case DISTRIBUTE:
    case UPSERT:
    case TRAVERSAL:
    case INDEX:
    case JOIN:
    case SHORTEST_PATH:
    case ENUMERATE_PATHS:
    case REMOTESINGLE:
    case REMOTE_MULTIPLE:
    case ENUMERATE_IRESEARCH_VIEW:
    case DISTRIBUTE_CONSUMER:
    case SUBQUERY_START:
    case SUBQUERY_END:
    case MATERIALIZE:
    case OFFSET_INFO_MATERIALIZE:
    case RETURN:
      return true;
    case CALCULATION:
    case SUBQUERY:
    case SINGLETON:
    case LIMIT:
    case WINDOW:
      return false;
    // It should be safe to return false for these, but is it necessary?
    // Returning true can lead to more efficient register usage.
    case REMOTE:  // simon: could probably return true
    case SCATTER:
    case GATHER:
    case ASYNC:
    case MUTEX:
      return false;
    case MAX_NODE_TYPE_VALUE:
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
  }
  ADB_UNREACHABLE;
}

bool ExecutionNode::alwaysCopiesRows() const {
  return ExecutionNode::alwaysCopiesRows(getType());
}

void ExecutionNode::setVarsUsedLater(VarSetStack varStack) {
  _varsUsedLaterStack = std::move(varStack);
}

void ExecutionNode::setVarsValid(VarSetStack varStack) {
  _varsValidStack = std::move(varStack);
}

auto ExecutionNode::getVarsUsedLater() const noexcept -> VarSet const& {
  return getVarsUsedLaterStack().back();
}

auto ExecutionNode::getVarsUsedLaterStack() const noexcept
    -> VarSetStack const& {
  TRI_ASSERT(_varUsageValid);
  return _varsUsedLaterStack;
}

auto ExecutionNode::getVarsValid() const noexcept -> VarSet const& {
  return getVarsValidStack().back();
}

auto ExecutionNode::getVarsValidStack() const noexcept -> VarSetStack const& {
  TRI_ASSERT(_varUsageValid);
  return _varsValidStack;
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SingletonNode::createBlock(
    ExecutionEngine& engine) const {
  auto registerInfos = createRegisterInfos({}, {});
  IdExecutorInfos infos(false);

  auto res = std::make_unique<ExecutionBlockImpl<IdExecutor<ConstFetcher>>>(
      &engine, this, std::move(registerInfos), std::move(infos));
  std::ignore =
      res->initializeCursor(InputAqlItemRow{CreateInvalidInputRowHint{}});
  return res;
}

/// @brief doToVelocyPack, for SingletonNode
void SingletonNode::doToVelocyPack(velocypack::Builder&, unsigned) const {
  // nothing to do here!
}

/// @brief the cost of a singleton is 1, it produces one item only
CostEstimate SingletonNode::estimateCost() const {
  CostEstimate estimate = CostEstimate::empty();
  estimate.estimatedNrItems = 1;
  estimate.estimatedCost = 1.0;
  return estimate;
}

AsyncPrefetchEligibility SingletonNode::canUseAsyncPrefetching()
    const noexcept {
  // a singleton node has no predecessors. so prefetching isn't
  // necessary.
  return AsyncPrefetchEligibility::kDisableForNode;
}

SingletonNode::SingletonNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ExecutionNode(plan, id) {}

SingletonNode::SingletonNode(ExecutionPlan* plan,
                             arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base) {}

ExecutionNode::NodeType SingletonNode::getType() const { return SINGLETON; }

size_t SingletonNode::getMemoryUsedBytes() const { return sizeof(*this); }

EnumerateCollectionNode::EnumerateCollectionNode(
    ExecutionPlan* plan, ExecutionNodeId id, aql::Collection const* collection,
    Variable const* outVariable, bool random, IndexHint const& hint)
    : ExecutionNode(plan, id),
      DocumentProducingNode(outVariable),
      CollectionAccessingNode(collection),
      _random(random),
      _hint(hint) {}

EnumerateCollectionNode::EnumerateCollectionNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      DocumentProducingNode(plan, base),
      CollectionAccessingNode(plan, base),
      _random(base.get("random").getBoolean()),
      _hint(base) {}

ExecutionNode::NodeType EnumerateCollectionNode::getType() const {
  return ENUMERATE_COLLECTION;
}

size_t EnumerateCollectionNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}

IndexHint const& EnumerateCollectionNode::hint() const { return _hint; }

/// @brief doToVelocyPack, for EnumerateCollectionNode
void EnumerateCollectionNode::doToVelocyPack(velocypack::Builder& builder,
                                             unsigned flags) const {
  builder.add("random", VPackValue(_random));

  _hint.toVelocyPack(builder);

  // add outvariable and projection
  DocumentProducingNode::toVelocyPack(builder, flags);

  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> EnumerateCollectionNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  // check which variables are used by the node's post-filter
  std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;

  if (hasFilter()) {
    VarSet inVars;
    filter()->variables(inVars);

    filterVarsToRegs.reserve(inVars.size());

    for (auto& var : inVars) {
      TRI_ASSERT(var != nullptr);
      auto regId = variableToRegisterId(var);
      filterVarsToRegs.emplace_back(var->id, regId);
    }
  }

  RegisterId outputRegister = variableToRegisterId(_outVariable);

  auto outputRegisters = RegIdSet{};

  auto const& p = projections();
  if (!p.empty()) {
    // projections. no need to produce the full document.
    // instead create one register per projection.
    for (size_t i = 0; i < p.size(); ++i) {
      Variable const* var = p[i].variable;
      if (var == nullptr) {
        // the output register can be a nullptr if the "optimize-projections"
        // rule was not executed (potentially because it was disabled).
        continue;
      }
      TRI_ASSERT(var != nullptr);
      auto regId = variableToRegisterId(var);
      filterVarsToRegs.emplace_back(var->id, regId);
      outputRegisters.emplace(regId);
    }
    // we should either have as many output registers set as there are
    // projections, or none. the latter can happen if the "optimize-projections"
    // rule did not (yet) run
    TRI_ASSERT(outputRegisters.empty() || outputRegisters.size() == p.size());
    // in case we do not have any output registers for the projections,
    // we must write them to the main output register, in a velocypack
    // object.
    // this will be handled below by adding the main output register.
  }
  if (outputRegisters.empty() && isProduceResult()) {
    outputRegisters.emplace(outputRegister);
  }
  TRI_ASSERT(!outputRegisters.empty() || !isProduceResult());

  auto registerInfos = createRegisterInfos({}, std::move(outputRegisters));
  auto executorInfos = EnumerateCollectionExecutorInfos(
      outputRegister, engine.getQuery(), collection(), _outVariable,
      isProduceResult(), filter(), projections(), std::move(filterVarsToRegs),
      _random, doCount(), canReadOwnWrites());
  return std::make_unique<ExecutionBlockImpl<EnumerateCollectionExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* EnumerateCollectionNode::clone(ExecutionPlan* plan,
                                              bool withDependencies) const {
  auto c = std::make_unique<EnumerateCollectionNode>(
      plan, _id, collection(), _outVariable, _random, _hint);

  CollectionAccessingNode::cloneInto(*c);
  DocumentProducingNode::cloneInto(plan, *c);

  return cloneHelper(std::move(c), withDependencies);
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void EnumerateCollectionNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  DocumentProducingNode::replaceVariables(replacements);
}

void EnumerateCollectionNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t /*index*/) {
  DocumentProducingNode::replaceAttributeAccess(self, searchVariable, attribute,
                                                replaceVariable);
}

void EnumerateCollectionNode::setRandom() { _random = true; }

bool EnumerateCollectionNode::isDeterministic() {
  return !_random && (canReadOwnWrites() == ReadOwnWrites::no);
}

void EnumerateCollectionNode::getVariablesUsedHere(VarSet& vars) const {
  if (hasFilter()) {
    Ast::getReferencedVariables(filter()->node(), vars);
    // need to unset the output variable that we produce ourselves,
    // otherwise the register planning runs into trouble. the register
    // planning's assumption is that all variables that are used in a
    // node must also be used later.
    vars.erase(outVariable());

    for (size_t i = 0; i < _projections.size(); ++i) {
      if (_projections[i].variable != nullptr) {
        vars.erase(_projections[i].variable);
      }
    }
  }
}

std::vector<Variable const*> EnumerateCollectionNode::getVariablesSetHere()
    const {
  return DocumentProducingNode::getVariablesSetHere();
}

void EnumerateCollectionNode::setProjections(Projections projections) {
  DocumentProducingNode::setProjections(std::move(projections));
}

bool EnumerateCollectionNode::isProduceResult() const {
  if (hasFilter()) {
    return true;
  }
  auto const& p = projections();
  if (p.empty()) {
    return isVarUsedLater(_outVariable);
  }
  // check output registers of projections
  size_t found = 0;
  for (size_t i = 0; i < p.size(); ++i) {
    Variable const* var = p[i].variable;
    // the output register can be a nullptr if the "optimize-projections"
    // rule was not executed (potentially because it was disabled).
    if (var != nullptr) {
      ++found;
      if (isVarUsedLater(var)) {
        return true;
      }
    }
  }
  if (found == 0) {
    return isVarUsedLater(_outVariable);
  }
  return false;
}

/// @brief the cost of an enumerate collection node is a multiple of the cost of
/// its unique dependency
CostEstimate EnumerateCollectionNode::estimateCost() const {
  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  if (trx.status() != transaction::Status::RUNNING) {
    return CostEstimate::empty();
  }

  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  auto estimatedNrItems =
      collection()->count(&trx, transaction::CountType::TryCache);
  if (_random) {
    // we retrieve at most one random document from the collection.
    // so the estimate is at most 1
    // estimatedNrItems = 1;
    // Unnecessary to set, we just leave estimate.estimatedNrItems as it is.
  } else if (!doCount()) {
    // if "count" mode is active, the estimated number of items from above
    // must not be multiplied with the number of items in this collection
    estimate.estimatedNrItems *= estimatedNrItems;
  }
  // We do a full collection scan for each incoming item.
  // random iteration is slightly more expensive than linear iteration
  // we also penalize each EnumerateCollectionNode slightly (and do not
  // do the same for IndexNodes) so IndexNodes will be preferred
  estimate.estimatedCost += estimate.estimatedNrItems *
                                (_random ? 1.005 : (hasFilter() ? 1.25 : 1.0)) +
                            1.0;

  return estimate;
}

AsyncPrefetchEligibility EnumerateCollectionNode::canUseAsyncPrefetching()
    const noexcept {
  return DocumentProducingNode::canUseAsyncPrefetching();
}

EnumerateListNode::EnumerateListNode(ExecutionPlan* plan, ExecutionNodeId id,
                                     Variable const* inVariable,
                                     Variable const* outVariable)
    : ExecutionNode(plan, id),
      _inVariable(inVariable),
      _outVariable(outVariable) {
  TRI_ASSERT(_inVariable != nullptr);
  TRI_ASSERT(_outVariable != nullptr);
}

EnumerateListNode::EnumerateListNode(ExecutionPlan* plan,
                                     arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")),
      _outVariable(
          Variable::varFromVPack(plan->getAst(), base, "outVariable")) {
  if (VPackSlice p = base.get(StaticStrings::Filter); !p.isNone()) {
    Ast* ast = plan->getAst();
    // new AstNode is memory-managed by the Ast
    setFilter(std::make_unique<Expression>(ast, ast->createNode(p)));
  }
}

/// @brief doToVelocyPack, for EnumerateListNode
void EnumerateListNode::doToVelocyPack(velocypack::Builder& nodes,
                                       unsigned flags) const {
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  if (_filter != nullptr) {
    nodes.add(VPackValue(StaticStrings::Filter));
    _filter->toVelocyPack(nodes, flags);
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> EnumerateListNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  RegisterId inputRegister = variableToRegisterId(_inVariable);
  RegisterId outRegister = variableToRegisterId(_outVariable);
  auto registerInfos =
      createRegisterInfos(RegIdSet{inputRegister}, RegIdSet{outRegister});

  std::vector<std::pair<VariableId, RegisterId>> varsToRegs;
  if (hasFilter()) {
    VarSet inVars;
    _filter->variables(inVars);

    for (auto const& var : inVars) {
      if (var->id != _outVariable->id) {
        auto regId = variableToRegisterId(var);
        varsToRegs.emplace_back(var->id, regId);
      }
    }
  }
  auto executorInfos = EnumerateListExecutorInfos(
      inputRegister, outRegister, engine.getQuery(), filter(), _outVariable->id,
      std::move(varsToRegs));
  return std::make_unique<ExecutionBlockImpl<EnumerateListExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* EnumerateListNode::clone(ExecutionPlan* plan,
                                        bool withDependencies) const {
  auto c =
      std::make_unique<EnumerateListNode>(plan, _id, _inVariable, _outVariable);

  if (hasFilter()) {
    c->setFilter(_filter->clone(plan->getAst(), true));
  }
  return cloneHelper(std::move(c), withDependencies);
}

/// @brief the cost of an enumerate list node
CostEstimate EnumerateListNode::estimateCost() const {
  size_t length = estimateListLength(_plan, _inVariable);

  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems *= length;
  estimate.estimatedCost +=
      estimate.estimatedNrItems * (hasFilter() ? 1.25 : 1.0);
  return estimate;
}

AsyncPrefetchEligibility EnumerateListNode::canUseAsyncPrefetching()
    const noexcept {
  return AsyncPrefetchEligibility::kEnableForNode;
}

ExecutionNode::NodeType EnumerateListNode::getType() const {
  return ENUMERATE_LIST;
}

size_t EnumerateListNode::getMemoryUsedBytes() const { return sizeof(*this); }

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void EnumerateListNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
}

void EnumerateListNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t index) {
  if (hasFilter() && self != this) {
    filter()->replaceAttributeAccess(searchVariable, attribute,
                                     replaceVariable);
  }
}

void EnumerateListNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
  if (hasFilter()) {
    Ast::getReferencedVariables(filter()->node(), vars);
    // need to unset the output variable that we produce ourselves,
    // otherwise the register planning runs into trouble. the register
    // planning's assumption is that all variables that are used in a
    // node must also be used later.
    vars.erase(outVariable());
  }
}

std::vector<Variable const*> EnumerateListNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}

/// @brief remember the condition to execute for early filtering
void EnumerateListNode::setFilter(std::unique_ptr<Expression> filter) {
  _filter = std::move(filter);
}

Variable const* EnumerateListNode::inVariable() const { return _inVariable; }

Variable const* EnumerateListNode::outVariable() const { return _outVariable; }

LimitNode::LimitNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _offset(base.get("offset").getNumericValue<decltype(_offset)>()),
      _limit(base.get("limit").getNumericValue<decltype(_limit)>()),
      _fullCount(base.get("fullCount").getBoolean()) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> LimitNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});

  auto executorInfos = LimitExecutorInfos(_offset, _limit, _fullCount);
  return std::make_unique<ExecutionBlockImpl<LimitExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

// @brief doToVelocyPack, for LimitNode
void LimitNode::doToVelocyPack(velocypack::Builder& nodes,
                               unsigned /*flags*/) const {
  nodes.add("offset", VPackValue(_offset));
  nodes.add("limit", VPackValue(_limit));
  nodes.add("fullCount", VPackValue(_fullCount));
}

/// @brief estimateCost
CostEstimate LimitNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();

  // arbitrary cost value for skipping a single document
  // skipping over a document is not fully free, because in the RocksDB
  // case, we need to move iterarors forward, invoke the comparator etc.
  double const skipCost = 0.000001;

  size_t estimatedNrItems = estimate.estimatedNrItems;
  if (estimatedNrItems >= _offset) {
    estimate.estimatedCost += _offset * skipCost;
    estimatedNrItems -= _offset;
  } else {
    estimate.estimatedCost += estimatedNrItems * skipCost;
    estimatedNrItems = 0;
  }
  estimate.estimatedNrItems = (std::min)(_limit, estimatedNrItems);
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

LimitNode::LimitNode(ExecutionPlan* plan, ExecutionNodeId id, size_t offset,
                     size_t limit)
    : ExecutionNode(plan, id),
      _offset(offset),
      _limit(limit),
      _fullCount(false) {}

ExecutionNode::NodeType LimitNode::getType() const { return LIMIT; }

size_t LimitNode::getMemoryUsedBytes() const { return sizeof(*this); }

ExecutionNode* LimitNode::clone(ExecutionPlan* plan,
                                bool withDependencies) const {
  auto c = std::make_unique<LimitNode>(plan, _id, _offset, _limit);

  if (_fullCount) {
    c->setFullCount();
  }

  return cloneHelper(std::move(c), withDependencies);
}

AsyncPrefetchEligibility LimitNode::canUseAsyncPrefetching() const noexcept {
  // LimitNodes do not support async prefetching.
  return AsyncPrefetchEligibility::kDisableForNodeAndDependencies;
}

void LimitNode::setFullCount(bool enable) { _fullCount = enable; }

bool LimitNode::fullCount() const noexcept { return _fullCount; }

size_t LimitNode::offset() const { return _offset; }

size_t LimitNode::limit() const { return _limit; }

CalculationNode::CalculationNode(ExecutionPlan* plan,
                                 arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _outVariable(Variable::varFromVPack(plan->getAst(), base, "outVariable")),
      _expression(std::make_unique<Expression>(plan->getAst(), base)) {}

CalculationNode::CalculationNode(ExecutionPlan* plan, ExecutionNodeId id,
                                 std::unique_ptr<Expression> expr,
                                 Variable const* outVariable)
    : ExecutionNode(plan, id),
      _outVariable(outVariable),
      _expression(std::move(expr)) {
  TRI_ASSERT(_expression != nullptr);
  TRI_ASSERT(_outVariable != nullptr);
}

CalculationNode::~CalculationNode() = default;

/// @brief doToVelocyPack, for CalculationNode
void CalculationNode::doToVelocyPack(velocypack::Builder& nodes,
                                     unsigned flags) const {
  nodes.add(VPackValue("expression"));
  _expression->toVelocyPack(nodes, flags);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add("canThrow", VPackValue(false));

  nodes.add("expressionType", VPackValue(_expression->typeString()));

  if ((flags & SERIALIZE_FUNCTIONS) && _expression->node() != nullptr) {
    auto root = _expression->node();
    if (root != nullptr) {
      // enumerate all used functions, but report each function only once
      std::unordered_set<std::string> functionsSeen;
      nodes.add("functions", VPackValue(VPackValueType::Array));

      Ast::traverseReadOnly(
          root, [&functionsSeen, &nodes](AstNode const* node) -> bool {
            if (node->type == NODE_TYPE_FCALL) {
              auto func = static_cast<Function const*>(node->getData());
              if (functionsSeen.insert(func->name).second) {
                // built-in function, not seen before
                nodes.openObject();
                nodes.add("name", VPackValue(func->name));
                nodes.add(
                    "isDeterministic",
                    VPackValue(func->hasFlag(Function::Flags::Deterministic)));
                nodes.add("canAccessDocuments",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanReadDocuments)));
                nodes.add("canRunOnDBServerCluster",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanRunOnDBServerCluster)));
                nodes.add("canRunOnDBServerOneShard",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanRunOnDBServerOneShard)));
                nodes.add(
                    "cacheable",
                    VPackValue(func->hasFlag(Function::Flags::Cacheable)));
                nodes.add("usesV8", VPackValue(node->willUseV8()));
                // deprecated
                nodes.add("canRunOnDBServer",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanRunOnDBServerCluster)));
                nodes.close();
              }
            } else if (node->type == NODE_TYPE_FCALL_USER) {
              auto func = node->getString();
              if (functionsSeen.insert(func).second) {
                // user defined function, not seen before
                nodes.openObject();
                nodes.add("name", VPackValue(func));
                nodes.add("isDeterministic", VPackValue(false));
                nodes.add("canRunOnDBServer", VPackValue(false));
                nodes.add("usesV8", VPackValue(true));
                nodes.close();
              }
            }
            return true;
          });

      nodes.close();
    }
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> CalculationNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId outputRegister = variableToRegisterId(_outVariable);
  TRI_ASSERT((_outVariable->type() == Variable::Type::Const) ==
             outputRegister.isConstRegister());

  VarSet inVars;
  _expression->variables(inVars);

  std::vector<std::pair<VariableId, RegisterId>> expInVarsToRegs;
  expInVarsToRegs.reserve(inVars.size());

  auto inputRegisters = RegIdSet{};

  for (auto const& var : inVars) {
    TRI_ASSERT(var != nullptr);
    auto regId = variableToRegisterId(var);
    expInVarsToRegs.emplace_back(var->id, regId);
    inputRegisters.emplace(regId);
  }

  bool const isReference = (expression()->node()->type == NODE_TYPE_REFERENCE);
  if (isReference) {
    TRI_ASSERT(expInVarsToRegs.size() == 1);
  }
#ifdef USE_V8
  bool const willUseV8 = expression()->willUseV8();
#endif

  TRI_ASSERT(expression() != nullptr);

  auto registerInfos = createRegisterInfos(std::move(inputRegisters),
                                           outputRegister.isRegularRegister()
                                               ? RegIdSet{outputRegister}
                                               : RegIdSet{});

  if (_outVariable->type() == Variable::Type::Const) {
    // we have a const variable, so we can simply use an IdExector to forward
    // the rows.
    auto executorInfos = IdExecutorInfos(false, outputRegister);
    return std::make_unique<ExecutionBlockImpl<
        IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }

  auto executorInfos = CalculationExecutorInfos(
      outputRegister,
      engine.getQuery() /* used for v8 contexts and in expression */,
      *expression(),
      std::move(expInVarsToRegs)); /* required by expression.execute */

  if (isReference) {
    return std::make_unique<
        ExecutionBlockImpl<CalculationExecutor<CalculationType::Reference>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
#ifdef USE_V8
  } else if (willUseV8) {
    return std::make_unique<
        ExecutionBlockImpl<CalculationExecutor<CalculationType::V8Condition>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
#endif
  } else {
    return std::make_unique<
        ExecutionBlockImpl<CalculationExecutor<CalculationType::Condition>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
}

ExecutionNode* CalculationNode::clone(ExecutionPlan* plan,
                                      bool withDependencies) const {
  return cloneHelper(
      std::make_unique<CalculationNode>(
          plan, _id, _expression->clone(plan->getAst(), true), _outVariable),
      withDependencies);
}

/// @brief estimateCost
CostEstimate CalculationNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

AsyncPrefetchEligibility CalculationNode::canUseAsyncPrefetching()
    const noexcept {
  // we cannot use prefetching if the calculation employs V8, because the
  // Query object only has a single V8 context, which it can enter and exit.
  // with prefetching, multiple threads can execute calculations in the same
  // Query instance concurrently, and when using V8, they could try to
  // enter/exit the V8 context of the query concurrently. this is currently
  // not thread-safe, so we don't use prefetching.
  // the constraint for determinism is there because we could produce
  // different query results when prefetching is enabled, at least in
  // streaming queries.
  return expression()->isDeterministic() && !expression()->willUseV8()
             ? AsyncPrefetchEligibility::kEnableForNode
             : AsyncPrefetchEligibility::kDisableForNode;
}

ExecutionNode::NodeType CalculationNode::getType() const { return CALCULATION; }

size_t CalculationNode::getMemoryUsedBytes() const { return sizeof(*this); }

Variable const* CalculationNode::outVariable() const { return _outVariable; }

Expression* CalculationNode::expression() const {
  TRI_ASSERT(_expression != nullptr);
  return _expression.get();
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void CalculationNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  VarSet variables;
  expression()->variables(variables);

  // check if the calculation uses any of the variables that we want to
  // replace
  for (auto const& it : variables) {
    if (replacements.contains(it->id)) {
      // calculation uses a to-be-replaced variable
      expression()->replaceVariables(replacements);
      return;
    }
  }
}

void CalculationNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t /*index*/) {
  expression()->replaceAttributeAccess(searchVariable, attribute,
                                       replaceVariable);
}

void CalculationNode::getVariablesUsedHere(VarSet& vars) const {
  expression()->variables(vars);
}

std::vector<Variable const*> CalculationNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}

bool CalculationNode::isDeterministic() {
  return _expression->isDeterministic();
}

SubqueryNode::SubqueryNode(ExecutionPlan* plan,
                           arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _subquery(nullptr),
      _outVariable(
          Variable::varFromVPack(plan->getAst(), base, "outVariable")) {}

/// @brief doToVelocyPack, for SubqueryNode
void SubqueryNode::doToVelocyPack(velocypack::Builder& nodes,
                                  unsigned flags) const {
  // Since we have spliced subqueries this should never be called.
  // However, we still keep the old implementation around in case it is needed
  // again at some point (e.g., if we want to serialize nodes during
  // optimization)
  TRI_ASSERT(false);
  {
    VPackObjectBuilder guard(&nodes, "subquery");
    nodes.add(VPackValue("nodes"));
    _subquery->allToVelocyPack(nodes, flags);
  }
  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add("isConst", VPackValue(const_cast<SubqueryNode*>(this)->isConst()));
}

/// @brief invalidate the cost estimation for the node and its dependencies
void SubqueryNode::invalidateCost() {
  ExecutionNode::invalidateCost();
  // pass invalidation call to subquery too
  getSubquery()->invalidateCost();
}

bool SubqueryNode::isConst() {
  if (isModificationNode() || !isDeterministic()) {
    return false;
  }

  if (mayAccessCollections() && _plan->getAst()->containsModificationNode()) {
    // a subquery that accesses data from a collection may not be const,
    // even if itself does not modify any data. it is possible that the
    // subquery is embedded into some outer loop that is modifying data
    return false;
  }

  VarSet vars;
  getVariablesUsedHere(vars);
  for (auto const& v : vars) {
    auto setter = _plan->getVarSetBy(v->id);

    if (setter == nullptr || setter->getType() != CALCULATION) {
      return false;
    }

    auto expression =
        ExecutionNode::castTo<CalculationNode const*>(setter)->expression();

    if (expression == nullptr) {
      return false;
    }
    if (!expression->isConstant()) {
      return false;
    }
  }

  return true;
}

bool SubqueryNode::mayAccessCollections() {
  if (_plan->getAst()->functionsMayAccessDocuments()) {
    // if the query contains any calls to functions that MAY access any
    // document, then we count this as a "yes"
    return true;
  }

  TRI_ASSERT(_subquery != nullptr);

  // if the subquery contains any of these nodes, it may access data from
  // a collection
  std::initializer_list<ExecutionNode::NodeType> const types = {
      ExecutionNode::ENUMERATE_IRESEARCH_VIEW,
      ExecutionNode::ENUMERATE_COLLECTION,
      ExecutionNode::INDEX,
      ExecutionNode::JOIN,
      ExecutionNode::INSERT,
      ExecutionNode::UPDATE,
      ExecutionNode::REPLACE,
      ExecutionNode::REMOVE,
      ExecutionNode::UPSERT,
      ExecutionNode::TRAVERSAL,
      ExecutionNode::SHORTEST_PATH,
      ExecutionNode::ENUMERATE_PATHS};

  containers::SmallVector<ExecutionNode*, 8> nodes;

  NodeFinder<std::initializer_list<ExecutionNode::NodeType>,
             WalkerUniqueness::Unique>
      finder(types, nodes, true);
  _subquery->walk(finder);

  if (!nodes.empty()) {
    return true;
  }

  return false;
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SubqueryNode::createBlock(
    ExecutionEngine& /*engine*/) const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "cannot instantiate SubqueryExecutor");
}

ExecutionNode* SubqueryNode::clone(ExecutionPlan* plan,
                                   bool withDependencies) const {
  return cloneHelper(std::make_unique<SubqueryNode>(
                         plan, _id, _subquery->clone(plan, true), _outVariable),
                     withDependencies);
}

/// @brief whether or not the subquery is a data-modification operation
bool SubqueryNode::isModificationNode() const {
  std::vector<ExecutionNode*> stack({_subquery});

  while (!stack.empty()) {
    ExecutionNode* current = stack.back();

    if (current->isModificationNode()) {
      return true;
    }

    stack.pop_back();

    current->dependencies(stack);
  }

  return false;
}

/// @brief replace the out variable, so we can adjust the name.
void SubqueryNode::replaceOutVariable(Variable const* var) {
  _outVariable = var;
}

/// @brief estimateCost
CostEstimate SubqueryNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate subEstimate = _subquery->getCost();

  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost +=
      estimate.estimatedNrItems * subEstimate.estimatedCost;
  return estimate;
}

/// @brief helper struct to find all (outer) variables used in a SubqueryNode
struct SubqueryVarUsageFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::Unique> {
  VarSet _usedLater;
  VarSet _valid;

  ~SubqueryVarUsageFinder() = default;

  bool before(ExecutionNode* en) override final {
    // Add variables used here to _usedLater:
    en->getVariablesUsedHere(_usedLater);
    return false;
  }

  void after(ExecutionNode* en) override final {
    // Add variables set here to _valid:
    for (auto& v : en->getVariablesSetHere()) {
      _valid.insert(v);
    }
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode* sub) override final {
    SubqueryVarUsageFinder subfinder;
    sub->walk(subfinder);

    // keep track of all variables used by a (dependent) subquery
    // this is, all variables in the subqueries _usedLater that are not in
    // _valid

    // create the set difference. note: cannot use std::set_difference as our
    // sets are NOT sorted
    for (auto var : subfinder._usedLater) {
      if (_valid.find(var) != _valid.end()) {
        _usedLater.insert(var);
      }
    }
    return false;
  }
};

/// @brief getVariablesUsedHere, modifying the set in-place
void SubqueryNode::getVariablesUsedHere(VarSet& vars) const {
  SubqueryVarUsageFinder finder;
  _subquery->walk(finder);

  for (auto var : finder._usedLater) {
    if (finder._valid.find(var) == finder._valid.end()) {
      vars.insert(var);
    }
  }
}

/// @brief is the node determistic?
struct DeterministicFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::Unique> {
  bool _isDeterministic = true;

  DeterministicFinder() : _isDeterministic(true) {}
  ~DeterministicFinder() = default;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* node) override final {
    if (!node->isDeterministic()) {
      _isDeterministic = false;
      return true;
    }
    return false;
  }
};

bool SubqueryNode::isDeterministic() {
  DeterministicFinder finder;
  _subquery->walk(finder);
  return finder._isDeterministic;
}

SubqueryNode::SubqueryNode(ExecutionPlan* plan, ExecutionNodeId id,
                           ExecutionNode* subquery, Variable const* outVariable)
    : ExecutionNode(plan, id), _subquery(subquery), _outVariable(outVariable) {
  TRI_ASSERT(_subquery != nullptr);
  TRI_ASSERT(_outVariable != nullptr);
}

ExecutionNode::NodeType SubqueryNode::getType() const { return SUBQUERY; }

size_t SubqueryNode::getMemoryUsedBytes() const { return sizeof(*this); }

Variable const* SubqueryNode::outVariable() const { return _outVariable; }

ExecutionNode* SubqueryNode::getSubquery() const { return _subquery; }

void SubqueryNode::setSubquery(ExecutionNode* subquery, bool forceOverwrite) {
  TRI_ASSERT(subquery != nullptr);
  TRI_ASSERT((forceOverwrite && _subquery != nullptr) ||
             (!forceOverwrite && _subquery == nullptr));
  _subquery = subquery;
}

std::vector<Variable const*> SubqueryNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}

FilterNode::FilterNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")) {}

/// @brief doToVelocyPack, for FilterNode
void FilterNode::doToVelocyPack(velocypack::Builder& nodes,
                                unsigned /*flags*/) const {
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> FilterNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  RegisterId inputRegister = variableToRegisterId(_inVariable);

  auto registerInfos = createRegisterInfos(RegIdSet{inputRegister}, {});
  auto executorInfos = FilterExecutorInfos(inputRegister);
  return std::make_unique<ExecutionBlockImpl<FilterExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* FilterNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  return cloneHelper(std::make_unique<FilterNode>(plan, _id, _inVariable),
                     withDependencies);
}

/// @brief estimateCost
CostEstimate FilterNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());

  // We are pessimistic here by not reducing the nrItems. However, in the
  // worst case the filter does not reduce the items at all. Furthermore,
  // no optimizer rule introduces FilterNodes, thus it is not important
  // that they appear to lower the costs. Note that contrary to this,
  // an IndexNode does lower the costs, it also has a better idea
  // to what extent the number of items is reduced. On the other hand it
  // is important that a FilterNode produces additional costs, otherwise
  // the rule throwing away a FilterNode that is already covered by an
  // IndexNode cannot reduce the costs.
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

AsyncPrefetchEligibility FilterNode::canUseAsyncPrefetching() const noexcept {
  return AsyncPrefetchEligibility::kEnableForNode;
}

FilterNode::FilterNode(ExecutionPlan* plan, ExecutionNodeId id,
                       Variable const* inVariable)
    : ExecutionNode(plan, id), _inVariable(inVariable) {
  TRI_ASSERT(_inVariable != nullptr);
}

ExecutionNode::NodeType FilterNode::getType() const { return FILTER; }

size_t FilterNode::getMemoryUsedBytes() const { return sizeof(*this); }

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void FilterNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
}

void FilterNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
}

Variable const* FilterNode::inVariable() const { return _inVariable; }

ReturnNode::ReturnNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")),
      _count(VelocyPackHelper::getBooleanValue(base, "count", false)) {}

/// @brief doToVelocyPack, for ReturnNode
void ReturnNode::doToVelocyPack(velocypack::Builder& nodes,
                                unsigned /*flags*/) const {
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);
  nodes.add("count", VPackValue(_count));
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ReturnNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId inputRegister = variableToRegisterId(_inVariable);

  // This is an important performance improvement:
  // If we have inherited results, we do move the block through
  // and do not modify it in any way.
  // In the other case it is important to shrink the matrix to exactly
  // one register that is stored within the DOCVEC.
  if (returnInheritedResults()) {
    auto executorInfos = IdExecutorInfos(_count, inputRegister);
    auto registerInfos = createRegisterInfos({}, {});
    return std::make_unique<ExecutionBlockImpl<
        IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else {
    TRI_ASSERT(!returnInheritedResults());
    // TODO We should be able to remove this special case when the new
    //      register planning is ready.
    // The ReturnExecutor only writes to register 0.
    constexpr auto outputRegister = RegisterId{0};

    auto registerInfos =
        createRegisterInfos(RegIdSet{inputRegister}, RegIdSet{outputRegister});
    auto executorInfos = ReturnExecutorInfos(inputRegister, _count);

    return std::make_unique<ExecutionBlockImpl<ReturnExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
}

bool ReturnNode::returnInheritedResults() const {
  bool const isRoot = plan()->root() == this;
  return isRoot;
}

/// @brief clone ExecutionNode recursively
ExecutionNode* ReturnNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  auto c = std::make_unique<ReturnNode>(plan, _id, _inVariable);

  if (_count) {
    c->setCount();
  }

  return cloneHelper(std::move(c), withDependencies);
}

/// @brief estimateCost
CostEstimate ReturnNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

AsyncPrefetchEligibility ReturnNode::canUseAsyncPrefetching() const noexcept {
  // cannot enable async prefetching for return nodes right now,
  // as it will produce assertion failures regarding the reference
  // count of the result block
  return AsyncPrefetchEligibility::kDisableForNode;
}

ReturnNode::ReturnNode(ExecutionPlan* plan, ExecutionNodeId id,
                       Variable const* inVariable)
    : ExecutionNode(plan, id), _inVariable(inVariable), _count(false) {
  TRI_ASSERT(_inVariable != nullptr);
}

ExecutionNode::NodeType ReturnNode::getType() const { return RETURN; }

size_t ReturnNode::getMemoryUsedBytes() const { return sizeof(*this); }

void ReturnNode::setCount() { _count = true; }

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void ReturnNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
}

void ReturnNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
}

Variable const* ReturnNode::inVariable() const { return _inVariable; }

void ReturnNode::inVariable(Variable const* v) { _inVariable = v; }

/// @brief doToVelocyPack, for NoResultsNode
void NoResultsNode::doToVelocyPack(velocypack::Builder&, unsigned) const {
  // nothing to do here!
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> NoResultsNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});
  return std::make_unique<ExecutionBlockImpl<NoResultsExecutor>>(
      &engine, this, std::move(registerInfos), EmptyExecutorInfos{});
}

/// @brief estimateCost, the cost of a NoResults is nearly 0
CostEstimate NoResultsNode::estimateCost() const {
  // we have trigger cost estimation for parent nodes because this node could
  // be spliced into a subquery.
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems = 0;
  estimate.estimatedCost = 0.5;  // just to make it non-zero
  return estimate;
}

NoResultsNode::NoResultsNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ExecutionNode(plan, id) {}

NoResultsNode::NoResultsNode(ExecutionPlan* plan,
                             arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base) {}

ExecutionNode::NodeType NoResultsNode::getType() const { return NORESULTS; }

ExecutionNode* NoResultsNode::clone(ExecutionPlan* plan,
                                    bool withDependencies) const {
  return cloneHelper(std::make_unique<NoResultsNode>(plan, _id),
                     withDependencies);
}

AsyncPrefetchEligibility NoResultsNode::canUseAsyncPrefetching()
    const noexcept {
  // NoResultsNodes could make use of async prefetching, but the gain
  // is too little to justify it.
  return AsyncPrefetchEligibility::kDisableForNode;
}

size_t NoResultsNode::getMemoryUsedBytes() const { return sizeof(*this); }

SortElement::SortElement(Variable const* v, bool asc)
    : var(v), ascending(asc) {}

SortElement::SortElement(Variable const* v, bool asc,
                         std::vector<std::string> path)
    : var(v), ascending(asc), attributePath(std::move(path)) {}

std::string SortElement::toString() const {
  std::string result("$");
  result += std::to_string(var->id);
  for (auto const& it : attributePath) {
    result += "." + it;
  }
  return result;
}

SortInformation::Match SortInformation::isCoveredBy(
    SortInformation const& other) {
  if (!isValid || !other.isValid) {
    return unequal;
  }

  if (isComplex || other.isComplex) {
    return unequal;
  }

  size_t const n = criteria.size();
  for (size_t i = 0; i < n; ++i) {
    if (other.criteria.size() <= i) {
      return otherLessAccurate;
    }

    auto ours = criteria[i];
    auto theirs = other.criteria[i];

    if (std::get<2>(ours) != std::get<2>(theirs)) {
      // sort order is different
      return unequal;
    }

    if (std::get<1>(ours) != std::get<1>(theirs)) {
      // sort criterion is different
      return unequal;
    }
  }

  if (other.criteria.size() > n) {
    return ourselvesLessAccurate;
  }

  return allEqual;
}

/// @brief doToVelocyPack, for AsyncNode
void AsyncNode::doToVelocyPack(velocypack::Builder&, unsigned) const {
  // nothing to do here!
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> AsyncNode::createBlock(
    ExecutionEngine& engine) const {
  return std::make_unique<ExecutionBlockImpl<AsyncExecutor>>(&engine, this);
}

/// @brief estimateCost
CostEstimate AsyncNode::estimateCost() const {
  if (_dependencies.empty()) {
    // we should always have dependency as we need input here...
    TRI_ASSERT(false);
    return aql::CostEstimate::empty();
  }
  aql::CostEstimate estimate = _dependencies[0]->getCost();
  return estimate;
}

AsyncNode::AsyncNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ExecutionNode(plan, id) {}

AsyncNode::AsyncNode(ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base) {}

ExecutionNode::NodeType AsyncNode::getType() const { return ASYNC; }

size_t AsyncNode::getMemoryUsedBytes() const { return sizeof(*this); }

ExecutionNode* AsyncNode::clone(ExecutionPlan* plan,
                                bool withDependencies) const {
  return cloneHelper(std::make_unique<AsyncNode>(plan, _id), withDependencies);
}

namespace {
constexpr std::string_view kMaterializeNodeInNmDocParam = "inNmDocId";
constexpr std::string_view kMaterializeNodeOutVariableParam = "outVariable";
constexpr std::string_view kMaterializeNodeMultiNodeParam = "multiNode";
constexpr std::string_view kMaterializeNodeInLocalDocIdParam = "inLocalDocId";
constexpr std::string_view kMaterializeNodeOldDocVar = "oldDocVariable";
}  // namespace

MaterializeNode* materialize::createMaterializeNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice const base) {
  auto isMulti = base.get(kMaterializeNodeMultiNodeParam);
  if (isMulti.isTrue()) {
    return new MaterializeSearchNode(plan, base);
  }
  return new MaterializeRocksDBNode(plan, base);
}

MaterializeNode::MaterializeNode(ExecutionPlan* plan, ExecutionNodeId id,
                                 aql::Variable const& inDocId,
                                 aql::Variable const& outVariable,
                                 aql::Variable const& oldDocVariable)
    : ExecutionNode(plan, id),
      _inNonMaterializedDocId(&inDocId),
      _outVariable(&outVariable),
      _oldDocVariable(&oldDocVariable) {}

MaterializeNode::MaterializeNode(ExecutionPlan* plan,
                                 arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inNonMaterializedDocId(aql::Variable::varFromVPack(
          plan->getAst(), base, kMaterializeNodeInNmDocParam, true)),
      _outVariable(aql::Variable::varFromVPack(
          plan->getAst(), base, kMaterializeNodeOutVariableParam)),
      _oldDocVariable(aql::Variable::varFromVPack(plan->getAst(), base,
                                                  kMaterializeNodeOldDocVar)) {}

void MaterializeNode::doToVelocyPack(velocypack::Builder& nodes,
                                     unsigned /*flags*/) const {
  nodes.add(VPackValue(kMaterializeNodeInNmDocParam));
  _inNonMaterializedDocId->toVelocyPack(nodes);

  nodes.add(VPackValue(kMaterializeNodeOutVariableParam));
  _outVariable->toVelocyPack(nodes);

  nodes.add(VPackValue(kMaterializeNodeOldDocVar));
  _oldDocVariable->toVelocyPack(nodes);
}

CostEstimate MaterializeNode::estimateCost() const {
  if (_dependencies.empty()) {
    // we should always have dependency as we need input for materializing
    TRI_ASSERT(false);
    return aql::CostEstimate::empty();
  }
  aql::CostEstimate estimate = _dependencies[0]->getCost();
  // we will materialize all output of our dependency
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

void MaterializeNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inNonMaterializedDocId);
}

size_t MaterializeNode::getMemoryUsedBytes() const { return sizeof(*this); }

std::vector<Variable const*> MaterializeNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}

MaterializeSearchNode::MaterializeSearchNode(
    ExecutionPlan* plan, ExecutionNodeId id, aql::Variable const& inDocId,
    aql::Variable const& outVariable, aql::Variable const& oldDocVariable)
    : MaterializeNode(plan, id, inDocId, outVariable, oldDocVariable) {}

MaterializeSearchNode::MaterializeSearchNode(ExecutionPlan* plan,
                                             arangodb::velocypack::Slice base)
    : MaterializeNode(plan, base) {}

void MaterializeSearchNode::doToVelocyPack(velocypack::Builder& nodes,
                                           unsigned flags) const {
  // call base class method
  MaterializeNode::doToVelocyPack(nodes, flags);
  nodes.add(kMaterializeNodeMultiNodeParam, velocypack::Value(true));
}

std::unique_ptr<ExecutionBlock> MaterializeSearchNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId inNmDocIdRegId;
  {
    auto it = getRegisterPlan()->varInfo.find(_inNonMaterializedDocId->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    inNmDocIdRegId = it->second.registerId;
  }
  RegisterId outDocumentRegId;
  {
    auto it = getRegisterPlan()->varInfo.find(_outVariable->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    outDocumentRegId = it->second.registerId;
  }

  RegIdSet readableInputRegisters{inNmDocIdRegId};
  auto writableOutputRegisters = RegIdSet{outDocumentRegId};

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  auto executorInfos = MaterializerExecutorInfos(
      inNmDocIdRegId, outDocumentRegId, engine.getQuery(), nullptr, {}, {});

  return std::make_unique<ExecutionBlockImpl<MaterializeSearchExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* MaterializeSearchNode::clone(ExecutionPlan* plan,
                                            bool withDependencies) const {
  TRI_ASSERT(plan);

  return cloneHelper(
      std::make_unique<MaterializeSearchNode>(
          plan, _id, *_inNonMaterializedDocId, *_outVariable, *_oldDocVariable),
      withDependencies);
}

MaterializeRocksDBNode::MaterializeRocksDBNode(
    ExecutionPlan* plan, ExecutionNodeId id, aql::Collection const* collection,
    aql::Variable const& inDocId, aql::Variable const& outVariable,
    aql::Variable const& oldDocVariable)
    : MaterializeNode(plan, id, inDocId, outVariable, oldDocVariable),
      CollectionAccessingNode(collection) {}

MaterializeRocksDBNode::MaterializeRocksDBNode(ExecutionPlan* plan,
                                               arangodb::velocypack::Slice base)
    : MaterializeNode(plan, base),
      CollectionAccessingNode(plan, base),
      _projections(Projections::fromVelocyPack(
          plan->getAst(), base, plan->getAst()->query().resourceMonitor())) {}

void MaterializeRocksDBNode::doToVelocyPack(velocypack::Builder& nodes,
                                            unsigned flags) const {
  // call base class method
  MaterializeNode::doToVelocyPack(nodes, flags);

  // add collection information
  CollectionAccessingNode::toVelocyPack(nodes, flags);
  nodes.add(kMaterializeNodeInLocalDocIdParam, true);

  if (!_projections.empty()) {
    _projections.toVelocyPack(nodes);
  }
}

std::vector<Variable const*> MaterializeRocksDBNode::getVariablesSetHere()
    const {
  if (projections().empty() || !projections().hasOutputRegisters()) {
    return std::vector<Variable const*>{_outVariable};
  } else {
    std::vector<Variable const*> vars;
    vars.reserve(projections().size());
    std::transform(projections().projections().begin(),
                   projections().projections().end(), std::back_inserter(vars),
                   [](auto const& p) { return p.variable; });
    return vars;
  }
}

std::unique_ptr<ExecutionBlock> MaterializeRocksDBNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto writableOutputRegisters = RegIdSet{};
  containers::FlatHashMap<VariableId, RegisterId> varsToRegs;

  RegisterId outDocumentRegId;
  if (projections().empty() || !projections().hasOutputRegisters()) {
    auto it = getRegisterPlan()->varInfo.find(_outVariable->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end())
        << "variable not found = " << _outVariable->id;
    outDocumentRegId = it->second.registerId;
    writableOutputRegisters.emplace(outDocumentRegId);
  } else {
    for (auto const& p : projections().projections()) {
      auto reg = getRegisterPlan()->variableToRegisterId(p.variable);
      varsToRegs.emplace(p.variable->id, reg);
      writableOutputRegisters.emplace(reg);
    }
  }
  RegisterId inNmDocIdRegId;
  {
    auto it = getRegisterPlan()->varInfo.find(_inNonMaterializedDocId->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    inNmDocIdRegId = it->second.registerId;
  }
  RegIdSet readableInputRegisters;
  if (inNmDocIdRegId.isValid()) {
    readableInputRegisters.emplace(inNmDocIdRegId);
  }

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  auto executorInfos = MaterializerExecutorInfos(
      inNmDocIdRegId, outDocumentRegId, engine.getQuery(), collection(),
      _projections, std::move(varsToRegs));

  return std::make_unique<ExecutionBlockImpl<MaterializeRocksDBExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* MaterializeRocksDBNode::clone(ExecutionPlan* plan,
                                             bool withDependencies) const {
  TRI_ASSERT(plan);

  auto c = std::make_unique<MaterializeRocksDBNode>(
      plan, _id, collection(), *_inNonMaterializedDocId, *_outVariable,
      *_oldDocVariable);
  c->_projections = _projections;
  CollectionAccessingNode::cloneInto(*c);
  return cloneHelper(std::move(c), withDependencies);
}
