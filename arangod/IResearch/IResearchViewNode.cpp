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

#include "IResearchCommon.h"
#include "IResearchViewCoordinator.h"
#include "IResearchViewDBServer.h"
#include "IResearchViewNode.h"
#include "IResearchViewBlock.h"
#include "IResearchOrderFactory.h"
#include "IResearchView.h"
#include "AqlHelper.h"
#include "Aql/Ast.h"
#include "Aql/BasicBlocks.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortCondition.h"
#include "Aql/Query.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "StorageEngine/TransactionState.h"

#include "velocypack/Iterator.h"

namespace {

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
aql::AstNode const ALL(arangodb::aql::AstNodeValue(true));

inline bool filterConditionIsEmpty(aql::AstNode const* filterCondition) {
  return filterCondition == &ALL;
}

// -----------------------------------------------------------------------------
// --SECTION--       helpers for std::vector<arangodb::iresearch::IResearchSort>
// -----------------------------------------------------------------------------

void toVelocyPack(
    velocypack::Builder& builder,
    std::vector<arangodb::iresearch::IResearchSort> const& sorts,
    bool verbose
) {
  VPackArrayBuilder arrayScope(&builder);
  for (auto const sort : sorts) {
    VPackObjectBuilder objectScope(&builder);
    builder.add("varId", VPackValue(sort.var->id));
    builder.add("asc", VPackValue(sort.asc));
    builder.add(VPackValue("node"));
    sort.node->toVelocyPack(builder, verbose);
  }
}

std::vector<arangodb::iresearch::IResearchSort> fromVelocyPack(
    arangodb::aql::ExecutionPlan& plan,
    arangodb::velocypack::Slice const& slice
) {
  if (!slice.isArray()) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "invalid json format detected while building IResearchViewNode sorting from velocy pack, array expected";
    return {};
  }

  auto* ast = plan.getAst();
  TRI_ASSERT(ast);
  auto const* vars = plan.getAst()->variables();
  TRI_ASSERT(vars);

  std::vector<arangodb::iresearch::IResearchSort> sorts;

  size_t i = 0;
  for (auto const sortSlice : velocypack::ArrayIterator(slice)) {
    auto const varIdSlice = sortSlice.get("varId");

    if (!varIdSlice.isNumber()) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "malformed variable identifier at line '" << i << "', number expected";
      return {};
    }

    auto const varId = varIdSlice.getNumber<aql::VariableId>();
    auto const* var = vars->getVariable(varId);

    if (!var) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "unable to find variable '" << varId << "' at line '" << i
        << "' while building IResearchViewNode sorting from velocy pack";
      return {};
    }

    auto const ascSlice = sortSlice.get("asc");

    if (!ascSlice.isBoolean()) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "malformed order mark at line " << i << "', boolean expected";
      return {};
    }

    bool const asc = ascSlice.getBoolean();

    // will be owned by Ast
    auto* node = new aql::AstNode(ast, sortSlice.get("node"));

    sorts.emplace_back(var, node, asc);
    ++i;
  }

  return sorts;
}

// -----------------------------------------------------------------------------
// --SECTION--                            helpers for IResearchViewNode::Options
// -----------------------------------------------------------------------------

void toVelocyPack(
    velocypack::Builder& builder,
    arangodb::iresearch::IResearchViewNode::Options const& options
) {
  VPackObjectBuilder objectScope(&builder);
  builder.add("waitForSync", VPackValue(options.forceSync));
}

bool fromVelocyPack(
    velocypack::Slice optionsSlice,
    arangodb::iresearch::IResearchViewNode::Options& options
) {
  if (!optionsSlice.isObject()) {
    return false;
  }

  // forceSync
  {
    auto const optionSlice = optionsSlice.get("waitForSync");

    if (!optionSlice.isBool()) {
      return false;
    }

    options.forceSync = optionSlice.getBool();
  }

  return true;
}

bool parseOptions(
    aql::AstNode const* optionsNode,
    arangodb::iresearch::IResearchViewNode::Options& options,
    std::string& error
) {
  typedef bool(*OptionHandler)(
    aql::AstNode const&,
    arangodb::iresearch::IResearchViewNode::Options&,
    std::string&
  );

  static std::map<irs::string_ref, OptionHandler> const Handlers {
    {
      "waitForSync", [](
        aql::AstNode const& value,
        arangodb::iresearch::IResearchViewNode::Options& options,
        std::string& error
      ) {
        if (!value.isValueType(aql::VALUE_TYPE_BOOL)) {
          error = "boolean value expected for 'waitForSync'";
          return false;
        }

        options.forceSync = value.getBoolValue();
        return true;
    }}
  };

  if (!optionsNode) {
    // nothing to parse
    return true;
  }

  if (aql::NODE_TYPE_OBJECT != optionsNode->type) {
    // must be an object
    return false;
  }

  const size_t n = optionsNode->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto const* attribute = optionsNode->getMemberUnchecked(i);

    if (!attribute
        || attribute->type != aql::NODE_TYPE_OBJECT_ELEMENT
        || !attribute->isValueType(aql::VALUE_TYPE_STRING)
        || !attribute->numMembers()) {
      // invalid or malformed node detected
      return false;
    }

    irs::string_ref const attributeName(
      attribute->getStringValue(), attribute->getStringLength()
    );

    auto const handler = Handlers.find(attributeName);

    if (handler == Handlers.end()) {
      // no handler found for attribute
      continue;
    }

    auto const* value = attribute->getMemberUnchecked(0);

    if (!value
        || !value->isConstant()
        || !handler->second(*value, options, error)) {
      // can't handle attribute
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     other helpers
// -----------------------------------------------------------------------------

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

/// negative value - value is dirty
/// _volatilityMask & 1 == volatile filter
/// _volatilityMask & 2 == volatile sort
int evaluateVolatility(arangodb::iresearch::IResearchViewNode const& node) {
  auto const inInnerLoop = node.isInInnerLoop();
  auto const& plan = *node.plan();
  auto const& outVariable = node.outVariable();

  std::unordered_set<arangodb::aql::Variable const*> vars;
  int mask = 0;

  // evaluate filter condition volatility
  auto& filterCondition = node.filterCondition();
  if (!::filterConditionIsEmpty(&filterCondition) && inInnerLoop) {
    irs::set_bit<0>(::hasDependecies(plan, filterCondition, outVariable, vars), mask);
  }

  // evaluate sort condition volatility
  auto& sortCondition = node.sortCondition();
  if (!sortCondition.empty() && inInnerLoop) {
    vars.clear();

    for (auto const& sort : sortCondition) {
      if (::hasDependecies(plan, *sort.node, outVariable, vars)) {
        irs::set_bit<1>(mask);
        break;
      }
    }
  }

  return mask;
}

std::function<bool(TRI_voc_cid_t)> const viewIsEmpty = [](TRI_voc_cid_t) {
  return false;
};

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
    std::shared_ptr<const arangodb::LogicalView> const& view,
    arangodb::aql::Variable const& outVariable,
    arangodb::aql::AstNode* filterCondition,
    arangodb::aql::AstNode* options,
    std::vector<IResearchSort>&& sortCondition)
  : arangodb::aql::ExecutionNode(&plan, id),
    _vocbase(vocbase),
    _view(view),
    _outVariable(&outVariable),
    // in case if filter is not specified
    // set it to surrogate 'RETURN ALL' node
    _filterCondition(filterCondition ? filterCondition : &ALL),
    _sortCondition(std::move(sortCondition)) {
  TRI_ASSERT(_view);
  TRI_ASSERT(iresearch::DATA_SOURCE_TYPE == _view->type());
  TRI_ASSERT(LogicalView::category() == _view->category());

  // FIXME any other way to validate options before object creation???
  std::string error;
  if (!parseOptions(options, _options, error)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      "invalid ArangoSearch options provided: " + error
    );
  }
}

IResearchViewNode::IResearchViewNode(
    aql::ExecutionPlan& plan,
    velocypack::Slice const& base)
  : aql::ExecutionNode(&plan, base),
    _vocbase(plan.getAst()->query()->vocbase()),
    _outVariable(aql::Variable::varFromVPack(plan.getAst(), base, "outVariable")),
    // in case if filter is not specified
    // set it to surrogate 'RETURN ALL' node
    _filterCondition(&ALL),
    _sortCondition(fromVelocyPack(plan, base.get("sortCondition"))) {
  // view
  auto const viewIdSlice = base.get("viewId");

  if (!viewIdSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      "invalid vpack format, 'viewId' attribute is intended to be a string"
    );
  }

  auto const viewId = viewIdSlice.copyString();

  if (ServerState::instance()->isSingleServer()) {
    _view = _vocbase.lookupView(basics::StringUtils::uint64(viewId));
  } else {
    // need cluster wide view
    TRI_ASSERT(ClusterInfo::instance());
    _view = ClusterInfo::instance()->getView(_vocbase.name(), viewId);
  }

  if (!_view || iresearch::DATA_SOURCE_TYPE != _view->type()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      "unable to find ArangoSearch view with id '" + viewId + "'"
    );
  }

  // filter condition
  auto const filterSlice = base.get("condition");

  if (filterSlice.isObject() && !filterSlice.isEmptyObject()) {
    // AST will own the node
    _filterCondition = new aql::AstNode(
      plan.getAst(), filterSlice
    );
  }

  // shards
  auto const shardsSlice = base.get("shards");

  if (shardsSlice.isArray()) {
    TRI_ASSERT(plan.getAst() && plan.getAst()->query());
    auto const* collections = plan.getAst()->query()->collections();
    TRI_ASSERT(collections);

    for (auto const shardSlice : velocypack::ArrayIterator(shardsSlice)) {
      auto const shardId = shardSlice.copyString(); // shardID is collection name on db server
      auto const* shard = collections->get(shardId);

      if (!shard) {
        LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
            << "unable to lookup shard '" << shardId << "' for the view '" << _view->name() << "'";
        continue;
      }

      _shards.push_back(shard->name());
    }
  } else {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "invalid 'IResearchViewNode' json format: unable to find 'shards' array";
  }

  // options
  if (!::fromVelocyPack(base.get("options"), _options)) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "failed to parse 'IResearchViewNode' options";
  }

  // volatility mask
  auto const volatilityMaskSlice = base.get("volatility");

  if (volatilityMaskSlice.isNumber()) {
    _volatilityMask = volatilityMaskSlice.getNumber<int>();
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

std::pair<bool, bool> IResearchViewNode::volatility(bool force /*=false*/) const {
  if (force || _volatilityMask < 0) {
    _volatilityMask = evaluateVolatility(*this);
  }

  return std::make_pair(
    irs::check_bit<0>(_volatilityMask), // filter
    irs::check_bit<1>(_volatilityMask)  // sort
  );
}

/// @brief toVelocyPack, for EnumerateViewNode
void IResearchViewNode::toVelocyPackHelper(
    VPackBuilder& nodes,
    unsigned flags
) const {
  // call base class method
  aql::ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  // system info
  nodes.add("database", VPackValue(_vocbase.name()));
  // need 'view' field to correctly print view name in JS explanation
  nodes.add("view", VPackValue(_view->name()));
  nodes.add("viewId", VPackValue(basics::StringUtils::itoa(_view->id())));

  // our variable
  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  // filter condition
  nodes.add(VPackValue("condition"));
  if (!::filterConditionIsEmpty(_filterCondition)) {
    _filterCondition->toVelocyPack(nodes, flags != 0);
  } else {
    nodes.openObject();
    nodes.close();
  }

  // sort condition
  nodes.add(VPackValue("sortCondition"));
  ::toVelocyPack(nodes, _sortCondition, flags != 0);

  // shards
  {
    VPackArrayBuilder arrayScope(&nodes, "shards");
    for (auto& shard: _shards) {
      nodes.add(VPackValue(shard));
    }
  }

  // options
  nodes.add(VPackValue("options"));
  ::toVelocyPack(nodes, _options);

  // volatility mask
  nodes.add("volatility", VPackValue(_volatilityMask));

  nodes.close();
}

std::vector<std::reference_wrapper<aql::Collection const>> IResearchViewNode::collections() const {
  TRI_ASSERT(_plan && _plan->getAst() && _plan->getAst()->query());
  auto const* collections = _plan->getAst()->query()->collections();
  TRI_ASSERT(collections);

  std::vector<std::reference_wrapper<aql::Collection const>> viewCollections;

  auto visitor = [&viewCollections, collections](TRI_voc_cid_t cid)->bool{
    auto const id = basics::StringUtils::itoa(cid);
    auto const* collection = collections->get(id);

    if (collection) {
      viewCollections.push_back(*collection);
    } else {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "collection with id '" << id << "' is not registered with the query";
    }

    return true;
  };

  _view->visitCollections(visitor);

  return viewCollections;
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
    _vocbase,
    _view,
    *outVariable,
    const_cast<aql::AstNode*>(_filterCondition),
    nullptr,
    decltype(_sortCondition)(_sortCondition)
  );
  node->_shards = _shards;
  node->_options = _options;
  node->_volatilityMask = _volatilityMask;

  return cloneHelper(std::move(node), withDependencies, withProperties);
}

bool IResearchViewNode::empty() const noexcept {
  return _view->visitCollections(viewIsEmpty);
}

/// @brief the cost of an enumerate view node
aql::CostEstimate IResearchViewNode::estimateCost() const {
  if (_dependencies.empty()) {
    return aql::CostEstimate::empty();
  }
  // TODO: get a better guess from view
  aql::CostEstimate estimate = _dependencies[0]->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate; 
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
  if (!::filterConditionIsEmpty(_filterCondition)) {
    aql::Ast::getReferencedVariables(_filterCondition, vars);
  }
}

void IResearchViewNode::filterCondition(aql::AstNode const* node) noexcept {
  _filterCondition = !node ? &ALL : node;
}

bool IResearchViewNode::filterConditionIsEmpty() const noexcept {
  return ::filterConditionIsEmpty(_filterCondition);
}

std::unique_ptr<aql::ExecutionBlock> IResearchViewNode::createBlock(
    aql::ExecutionEngine& engine,
    std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&
) const {
  if (ServerState::instance()->isCoordinator()) {
    // coordinator in a cluster: empty view case

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(this->empty());
#endif

    return std::make_unique<aql::NoResultsBlock>(&engine, this);
  }

  auto* trx = engine.getQuery()->trx();

  if (!trx) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to get transaction while creating IResearchView ExecutionBlock";

    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "failed to get transaction while creating IResearchView ExecutionBlock"
    );
  }

  auto& view = *this->view();
  irs::index_reader const* reader;

  LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
    << "Start getting snapshot for view '" << view.name() << "'";

  if (ServerState::instance()->isDBServer()) {
    // there are no cluster-wide transactions,
    // no place to store snapshot
    static IResearchView::Snapshot const SNAPSHOT[] {
      IResearchView::Snapshot::FindOrCreate,
      IResearchView::Snapshot::SyncAndReplace
    };

    reader = LogicalView::cast<IResearchViewDBServer>(view).snapshot(
      *trx, _shards, SNAPSHOT[size_t(_options.forceSync)]
    );
  } else {
    static IResearchView::Snapshot const SNAPSHOT[] {
      IResearchView::Snapshot::Find,
      IResearchView::Snapshot::SyncAndReplace
    };

    reader = LogicalView::cast<IResearchView>(view).snapshot(
      *trx, SNAPSHOT[size_t(_options.forceSync)]
    );
  }

  if (!reader) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to get snapshot while creating arangosearch view ExecutionBlock for view '" << view.name() << "' tid '";

    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "failed to get snapshot while creating arangosearch view ExecutionBlock"
    );
  }

  LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
    << "Finish getting snapshot for view '" << view.name() << "'";

  if (_sortCondition.empty()) {
    // unordered case
    return std::make_unique<IResearchViewUnorderedBlock>(*reader, engine, *this);
  }

  // generic case
  return std::make_unique<IResearchViewBlock>(*reader, engine, *this);
}

} // iresearch
} // arangodb
