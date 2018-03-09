////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchFeature.h"
#include "IResearchViewNode.h"
#include "IResearchViewBlock.h"
#include "IResearchOrderFactory.h"
#include "IResearchView.h"
#include "AqlHelper.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/SortCondition.h"
#include "Aql/Query.h"
#include "Aql/ExecutionEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Basics/StringUtils.h"

namespace {

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
aql::AstNode const ALL(true, aql::VALUE_TYPE_BOOL);

inline bool filterConditionIsEmpty(aql::AstNode const* filterCondition) {
  return filterCondition == &ALL;
}

// in loop or non-deterministic
bool hasDependecies(
    aql::ExecutionPlan const& plan,
    aql::AstNode const& node,
    aql::Variable const& ref,
    std::unordered_set<aql::Variable const*>& vars
) {
  if (!node.isDeterministic()) {
    return false;
  }

  vars.clear();
  aql::Ast::getReferencedVariables(&node, vars);
  vars.erase(&ref); // remove "our" variable

  for (auto const* var : vars) {
    auto* setter = plan.getVarSetBy(var->id);

    if (!setter) {
      // unable to find setter
      continue;
    }

    if (!setter->isDeterministic()) {
      // found nondeterministic setter
      return true;
    }

    switch (setter->getType()) {
      case aql::ExecutionNode::ENUMERATE_COLLECTION:
      case aql::ExecutionNode::ENUMERATE_LIST:
      case aql::ExecutionNode::SUBQUERY:
      case aql::ExecutionNode::COLLECT:
      case aql::ExecutionNode::TRAVERSAL:
      case aql::ExecutionNode::INDEX:
      case aql::ExecutionNode::SHORTEST_PATH:
      case aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
        // we're in the loop with dependent context
        return true;
      default:
        break;
    }
  }

  return false;
}

void toVelocyPack(
    velocypack::Builder& builder,
    std::vector<arangodb::iresearch::IResearchSort> const& sorts,
    bool verbose
) {
  builder.openObject();
  // FIXME todo
  builder.close();
}

std::vector<arangodb::iresearch::IResearchSort> fromVelocyPack(
    arangodb::aql::ExecutionPlan* plan,
    arangodb::velocypack::Slice const& slice
) {
  // FIXME todo
  return {};
}

}

namespace arangodb {
namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                  IResearchViewNode implementation
// -----------------------------------------------------------------------------

IResearchViewNode::IResearchViewNode(
    aql::ExecutionPlan& plan,
    size_t id,
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view,
    arangodb::aql::Variable const& outVariable,
    arangodb::aql::AstNode* filterCondition,
    std::vector<IResearchSort>&& sortCondition)
  : arangodb::aql::ExecutionNode(&plan, id),
    _vocbase(&vocbase),
    _view(&view),
    _outVariable(&outVariable),
    // in case if filter is not specified
    // set it to surrogate 'RETURN ALL' node
    _filterCondition(filterCondition ? filterCondition : &ALL),
    _sortCondition(std::move(sortCondition)) {
  TRI_ASSERT(IResearchView::type() == _view->type());
}

IResearchViewNode::IResearchViewNode(
    aql::ExecutionPlan& plan,
    velocypack::Slice const& base)
  : aql::ExecutionNode(&plan, base),
    _vocbase(plan.getAst()->query()->vocbase()),
    _view(nullptr),
    _outVariable(aql::Variable::varFromVPack(plan.getAst(), base, "outVariable")),
    // in case if filter is not specified
    // set it to surrogate 'RETURN ALL' node
    _filterCondition(&ALL),
    _sortCondition(fromVelocyPack(&plan, base.get("sortCondition"))) {
  // FIXME how to check properly
  auto view = _vocbase->lookupView(
    basics::StringUtils::uint64(base.get("view").copyString())
  );
  TRI_ASSERT(view && IResearchView::type() == view->type());
  _view = view.get();

  auto const filterSlice = base.get("condition");

  if (!filterSlice.isEmptyObject()) {
    // AST will own the node
    _filterCondition = new aql::AstNode(
      plan.getAst(), filterSlice
    );
  }
}

void IResearchViewNode::planNodeRegisters(
    std::vector<aql::RegisterId>& nrRegsHere,
    std::vector<aql::RegisterId>& nrRegs,
    std::unordered_map<aql::VariableId, VarInfo>& varInfo,
    unsigned int& totalNrRegs,
    unsigned int depth
) const {
  nrRegsHere.emplace_back(1);
  // create a copy of the last value here
  // this is requried because back returns a reference and emplace/push_back
  // may invalidate all references
  aql::RegisterId const registerId = 1 + nrRegs.back();
  nrRegs.emplace_back(registerId);

  varInfo.emplace(_outVariable->id, VarInfo(depth, totalNrRegs++));

//  if (isInInnerLoop()) {
//    return;
//  }

  // plan registers for output scores
  for (auto const& sort : _sortCondition) {
    ++nrRegsHere[depth];
    ++nrRegs[depth];
    varInfo.emplace(sort.var->id, VarInfo(depth, totalNrRegs++));
  }
}

bool IResearchViewNode::volatile_filter() const {
  if (!filterConditionIsEmpty(_filterCondition) && isInInnerLoop()) {
    std::unordered_set<arangodb::aql::Variable const*> vars;
    return ::hasDependecies(*plan(), *_filterCondition, outVariable(), vars);
  }

  return false;
}

bool IResearchViewNode::volatile_sort() const {
  if (!_sortCondition.empty() && isInInnerLoop()) {
    std::unordered_set<aql::Variable const*> vars;

    for (auto const& sort : _sortCondition) {
      if (::hasDependecies(*plan(), *sort.node, outVariable(), vars)) {
        return true;
      }
    }
  }

  return false;
}

/// @brief toVelocyPack, for EnumerateViewNode
void IResearchViewNode::toVelocyPackHelper(
    VPackBuilder& nodes,
    bool verbose
) const {

  // call base class method
  aql::ExecutionNode::toVelocyPackHelperGeneric(nodes, verbose);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("view", VPackValue(basics::StringUtils::itoa(_view->id())));

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add(VPackValue("condition"));
  if (!filterConditionIsEmpty(_filterCondition)) {
    _filterCondition->toVelocyPack(nodes, verbose);
  } else {
    nodes.openObject();
    nodes.close();
  }

  nodes.add(VPackValue("sortCondition"));
  ::toVelocyPack(nodes, _sortCondition, verbose);

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
aql::ExecutionNode* IResearchViewNode::clone(
    aql::ExecutionPlan* plan,
    bool withDependencies,
    bool withProperties
) const {
  TRI_ASSERT(plan);

  auto* outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto node = std::make_unique<IResearchViewNode>(
    *plan,
    _id,
    *_vocbase,
    *_view,
    *outVariable,
    const_cast<aql::AstNode*>(_filterCondition),
    decltype(_sortCondition)(_sortCondition)
  );

  cloneHelper(node.get(), withDependencies, withProperties);

  return node.release();
}

/// @brief the cost of an enumerate view node
double IResearchViewNode::estimateCost(size_t& nrItems) const {
  // TODO: get a better guess from view
  return _dependencies.empty() ? 0. : _dependencies[0]->getCost(nrItems);
}

std::vector<aql::Variable const*> IResearchViewNode::getVariablesUsedHere() const {
  std::unordered_set<aql::Variable const*> vars;
  getVariablesUsedHere(vars);
  return { vars.begin(), vars.end() };
}

/// @brief getVariablesUsedHere, modifying the set in-place
void IResearchViewNode::getVariablesUsedHere(
    std::unordered_set<aql::Variable const*>& vars
) const {
  if (!filterConditionIsEmpty(_filterCondition)) {
    aql::Ast::getReferencedVariables(_filterCondition, vars);
  }
}

std::unique_ptr<aql::ExecutionBlock> IResearchViewNode::createBlock(
    aql::ExecutionEngine& engine,
    std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&,
    std::unordered_set<std::string> const&
) const {
  if (ServerState::isCoordinator()) {
    // coordinator in a cluster
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "IResearchView node is not intended to use on a coordinator"
    );
  }

  if (ServerState::isDBServer()) {
    // db server in a cluster

    // FIXME
    // retrieve master shards from all collections involved
    // and build up corresponding index reader
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  // single server
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* impl = dynamic_cast<IResearchView*>(view().getImplementation());
#else
  auto* impl = static_cast<IResearchView*>(view().getImplementation());
#endif

  if (!impl) {
    LOG_TOPIC(WARN, IResearchFeature::IRESEARCH)
      << "failed to get view implementation while creating IResearchView ExecutionBlock";

    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "failed to get view implementation while creating IResearchView ExecutionBlock"
    );
  }

  auto* trx = engine.getQuery()->trx();

  if (!trx || !(trx->state())) {
    LOG_TOPIC(WARN, IResearchFeature::IRESEARCH)
      << "failed to get transaction state while creating IResearchView ExecutionBlock";

    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "failed to get transaction state while creating IResearchView ExecutionBlock"
    );
  }

  auto& state = *(trx->state());
  auto* reader = impl->snapshot(state);

  if (!reader) {
    LOG_TOPIC(WARN, IResearchFeature::IRESEARCH)
      << "failed to get snapshot while creating IResearchView ExecutionBlock for IResearchView '" << impl->name() << "' tid '" << state.id() << "'";

    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "failed to get snapshot while creating IResearchView ExecutionBlock for IResearchView"
    );
  }

  if (_sortCondition.empty()) {
    // unordered case
    return std::make_unique<IResearchViewUnorderedBlock>(*reader, engine, *this);
  }

//FIXME uncomment when the following method will be there:
// `int getAndSkip(size_t skip, size_t& skipped, size_t read, size_t& count, AqlItemBlock*& res)`
//
//  if (!isInInnerLoop()) {
//    // optimized execution for simple queries
//    return new IResearchViewOrderedBlock(*reader, engine, *this);
//  }

  // generic case
  return std::make_unique<IResearchViewBlock>(*reader, engine, *this);
}

// -----------------------------------------------------------------------------
// --SECTION--                           ScatterIResearchViewNode implementation
// -----------------------------------------------------------------------------

IResearchViewScatterNode::IResearchViewScatterNode(
    aql::ExecutionPlan& plan,
    size_t id,
    TRI_vocbase_t& vocbase,
    LogicalView const& view
) : ExecutionNode(&plan, id),
    _vocbase(&vocbase),
    _view(&view) {
  TRI_ASSERT(IResearchView::type() == _view->type());
}

IResearchViewScatterNode::IResearchViewScatterNode(
    aql::ExecutionPlan& plan,
    arangodb::velocypack::Slice const& base
) : ExecutionNode(&plan, base),
    _vocbase(plan.getAst()->query()->vocbase()),
    //_view(plan.getAst()->query()->collections()->get(base.get("view").copyString())) { // FIXME: where to find a view
    _view(nullptr) {
  auto view = _vocbase->lookupView(
    basics::StringUtils::uint64(base.get("view").copyString())
  );

  // FIXME how to check properly
  TRI_ASSERT(view && IResearchView::type() == view->type());
  _view = view.get();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<aql::ExecutionBlock> IResearchViewScatterNode::createBlock(
    aql::ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, aql::ExecutionBlock*> const&,
    std::unordered_set<std::string> const& includedShards
) const {
  if (!ServerState::isCoordinator()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "IResearchScatterView node is intended to use on a coordinator only"
    );
  }

  // FIXME
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief toVelocyPack, for ScatterNode
void IResearchViewScatterNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, verbose);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("view", VPackValue(basics::StringUtils::itoa(_view->id())));

  // And close it
  nodes.close();
}

/// @brief estimateCost
double IResearchViewScatterNode::estimateCost(size_t& nrItems) const {
  double const depCost = _dependencies.empty()
    ? 0. 
    : _dependencies[0]->getCost(nrItems);
//  auto shardIds = _collection->shardIds();
//  size_t nrShards = shardIds->size();
  return depCost; //+ nrIterms * nrShards;
}

} // iresearch
} // arangodb
