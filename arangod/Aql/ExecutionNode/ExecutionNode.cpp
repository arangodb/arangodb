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

#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/AsyncNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/MaterializeNode.h"
#include "Aql/ExecutionNode/MultipleRemoteModificationNode.h"
#include "Aql/ExecutionNode/MutexNode.h"
#include "Aql/ExecutionNode/NoResultsNode.h"
#include "Aql/ExecutionNode/ReplaceNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/SingleRemoteOperationNode.h"
#include "Aql/ExecutionNode/SingletonNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryEndExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/SubqueryStartExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Aql/ExecutionNode/WindowNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Aql/Range.h"
#include "Aql/RegisterPlan.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Basics/VelocyPackHelper.h"

#include <absl/strings/str_cat.h>
#include <frozen/unordered_map.h>
#include <velocypack/Iterator.h>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace {

/// @brief NodeType to string mapping
frozen::unordered_map<int, std::string_view, 36> const kTypeNames{
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

/// @brief resolve nodeType to a string_view.
std::string_view ExecutionNode::getTypeString(NodeType type) {
  auto it = ::kTypeNames.find(static_cast<int>(type));

  if (it != ::kTypeNames.end()) {
    return (*it).second;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "missing type in TypeNames");
}

/// @brief returns the type name of the node
std::string_view ExecutionNode::getTypeString() const {
  return getTypeString(getType());
}

void ExecutionNode::validateType(int type) {
  if (::kTypeNames.find(static_cast<int>(type)) == ::kTypeNames.end()) {
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
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        absl::StrCat("unexpected value for ", which, " elements"));
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
      for (auto it2 : VPackArrayIterator(path)) {
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

          aggregateVariables.emplace_back(
              AggregateVarInfo{outVar, inVar, it.get("type").copyString()});
        }
      }

      return new CollectNode(plan, slice, expressionVariable, outVariable,
                             keepVariables,
                             plan->getAst()->variables()->variables(false),
                             groupVariables, aggregateVariables);
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
      return materialize::createMaterializeNode(plan, slice);
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

          aggregateVariables.emplace_back(
              AggregateVarInfo{outVar, inVar, it.get("type").copyString()});
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

  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      absl::StrCat("unable to create ExecutionNode from snippet ",
                   slice.toJson()));
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

  setIsAsyncPrefetchEnabled(VelocyPackHelper::getBooleanValue(
      slice, "isAsyncPrefetchEnabled", false));

  _isCallstackSplitEnabled = VelocyPackHelper::getBooleanValue(
      slice, "isCallstackSplitEnabled", false);

  if (isAsyncPrefetchEnabled()) {
    plan->getAst()->setContainsAsyncPrefetch();
  }
}

ExecutionNode::ExecutionNode(ExecutionPlan& plan, ExecutionNode const& other)
    : ExecutionNode(&plan, other.id()) {
  TRI_ASSERT(&plan == other.plan());
  other.cloneWithoutRegisteringAndDependencies(plan, *this);
}

/// @brief exports this ExecutionNode with all its dependencies to VelocyPack.
/// This function implicitly creates an array and serializes all nodes
/// top-down, i.e., the upmost dependency will be the first, and this node
/// will be the last in the array.
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
  if (isAsyncPrefetchEnabled()) {
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

bool ExecutionNode::isAsyncPrefetchEnabled() const noexcept {
  return _isAsyncPrefetchEnabled;
}

void ExecutionNode::setIsAsyncPrefetchEnabled(bool v) noexcept {
  if (v != _isAsyncPrefetchEnabled) {
    _isAsyncPrefetchEnabled = v;
    if (v) {
      _plan->increaseAsyncPrefetchNodes();
    } else {
      _plan->decreaseAsyncPrefetchNodes();
    }
  }
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
    if (which.contains(v)) {
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
