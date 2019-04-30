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

#include "IResearchViewNode.h"

#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IResearchViewExecutor.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/types.h"
#include "Basics/NumberUtils.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
aql::AstNode const ALL(aql::AstNodeValue(true));

inline bool filterConditionIsEmpty(aql::AstNode const* filterCondition) {
  return filterCondition == &ALL;
}

// -----------------------------------------------------------------------------
// --SECTION--       helpers for std::vector<arangodb::iresearch::IResearchSort>
// -----------------------------------------------------------------------------

void toVelocyPack(velocypack::Builder& builder,
                  std::vector<Scorer> const& scorers, bool verbose) {
  VPackArrayBuilder arrayScope(&builder);
  for (auto const& scorer : scorers) {
    VPackObjectBuilder objectScope(&builder);
    builder.add("id", VPackValue(scorer.var->id));
    builder.add("name", VPackValue(scorer.var->name));  // for explainer.js
    builder.add(VPackValue("node"));
    scorer.node->toVelocyPack(builder, verbose);
  }
}

std::vector<Scorer> fromVelocyPack(aql::ExecutionPlan& plan, velocypack::Slice const& slice) {
  if (!slice.isArray()) {
    LOG_TOPIC("b50b2", ERR, arangodb::iresearch::TOPIC)
        << "invalid json format detected while building IResearchViewNode "
           "sorting from velocy pack, array expected";
    return {};
  }

  auto* ast = plan.getAst();
  TRI_ASSERT(ast);
  auto const* vars = plan.getAst()->variables();
  TRI_ASSERT(vars);

  std::vector<Scorer> scorers;

  size_t i = 0;
  for (auto const sortSlice : velocypack::ArrayIterator(slice)) {
    auto const varIdSlice = sortSlice.get("id");

    if (!varIdSlice.isNumber()) {
      LOG_TOPIC("c3790", ERR, arangodb::iresearch::TOPIC)
          << "malformed variable identifier at line '" << i << "', number expected";
      return {};
    }

    auto const varId = varIdSlice.getNumber<aql::VariableId>();
    auto const* var = vars->getVariable(varId);

    if (!var) {
      LOG_TOPIC("4eeb9", ERR, arangodb::iresearch::TOPIC)
          << "unable to find variable '" << varId << "' at line '" << i
          << "' while building IResearchViewNode sorting from velocy pack";
      return {};
    }

    // will be owned by Ast
    auto* node = new aql::AstNode(ast, sortSlice.get("node"));

    scorers.emplace_back(var, node);
    ++i;
  }

  return scorers;
}

// -----------------------------------------------------------------------------
// --SECTION--                            helpers for IResearchViewNode::Options
// -----------------------------------------------------------------------------

void toVelocyPack(velocypack::Builder& builder, IResearchViewNode::Options const& options) {
  VPackObjectBuilder objectScope(&builder);
  builder.add("waitForSync", VPackValue(options.forceSync));

  if (!options.restrictSources) {
    builder.add("collections", VPackValue(VPackValueType::Null));
  } else {
    VPackArrayBuilder arrayScope(&builder, "collections");
    for (auto const cid : options.sources) {
      builder.add(VPackValue(cid));
    }
  }
}

bool fromVelocyPack(velocypack::Slice optionsSlice, IResearchViewNode::Options& options) {
  if (optionsSlice.isNone()) {
    // no options specified
    return true;
  }

  if (!optionsSlice.isObject()) {
    return false;
  }

  // forceSync
  {
    auto const optionSlice = optionsSlice.get("waitForSync");

    if (!optionSlice.isNone()) {
      // 'waitForSync' is optional
      if (!optionSlice.isBool()) {
        return false;
      }

      options.forceSync = optionSlice.getBool();
    }
  }

  // collections
  {
    auto const optionSlice = optionsSlice.get("collections");

    if (!optionSlice.isNone() && !optionSlice.isNull()) {
      if (!optionSlice.isArray()) {
        return false;
      }

      for (auto idSlice : VPackArrayIterator(optionSlice)) {
        if (!idSlice.isNumber()) {
          return false;
        }

        auto const cid = idSlice.getNumber<TRI_voc_cid_t>();

        if (!cid) {
          return false;
        }

        options.sources.insert(cid);
      }

      options.restrictSources = true;
    }
  }

  return true;
}

bool parseOptions(aql::Query& query, LogicalView const& view, aql::AstNode const* optionsNode,
                  IResearchViewNode::Options& options, std::string& error) {
  typedef bool (*OptionHandler)(aql::Query&, LogicalView const& view, aql::AstNode const&,
                                IResearchViewNode::Options&, std::string&);

  static std::map<irs::string_ref, OptionHandler> const Handlers{
      {"collections",
       [](aql::Query& query, LogicalView const& view, aql::AstNode const& value,
          IResearchViewNode::Options& options, std::string& error) {
         if (value.isNullValue()) {
           // have nothing to restrict
           return true;
         }

         if (!value.isArray()) {
           error =
               "null value or array of strings or numbers"
               " is expected for option 'collections'";
           return false;
         }

         auto& resolver = query.resolver();
         arangodb::HashSet<TRI_voc_cid_t> sources;

         // get list of CIDs for restricted collections
         for (size_t i = 0, n = value.numMembers(); i < n; ++i) {
           auto const* sub = value.getMemberUnchecked(i);
           TRI_ASSERT(sub);

           switch (sub->value.type) {
             case aql::VALUE_TYPE_INT: {
               sources.insert(TRI_voc_cid_t(sub->getIntValue(true)));
               break;
             }

             case aql::VALUE_TYPE_STRING: {
               auto name = sub->getString();

               auto collection = resolver.getCollection(name);

               if (!collection) {
                 // check if TRI_voc_cid_t is passed as string
                 auto const cid =
                     NumberUtils::atoi_zero<TRI_voc_cid_t>(name.data(),
                                                           name.data() + name.size());

                 collection = resolver.getCollection(cid);

                 if (!collection) {
                   error = "invalid data source name '" + name +
                           "' while parsing option 'collections'";
                   return false;
                 }
               }

               sources.insert(collection->id());
               break;
             }

             default: {
               error =
                   "null value or array of strings or numbers"
                   " is expected for option 'collections'";
               return false;
             }
           }
         }

         // check if CIDs are valid
         size_t sourcesFound = 0;
         auto checkCids = [&sources, &sourcesFound](TRI_voc_cid_t cid) {
           sourcesFound += size_t(sources.contains(cid));
           return true;
         };
         view.visitCollections(checkCids);

         if (sourcesFound != sources.size()) {
           error = "only " + basics::StringUtils::itoa(sourcesFound) +
                   " out of " + basics::StringUtils::itoa(sources.size()) +
                   " provided collection(s) in option 'collections' are "
                   "registered with the view '" +
                   view.name() + "'";
           return false;
         }

         // parsing is done
         options.sources = std::move(sources);
         options.restrictSources = true;

         return true;
       }},
      {"waitForSync", [](aql::Query& /*query*/, LogicalView const& /*view*/,
                         aql::AstNode const& value,
                         IResearchViewNode::Options& options, std::string& error) {
         if (!value.isValueType(aql::VALUE_TYPE_BOOL)) {
           error = "boolean value expected for option 'waitForSync'";
           return false;
         }

         options.forceSync = value.getBoolValue();
         return true;
       }}};

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

    if (!attribute || attribute->type != aql::NODE_TYPE_OBJECT_ELEMENT ||
        !attribute->isValueType(aql::VALUE_TYPE_STRING) || !attribute->numMembers()) {
      // invalid or malformed node detected
      return false;
    }

    irs::string_ref const attributeName(attribute->getStringValue(),
                                        attribute->getStringLength());

    auto const handler = Handlers.find(attributeName);

    if (handler == Handlers.end()) {
      // no handler found for attribute
      continue;
    }

    auto const* value = attribute->getMemberUnchecked(0);

    if (!value) {
      // can't handle attribute
      return false;
    }

    if (!value->isConstant()) {
      // 'Ast::injectBindParameters` doesn't handle
      // constness of parent nodes correctly, re-evaluate flags
      value->removeFlag(aql::DETERMINED_CONSTANT);

      if (!value->isConstant()) {
        // can't handle non-const values in options
        return false;
      }
    }

    if (!handler->second(query, view, *value, options, error)) {
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
bool hasDependencies(aql::ExecutionPlan const& plan, aql::AstNode const& node,
                     aql::Variable const& ref,
                     arangodb::HashSet<aql::Variable const*>& vars) {
  if (!node.isDeterministic()) {
    return false;
  }

  vars.clear();
  aql::Ast::getReferencedVariables(&node, vars);
  vars.erase(&ref);  // remove "our" variable

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

/// @returns true if a given node is located inside a loop or subquery
bool isInInnerLoopOrSubquery(aql::ExecutionNode const& node) {
  auto* cur = &node;

  while (true) {
    auto const* dep = cur->getFirstDependency();

    if (!dep) {
      break;
    }

    switch (dep->getType()) {
      case aql::ExecutionNode::ENUMERATE_COLLECTION:
      case aql::ExecutionNode::INDEX:
      case aql::ExecutionNode::TRAVERSAL:
      case aql::ExecutionNode::ENUMERATE_LIST:
      case aql::ExecutionNode::SHORTEST_PATH:
      case aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
        // we're in a loop
        return true;
      default:
        break;
    }

    cur = dep;
  }

  TRI_ASSERT(cur);
  return cur->getType() == aql::ExecutionNode::SINGLETON
      && cur->id() != 1; // SIGNLETON nodes in subqueries have id != 1
}

/// negative value - value is dirty
/// _volatilityMask & 1 == volatile filter
/// _volatilityMask & 2 == volatile sort
int evaluateVolatility(IResearchViewNode const& node) {
  auto const inDependentScope = isInInnerLoopOrSubquery(node);
  auto const& plan = *node.plan();
  auto const& outVariable = node.outVariable();

  arangodb::HashSet<aql::Variable const*> vars;
  int mask = 0;

  // evaluate filter condition volatility
  auto& filterCondition = node.filterCondition();
  if (!::filterConditionIsEmpty(&filterCondition) && inDependentScope) {
    irs::set_bit<0>(::hasDependencies(plan, filterCondition, outVariable, vars), mask);
  }

  // evaluate sort condition volatility
  auto& scorers = node.scorers();
  if (!scorers.empty() && inDependentScope) {
    vars.clear();

    for (auto const& scorer : scorers) {
      if (::hasDependencies(plan, *scorer.node, outVariable, vars)) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
/// @note it is assumed that ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held
////////////////////////////////////////////////////////////////////////////////
class Snapshot : public IResearchView::Snapshot, private irs::util::noncopyable {
 public:
  typedef std::vector<std::pair<TRI_voc_cid_t, irs::sub_reader const*>> readers_t;

  Snapshot(readers_t&& readers, uint64_t docs_count, uint64_t live_docs_count) NOEXCEPT
      : _readers(std::move(readers)),
        _docs_count(docs_count),
        _live_docs_count(live_docs_count) {}

  /// @brief constructs snapshot from a given snapshot
  ///        according to specified set of collections
  Snapshot(const IResearchView::Snapshot& rhs,
           arangodb::HashSet<TRI_voc_cid_t> const& collections);

  /// @returns corresponding sub-reader
  virtual const irs::sub_reader& operator[](size_t i) const NOEXCEPT override {
    assert(i < readers_.size());
    return *(_readers[i].second);
  }

  virtual TRI_voc_cid_t cid(size_t i) const NOEXCEPT override {
    assert(i < readers_.size());
    return _readers[i].first;
  }

  /// @returns number of documents
  virtual uint64_t docs_count() const NOEXCEPT override { return _docs_count; }

  /// @returns number of live documents
  virtual uint64_t live_docs_count() const NOEXCEPT override {
    return _live_docs_count;
  }

  /// @returns total number of opened writers
  virtual size_t size() const NOEXCEPT override { return _readers.size(); }

 private:
  readers_t _readers;
  uint64_t _docs_count;
  uint64_t _live_docs_count;
};  // Snapshot

Snapshot::Snapshot(const IResearchView::Snapshot& rhs,
                   arangodb::HashSet<TRI_voc_cid_t> const& collections)
    : _docs_count(0), _live_docs_count(0) {
  for (size_t i = 0, size = rhs.size(); i < size; ++i) {
    auto const cid = rhs.cid(i);

    if (!collections.contains(cid)) {
      continue;
    }

    auto& segment = rhs[i];

    _docs_count += segment.docs_count();
    _live_docs_count += segment.live_docs_count();
    _readers.emplace_back(cid, &segment);
  }
}

typedef std::shared_ptr<IResearchView::Snapshot const> SnapshotPtr;

/// @brief Since cluster is not transactional and each distributed
///        part of a query starts it's own trasaction associated
///        with global query identifier, there is no single place
///        to store a snapshot and we do the following:
///
///   1. Each query part on DB server gets the list of shards
///      to be included into a query and starts its own transaction
///
///   2. Given the list of shards we take view snapshot according
///      to the list of restricted data sources specified in options
///      of corresponding IResearchViewNode
///
///   3. If waitForSync is specified, we refresh snapshot
///      of each shard we need and finally put it to transaction
///      associated to a part of the distributed query. We use
///      default snapshot key if there are no restricted sources
///      specified in options or IResearchViewNode address otherwise
///
///      Custom key is needed for the following query
///      (assume 'view' is lined with 'c1' and 'c2' in the example below):
/// 		FOR d IN view OPTIONS { collections : [ 'c1' ] }
/// 		FOR x IN view OPTIONS { collections : [ 'c2' ] }
/// 		RETURN {d, x}
///
SnapshotPtr snapshotDBServer(IResearchViewNode const& node, transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  static IResearchView::SnapshotMode const SNAPSHOT[]{IResearchView::SnapshotMode::FindOrCreate,
                                                      IResearchView::SnapshotMode::SyncAndReplace};

  auto& view = LogicalView::cast<IResearchView>(*node.view());
  auto& options = node.options();
  auto* resolver = trx.resolver();
  TRI_ASSERT(resolver);

  arangodb::HashSet<TRI_voc_cid_t> collections;
  for (auto& shard : node.shards()) {
    auto collection = resolver->getCollection(shard);

    if (!collection) {
      THROW_ARANGO_EXCEPTION(
          arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                           std::string("failed to find shard by id '") + shard + "'"));
    }

    if (options.restrictSources && !options.sources.contains(collection->planId())) {
      // skip restricted collections if any
      continue;
    }

    collections.emplace(collection->id());
  }

  void const* snapshotKey = nullptr;

  if (options.restrictSources) {
    // use node address as the snapshot identifier
    snapshotKey = &node;
  }

  // use aliasing ctor
  return {SnapshotPtr(), view.snapshot(trx, SNAPSHOT[size_t(options.forceSync)],
                                       &collections, snapshotKey)};
}

/// @brief Since single-server is transactional we do the following:
///
///   1. When transaction starts we put index snapshot into it
///
///   2. If waitForSync is specified, we refresh snapshot
///      taken in (1), object itself remains valid
///
///   3. If there are no restricted sources in a query, we reuse
///      snapshot taken in (1),
///      otherwise we reassemble restricted snapshot based on the
///      original one taken in (1) and return it
///
SnapshotPtr snapshotSingleServer(IResearchViewNode const& node, transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isSingleServer());

  static IResearchView::SnapshotMode const SNAPSHOT[]{IResearchView::SnapshotMode::Find,
                                                      IResearchView::SnapshotMode::SyncAndReplace};

  auto& view = LogicalView::cast<IResearchView>(*node.view());
  auto& options = node.options();

  // use aliasing ctor
  auto reader = SnapshotPtr(SnapshotPtr(),
                            view.snapshot(trx, SNAPSHOT[size_t(options.forceSync)]));

  if (options.restrictSources && reader) {
    // reassemble reader
    reader = std::make_shared<Snapshot>(*reader, options.sources);
  }

  return reader;
}

}  // namespace

namespace arangodb {
namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                  IResearchViewNode implementation
// -----------------------------------------------------------------------------

IResearchViewNode::IResearchViewNode(aql::ExecutionPlan& plan, size_t id,
                                     TRI_vocbase_t& vocbase,
                                     std::shared_ptr<const LogicalView> const& view,
                                     aql::Variable const& outVariable,
                                     aql::AstNode* filterCondition,
                                     aql::AstNode* options, std::vector<Scorer>&& scorers)
    : aql::ExecutionNode(&plan, id),
      _vocbase(vocbase),
      _view(view),
      _outVariable(&outVariable),
      // in case if filter is not specified
      // set it to surrogate 'RETURN ALL' node
      _filterCondition(filterCondition ? filterCondition : &ALL),
      _scorers(std::move(scorers)) {
  TRI_ASSERT(_view);
  TRI_ASSERT(iresearch::DATA_SOURCE_TYPE == _view->type());
  TRI_ASSERT(LogicalView::category() == _view->category());

  auto* ast = plan.getAst();
  TRI_ASSERT(ast && ast->query());

  // FIXME any other way to validate options before object creation???
  std::string error;
  if (!parseOptions(*ast->query(), *_view, options, _options, error)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid ArangoSearch options provided: " + error);
  }
}

IResearchViewNode::IResearchViewNode(aql::ExecutionPlan& plan, velocypack::Slice const& base)
    : aql::ExecutionNode(&plan, base),
      _vocbase(plan.getAst()->query()->vocbase()),
      _outVariable(
          aql::Variable::varFromVPack(plan.getAst(), base, "outVariable")),
      // in case if filter is not specified
      // set it to surrogate 'RETURN ALL' node
      _filterCondition(&ALL),
      _scorers(fromVelocyPack(plan, base.get("scorers"))) {
  // view
  auto const viewIdSlice = base.get("viewId");

  if (!viewIdSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "invalid vpack format, 'viewId' attribute is intended to be a string");
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
        "unable to find ArangoSearch view with id '" + viewId + "'");
  }

  // filter condition
  auto const filterSlice = base.get("condition");

  if (filterSlice.isObject() && !filterSlice.isEmptyObject()) {
    // AST will own the node
    _filterCondition = new aql::AstNode(plan.getAst(), filterSlice);
  }

  // shards
  auto const shardsSlice = base.get("shards");

  if (shardsSlice.isArray()) {
    TRI_ASSERT(plan.getAst() && plan.getAst()->query());
    auto const* collections = plan.getAst()->query()->collections();
    TRI_ASSERT(collections);

    for (auto const shardSlice : velocypack::ArrayIterator(shardsSlice)) {
      auto const shardId = shardSlice.copyString();  // shardID is collection name on db server
      auto const* shard = collections->get(shardId);

      if (!shard) {
        LOG_TOPIC("6fba2", ERR, arangodb::iresearch::TOPIC)
            << "unable to lookup shard '" << shardId << "' for the view '"
            << _view->name() << "'";
        continue;
      }

      _shards.push_back(shard->name());
    }
  } else {
    LOG_TOPIC("a48f3", ERR, arangodb::iresearch::TOPIC)
        << "invalid 'IResearchViewNode' json format: unable to find 'shards' "
           "array";
  }

  // options
  TRI_ASSERT(plan.getAst() && plan.getAst()->query());

  auto const options = base.get("options");

  if (!::fromVelocyPack(options, _options)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "failed to parse 'IResearchViewNode' options: " + options.toString());
  }

  // volatility mask
  auto const volatilityMaskSlice = base.get("volatility");

  if (volatilityMaskSlice.isNumber()) {
    _volatilityMask = volatilityMaskSlice.getNumber<int>();
  }
}

void IResearchViewNode::planNodeRegisters(std::vector<aql::RegisterId>& nrRegsHere,
                                          std::vector<aql::RegisterId>& nrRegs,
                                          std::unordered_map<aql::VariableId, VarInfo>& varInfo,
                                          unsigned int& totalNrRegs,
                                          unsigned int depth) const {
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
  for (auto const& scorer : _scorers) {
    ++nrRegsHere[depth];
    ++nrRegs[depth];
    varInfo.emplace(scorer.var->id, VarInfo(depth, totalNrRegs++));
  }
}

std::pair<bool, bool> IResearchViewNode::volatility(bool force /*=false*/) const {
  if (force || _volatilityMask < 0) {
    _volatilityMask = evaluateVolatility(*this);
  }

  return std::make_pair(irs::check_bit<0>(_volatilityMask),  // filter
                        irs::check_bit<1>(_volatilityMask)   // sort
  );
}

/// @brief toVelocyPack, for EnumerateViewNode
void IResearchViewNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
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
  nodes.add(VPackValue("scorers"));
  ::toVelocyPack(nodes, _scorers, flags != 0);

  // shards
  {
    VPackArrayBuilder arrayScope(&nodes, "shards");
    for (auto& shard : _shards) {
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

  auto visitor = [&viewCollections, collections](TRI_voc_cid_t cid) -> bool {
    auto const id = basics::StringUtils::itoa(cid);
    auto const* collection = collections->get(id);

    if (collection) {
      viewCollections.push_back(*collection);
    } else {
      LOG_TOPIC("ee270", WARN, arangodb::iresearch::TOPIC)
          << "collection with id '" << id << "' is not registered with the query";
    }

    return true;
  };

  if (_options.restrictSources) {
    viewCollections.reserve(_options.sources.size());
    for (auto const cid : _options.sources) {
      visitor(cid);
    }
  } else {
    _view->visitCollections(visitor);
  }

  return viewCollections;
}

/// @brief clone ExecutionNode recursively
aql::ExecutionNode* IResearchViewNode::clone(aql::ExecutionPlan* plan, bool withDependencies,
                                             bool withProperties) const {
  TRI_ASSERT(plan);

  auto* outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto node =
      std::make_unique<IResearchViewNode>(*plan, _id, _vocbase, _view, *outVariable,
                                          const_cast<aql::AstNode*>(_filterCondition),
                                          nullptr, decltype(_scorers)(_scorers));
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

/// @brief getVariablesUsedHere, modifying the set in-place
void IResearchViewNode::getVariablesUsedHere(arangodb::HashSet<aql::Variable const*>& vars) const {
  if (!::filterConditionIsEmpty(_filterCondition)) {
    aql::Ast::getReferencedVariables(_filterCondition, vars);
  }

  for (auto& scorer : _scorers) {
    aql::Ast::getReferencedVariables(scorer.node, vars);
  }
}

std::shared_ptr<std::unordered_set<aql::RegisterId>> IResearchViewNode::calcInputRegs() const {
  std::shared_ptr<std::unordered_set<aql::RegisterId>> inputRegs =
      std::make_shared<std::unordered_set<aql::RegisterId>>();

  if (!::filterConditionIsEmpty(_filterCondition)) {
    arangodb::HashSet<aql::Variable const*> vars;
    aql::Ast::getReferencedVariables(_filterCondition, vars);

    for (auto const& it : vars) {
      aql::RegisterId reg = varToRegUnchecked(*it);
      // The filter condition may refer to registers that are written here
      if (reg < getNrInputRegisters()) {
        inputRegs->emplace(reg);
      }
    }
  }

  return inputRegs;
}

void IResearchViewNode::filterCondition(aql::AstNode const* node) noexcept {
  _filterCondition = !node ? &ALL : node;
}

bool IResearchViewNode::filterConditionIsEmpty() const noexcept {
  return ::filterConditionIsEmpty(_filterCondition);
}

std::unique_ptr<aql::ExecutionBlock> IResearchViewNode::createBlock(
    aql::ExecutionEngine& engine,
    std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&) const {
  if (ServerState::instance()->isCoordinator()) {
    // coordinator in a cluster: empty view case

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(ServerState::instance()->isCoordinator());
#endif
    aql::ExecutionNode const* previousNode = getFirstDependency();
    TRI_ASSERT(previousNode != nullptr);
    auto it = getRegisterPlan()->varInfo.find(outVariable().id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    aql::RegisterId outputRegister = it->second.registerId;
    aql::ExecutorInfos infos(arangodb::aql::make_shared_unordered_set(),
                             arangodb::aql::make_shared_unordered_set({outputRegister}),
                             getRegisterPlan()->nrRegs[previousNode->getDepth()],
                             getRegisterPlan()->nrRegs[getDepth()],
                             getRegsToClear(), calcRegsToKeep());

    return std::make_unique<aql::ExecutionBlockImpl<aql::NoResultsExecutor>>(&engine, this,
                                                                             std::move(infos));
  }

  auto* trx = engine.getQuery()->trx();

  if (!trx) {
    LOG_TOPIC("7c905", WARN, arangodb::iresearch::TOPIC)
        << "failed to get transaction while creating IResearchView "
           "ExecutionBlock";

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "failed to get transaction while creating "
                                   "IResearchView ExecutionBlock");
  }

  auto& view = LogicalView::cast<IResearchView>(*this->view());

  std::shared_ptr<IResearchView::Snapshot const> reader;

  LOG_TOPIC("82af6", TRACE, arangodb::iresearch::TOPIC)
      << "Start getting snapshot for view '" << view.name() << "'";

  if (options().forceSync &&
      trx->state()->hasHint(arangodb::transaction::Hints::Hint::GLOBAL_MANAGED)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "cannot use waitForSync with "
                                   "views and transactions");
  }
  
  // we manage snapshot differently in single-server/db server,
  // see description of functions below to learn how
  if (ServerState::instance()->isDBServer()) {
    reader = snapshotDBServer(*this, *trx);
  } else {
    reader = snapshotSingleServer(*this, *trx);
  }

  if (!reader) {
    LOG_TOPIC("9bb93", WARN, arangodb::iresearch::TOPIC)
        << "failed to get snapshot while creating arangosearch view "
           "ExecutionBlock for view '"
        << view.name() << "'";

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "failed to get snapshot while creating "
                                   "arangosearch view ExecutionBlock");
  }

  if (0 == reader->size()) {
    // nothing to query
    aql::ExecutionNode const* previousNode = getFirstDependency();
    TRI_ASSERT(previousNode != nullptr);
    auto it = getRegisterPlan()->varInfo.find(outVariable().id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    aql::RegisterId outputRegister = it->second.registerId;
    aql::ExecutorInfos infos(arangodb::aql::make_shared_unordered_set(),
                             arangodb::aql::make_shared_unordered_set({outputRegister}),
                             getRegisterPlan()->nrRegs[previousNode->getDepth()],
                             getRegisterPlan()->nrRegs[getDepth()],
                             getRegsToClear(), calcRegsToKeep());

    return std::make_unique<aql::ExecutionBlockImpl<aql::NoResultsExecutor>>(&engine, this,
                                                                             std::move(infos));
  }

  LOG_TOPIC("33853", TRACE, arangodb::iresearch::TOPIC)
      << "Finish getting snapshot for view '" << view.name() << "'";

  bool const ordered = !_scorers.empty();

  // We have one output register for documents, which is always the first after
  // the input registers.
  aql::RegisterId const firstOutputRegister = getNrInputRegisters();
  auto numScoreRegisters = static_cast<aql::RegisterId>(_scorers.size());

  // We have one additional output register for each scorer, consecutively after
  // the output register for documents. These must of course fit in the
  // available registers. There may be unused registers reserved for later
  // blocks.
  TRI_ASSERT(getNrInputRegisters() + 1 + numScoreRegisters <= getNrOutputRegisters());
  std::shared_ptr<std::unordered_set<aql::RegisterId>> writableOutputRegisters =
      aql::make_shared_unordered_set();
  writableOutputRegisters->reserve(1 + numScoreRegisters);
  for (aql::RegisterId reg = firstOutputRegister;
       reg < firstOutputRegister + numScoreRegisters + 1; reg++) {
    writableOutputRegisters->emplace(reg);
  }
  TRI_ASSERT(writableOutputRegisters->size() == 1 + numScoreRegisters);
  TRI_ASSERT(writableOutputRegisters->begin() != writableOutputRegisters->end());
  TRI_ASSERT(firstOutputRegister == *std::min_element(writableOutputRegisters->begin(),
                                                      writableOutputRegisters->end()));
  aql::ExecutorInfos infos = createRegisterInfos(calcInputRegs(), std::move(writableOutputRegisters));

  aql::IResearchViewExecutorInfos executorInfos{std::move(infos),
                                                reader,
                                                firstOutputRegister,
                                                numScoreRegisters,
                                                *engine.getQuery(),
                                                scorers(),
                                                *plan(),
                                                outVariable(),
                                                filterCondition(),
                                                volatility(),
                                                getRegisterPlan()->varInfo,
                                                getDepth()};

  if (!ordered) {
    return std::make_unique<aql::ExecutionBlockImpl<aql::IResearchViewExecutor<false>>>(
        &engine, this, std::move(executorInfos));
  }
  return std::make_unique<aql::ExecutionBlockImpl<aql::IResearchViewExecutor<true>>>(
      &engine, this, std::move(executorInfos));
}

}  // namespace iresearch
}  // namespace arangodb
