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

#include "ClusterNodes.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/BlocksWithClients.h"
#include "Aql/Collection.h"
#include "Aql/DistributeExecutor.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/GraphNode.h"
#include "Aql/IdExecutor.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/ParallelUnsortedGatherExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RemoteExecutor.h"
#include "Aql/ScatterExecutor.h"
#include "Aql/SingleRemoteModificationExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/SortingGatherExecutor.h"
#include "Aql/UnsortedGatherExecutor.h"
#include "Aql/types.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>

#include <string_view>
#include <type_traits>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;

namespace {

constexpr std::string_view kSortModeUnset("unset");
constexpr std::string_view kSortModeMinElement("minelement");
constexpr std::string_view kSortModeHeap("heap");

constexpr std::string_view kParallelismParallel("parallel");
constexpr std::string_view kParallelismSerial("serial");
constexpr std::string_view kParallelismUndefined("undefined");

constexpr std::string_view toString(GatherNode::Parallelism value) noexcept {
  switch (value) {
    case GatherNode::Parallelism::Parallel:
      return kParallelismParallel;
    case GatherNode::Parallelism::Serial:
      return kParallelismSerial;
    case GatherNode::Parallelism::Undefined:
    default:
      return kParallelismUndefined;
  }
}

constexpr GatherNode::Parallelism parallelismFromString(
    std::string_view value) noexcept {
  if (value == kParallelismParallel) {
    return GatherNode::Parallelism::Parallel;
  } else if (value == kParallelismSerial) {
    return GatherNode::Parallelism::Serial;
  }
  return GatherNode::Parallelism::Undefined;
}

std::map<std::string_view, GatherNode::SortMode> const NameToValue{
    {kSortModeMinElement, GatherNode::SortMode::MinElement},
    {kSortModeHeap, GatherNode::SortMode::Heap},
    {kSortModeUnset, GatherNode::SortMode::Default}};

bool toSortMode(std::string_view str, GatherNode::SortMode& mode) noexcept {
  // std::map ~25-30% faster than std::unordered_map for small number of
  // elements
  auto const it = NameToValue.find(str);

  if (it == NameToValue.end()) {
    TRI_ASSERT(false);
    return false;
  }

  mode = it->second;
  return true;
}

std::string_view toString(GatherNode::SortMode mode) noexcept {
  switch (mode) {
    case GatherNode::SortMode::MinElement:
      return kSortModeMinElement;
    case GatherNode::SortMode::Heap:
      return kSortModeHeap;
    case GatherNode::SortMode::Default:
      return kSortModeUnset;
    default:
      TRI_ASSERT(false);
      return {};
  }
}

}  // namespace

/// @brief constructor for RemoteNode
RemoteNode::RemoteNode(ExecutionPlan* plan,
                       arangodb::velocypack::Slice const& base)
    : DistributeConsumerNode(plan, base),
      _vocbase(&(plan->getAst()->query().vocbase())),
      _server(base.get("server").copyString()),
      _queryId(base.get("queryId").copyString()) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> RemoteNode::createBlock(
    ExecutionEngine& engine) const {
  auto const nrOutRegs = getRegisterPlan()->nrRegs[getDepth()];
  auto const nrInRegs = nrOutRegs;

  auto regsToKeep = getRegsToKeepStack();
  auto regsToClear = getRegsToClear();

  // Everything that is cleared here could and should have been cleared before,
  // i.e. before sending it over the network.
  TRI_ASSERT(regsToClear.empty());

  auto infos = RegisterInfos({}, {}, nrInRegs, nrOutRegs,
                             std::move(regsToClear), std::move(regsToKeep));

  return std::make_unique<ExecutionBlockImpl<RemoteExecutor>>(
      &engine, this, std::move(infos), server(), getDistributeId(), queryId());
}

/// @brief doToVelocyPack, for RemoteNode
void RemoteNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  DistributeConsumerNode::doToVelocyPack(nodes, flags);

  nodes.add("server", VPackValue(_server));
  nodes.add("queryId", VPackValue(_queryId));
}

/// @brief estimateCost
CostEstimate RemoteNode::estimateCost() const {
  if (_dependencies.size() == 1) {
    CostEstimate estimate = _dependencies[0]->getCost();
    estimate.estimatedCost += estimate.estimatedNrItems;
    return estimate;
  }
  // We really should not get here, but if so, do something bordering on
  // sensible:
  CostEstimate estimate = CostEstimate::empty();
  estimate.estimatedNrItems = 1;
  estimate.estimatedCost = 1.0;
  return estimate;
}

size_t RemoteNode::getMemoryUsedBytes() const { return sizeof(*this); }

/// @brief construct a scatter node
ScatterNode::ScatterNode(ExecutionPlan* plan, ExecutionNodeId id,
                         ScatterType type)
    : ExecutionNode(plan, id), _type(type) {}

ScatterNode::ScatterNode(ExecutionPlan* plan,
                         arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {
  readClientsFromVelocyPack(base);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ScatterNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});
  auto executorInfos = ScatterExecutorInfos(_clients);

  return std::make_unique<ExecutionBlockImpl<ScatterExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* ScatterNode::clone(ExecutionPlan* plan,
                                  bool withDependencies) const {
  auto c = std::make_unique<ScatterNode>(plan, _id, getScatterType());
  c->copyClients(clients());
  return cloneHelper(std::move(c), withDependencies);
}

/// @brief doToVelocyPack, for ScatterNode
void ScatterNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  // serialize clients
  writeClientsToVelocyPack(nodes);
}

size_t ScatterNode::getMemoryUsedBytes() const { return sizeof(*this); }

bool ScatterNode::readClientsFromVelocyPack(VPackSlice base) {
  auto const clientsSlice = base.get("clients");

  if (!clientsSlice.isArray()) {
    LOG_TOPIC("49ba1", ERR, Logger::AQL)
        << "invalid serialized ScatterNode definition, 'clients' attribute is "
           "expected to be an array of string";
    return false;
  }

  size_t pos = 0;
  for (auto const clientSlice : velocypack::ArrayIterator(clientsSlice)) {
    if (!clientSlice.isString()) {
      LOG_TOPIC("c6131", ERR, Logger::AQL)
          << "invalid serialized ScatterNode definition, 'clients' attribute "
             "is expected to be an array of string but got not a string at "
             "line "
          << pos;
      _clients.clear();  // clear malformed node
      return false;
    }

    _clients.emplace_back(clientSlice.copyString());
    ++pos;
  }

  _type = static_cast<ScatterNode::ScatterType>(
      basics::VelocyPackHelper::getNumericValue<uint64_t>(base, "scatterType",
                                                          0));

  return true;
}

void ScatterNode::writeClientsToVelocyPack(VPackBuilder& builder) const {
  builder.add("scatterType",
              VPackValue(static_cast<uint64_t>(getScatterType())));
  VPackArrayBuilder arrayScope(&builder, "clients");
  for (auto const& client : _clients) {
    builder.add(VPackValue(client));
  }
}

/// @brief estimateCost
CostEstimate ScatterNode::estimateCost() const {
  CostEstimate estimate = _dependencies[0]->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems * _clients.size();
  return estimate;
}

DistributeNode::DistributeNode(ExecutionPlan* plan, ExecutionNodeId id,
                               ScatterNode::ScatterType type,
                               Collection const* collection,
                               Variable const* variable,
                               ExecutionNodeId targetNodeId)
    : ScatterNode(plan, id, type),
      CollectionAccessingNode(collection),
      _variable(variable),
      _attribute(determineProjectionAttribute()),
      _targetNodeId(targetNodeId) {}

/// @brief construct a distribute node
DistributeNode::DistributeNode(ExecutionPlan* plan,
                               arangodb::velocypack::Slice const& base)
    : ScatterNode(plan, base),
      CollectionAccessingNode(plan, base),
      _variable(Variable::varFromVPack(plan->getAst(), base, "variable")) {
  if (auto sats = base.get("satelliteCollections"); sats.isArray()) {
    auto& queryCols = plan->getAst()->query().collections();
    _satellites.reserve(sats.length());

    for (VPackSlice it : VPackArrayIterator(sats)) {
      std::string v =
          arangodb::basics::VelocyPackHelper::getStringValue(it, "");
      auto c = queryCols.add(v, AccessMode::Type::READ,
                             aql::Collection::Hint::Collection);
      addSatellite(c);
    }
  }

  if (auto attr = base.get("attribute"); attr.isArray()) {
    for (VPackSlice it : VPackArrayIterator(attr)) {
      _attribute.push_back(it.copyString());
    }
  }
}

/// @brief clone ExecutionNode recursively
ExecutionNode* DistributeNode::clone(ExecutionPlan* plan,
                                     bool withDependencies) const {
  auto c = std::make_unique<DistributeNode>(
      plan, _id, getScatterType(), collection(), _variable, _targetNodeId);
  c->copyClients(clients());
  CollectionAccessingNode::cloneInto(*c);
  c->_satellites.reserve(_satellites.size());
  for (auto& it : _satellites) {
    c->_satellites.emplace_back(it);
  }

  return cloneHelper(std::move(c), withDependencies);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> DistributeNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  // get the variable to inspect . . .
  TRI_ASSERT(_variable != nullptr);
  VariableId varId = _variable->id;

  // get the register id of the variable to inspect . . .
  auto it = getRegisterPlan()->varInfo.find(varId);
  TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
  RegisterId regId = (*it).second.registerId;

  TRI_ASSERT(regId.isValid());

  auto inRegs = RegIdSet{regId};
  auto registerInfos = createRegisterInfos(std::move(inRegs), {});
  auto infos =
      DistributeExecutorInfos(clients(), collection(), regId, _attribute,
                              getScatterType(), getSatellites());

  return std::make_unique<ExecutionBlockImpl<DistributeExecutor>>(
      &engine, this, std::move(registerInfos), std::move(infos));
}

void DistributeNode::setVariable(Variable const* var) {
  _variable = var;
  determineProjectionAttribute();
}

std::vector<std::string> DistributeNode::determineProjectionAttribute() const {
  std::vector<std::string> attribute;
  // check where input variable of DISTRIBUTE comes from.
  // if it is from a projection, we also look up the projection's
  // attribute name
  TRI_ASSERT(_variable != nullptr);
  auto setter = plan()->getVarSetBy(_variable->id);
  if (setter != nullptr &&
      (setter->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
       setter->getType() == ExecutionNode::INDEX)) {
    DocumentProducingNode const* document =
        dynamic_cast<DocumentProducingNode const*>(setter);
    auto const& p = document->projections();
    for (size_t i = 0; i < p.size(); ++i) {
      if (p[i].variable == _variable) {
        // found the DISTRIBUTE input variable in a projection
        attribute = p[i].path.get();
        break;
      }
    }
  }

  return attribute;
}

/// @brief doToVelocyPack, for DistributeNode
void DistributeNode::doToVelocyPack(VPackBuilder& builder,
                                    unsigned flags) const {
  // call base class method
  ScatterNode::doToVelocyPack(builder, flags);
  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue("variable"));
  _variable->toVelocyPack(builder);

  builder.add("attribute", VPackValue(VPackValueType::Array));
  for (auto const& it : _attribute) {
    builder.add(VPackValue(it));
  }
  builder.close();  // "attribute"

  if (!_satellites.empty()) {
    builder.add(VPackValue("satelliteCollections"));
    {
      VPackArrayBuilder guard(&builder);
      for (auto const& v : _satellites) {
        // if the mapped shard for a collection is empty, it means that
        // we have a vertex collection that is only relevant on some of the
        // target servers
        builder.add(VPackValue(v->name()));
      }
    }
  }
}

void DistributeNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _variable = Variable::replace(_variable, replacements);
  _attribute = determineProjectionAttribute();
}

/// @brief getVariablesUsedHere, modifying the set in-place
void DistributeNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_variable);
}

/// @brief estimateCost
CostEstimate DistributeNode::estimateCost() const {
  CostEstimate estimate = _dependencies[0]->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

void DistributeNode::addSatellite(aql::Collection* satellite) {
  // Only relevant for enterprise disjoint smart graphs
  TRI_ASSERT(satellite->isSatellite());
  _satellites.emplace_back(satellite);
}

size_t DistributeNode::getMemoryUsedBytes() const { return sizeof(*this); }

/*static*/ Collection const* GatherNode::findCollection(
    GatherNode const& root) noexcept {
  ExecutionNode const* node = root.getFirstDependency();

  auto remotesSeen = 0;

  while (node) {
    switch (node->getType()) {
      case UPDATE:
      case REMOVE:
      case INSERT:
      case UPSERT:
      case REPLACE:
      case MATERIALIZE:
      case TRAVERSAL:
      case SHORTEST_PATH:
      case ENUMERATE_PATHS:
      case INDEX:
      case ENUMERATE_COLLECTION: {
        auto const* cNode = castTo<CollectionAccessingNode const*>(node);
        if (!cNode->isUsedAsSatellite() &&
            cNode->prototypeCollection() == nullptr) {
          return cNode->collection();
        }
        break;
      }
      case ENUMERATE_IRESEARCH_VIEW:
        // Views are instantiated per DBServer, not per Shard, and are not
        // CollectionAccessingNodes. And we don't know the number of DBServers
        // at this point.
        return nullptr;
      case REMOTE:
        ++remotesSeen;
        if (remotesSeen > 1) {
          TRI_ASSERT(false);
          return nullptr;  // diamond boundary
        }
        break;
      case SCATTER:
      case DISTRIBUTE:
        TRI_ASSERT(false);
        return nullptr;  // diamond boundary
      case REMOTESINGLE:
      case REMOTE_MULTIPLE:
        // While being a CollectionAccessingNode, it lives on the Coordinator.
        // However it should thus not be encountered here.
        TRI_ASSERT(false);
        return nullptr;
      default:
        break;
    }

    node = node->getFirstDependency();
  }

  return nullptr;
}

/// @brief construct a gather node
GatherNode::GatherNode(ExecutionPlan* plan,
                       arangodb::velocypack::Slice const& base,
                       SortElementVector const& elements)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query().vocbase())),
      _elements(elements),
      _sortmode(SortMode::MinElement),
      _parallelism(Parallelism::Undefined),
      _limit(0) {
  if (!_elements.empty()) {
    auto const sortModeSlice = base.get("sortmode");

    if (!toSortMode(
            VelocyPackHelper::getStringView(sortModeSlice, std::string_view()),
            _sortmode)) {
      LOG_TOPIC("2c6f3", ERR, Logger::AQL)
          << "invalid sort mode detected while "
             "creating 'GatherNode' from vpack";
    }

    _limit =
        VelocyPackHelper::getNumericValue<decltype(_limit)>(base, "limit", 0);
  }

  setParallelism(parallelismFromString(
      VelocyPackHelper::getStringValue(base, StaticStrings::Parallelism, "")));
}

GatherNode::GatherNode(ExecutionPlan* plan, ExecutionNodeId id,
                       SortMode sortMode, Parallelism parallelism) noexcept
    : ExecutionNode(plan, id),
      _vocbase(&(plan->getAst()->query().vocbase())),
      _sortmode(sortMode),
      _parallelism(parallelism),
      _limit(0) {}

/// @brief doToVelocyPack, for GatherNode
void GatherNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  nodes.add("parallelism", VPackValue(toString(_parallelism)));

  if (_elements.empty()) {
    nodes.add("sortmode", VPackValue(kSortModeUnset));
  } else {
    nodes.add("sortmode", VPackValue(toString(_sortmode)));
    nodes.add("limit", VPackValue(_limit));
  }

  nodes.add(VPackValue("elements"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _elements) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("inVariable"));
      it.var->toVelocyPack(nodes);
      nodes.add("ascending", VPackValue(it.ascending));
      if (!it.attributePath.empty()) {
        nodes.add(VPackValue("path"));
        VPackArrayBuilder arr(&nodes);
        for (auto const& a : it.attributePath) {
          nodes.add(VPackValue(a));
        }
      }
    }
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> GatherNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  auto registerInfos = createRegisterInfos({}, {});
  if (_elements.empty()) {
    TRI_ASSERT(getRegisterPlan()->nrRegs[previousNode->getDepth()] ==
               getRegisterPlan()->nrRegs[getDepth()]);
    if (_parallelism == Parallelism::Parallel) {
      return std::make_unique<
          ExecutionBlockImpl<ParallelUnsortedGatherExecutor>>(
          &engine, this, std::move(registerInfos), EmptyExecutorInfos());
    } else {
      auto executorInfos = IdExecutorInfos(false);

      return std::make_unique<ExecutionBlockImpl<UnsortedGatherExecutor>>(
          &engine, this, std::move(registerInfos), std::move(executorInfos));
    }
  }

  Parallelism p = _parallelism;
  if (ServerState::instance()->isDBServer()) {
    p = Parallelism::Serial;  // not supported in v36
  }

  std::vector<SortRegister> sortRegister;
  SortRegister::fill(*plan(), *getRegisterPlan(), _elements, sortRegister);

  auto executorInfos = SortingGatherExecutorInfos(
      std::move(sortRegister), _plan->getAst()->query(), sortMode(),
      constrainedSortLimit(), p);

  return std::make_unique<ExecutionBlockImpl<SortingGatherExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief estimateCost
CostEstimate GatherNode::estimateCost() const {
  CostEstimate estimate = _dependencies[0]->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

void GatherNode::setConstrainedSortLimit(size_t limit) noexcept {
  _limit = limit;
}

size_t GatherNode::constrainedSortLimit() const noexcept { return _limit; }

bool GatherNode::isSortingGather() const noexcept {
  return !elements().empty();
}

void GatherNode::setParallelism(GatherNode::Parallelism value) {
  _parallelism = value;
}

GatherNode::SortMode GatherNode::evaluateSortMode(
    size_t numberOfShards, size_t shardsRequiredForHeapMerge) noexcept {
  return numberOfShards >= shardsRequiredForHeapMerge ? SortMode::Heap
                                                      : SortMode::MinElement;
}

GatherNode::Parallelism GatherNode::evaluateParallelism(
    Collection const& collection) noexcept {
  // single-sharded collections don't require any parallelism. collections with
  // more than one shard are eligible for later parallelization (the Undefined
  // allows this)
  return (collection.numberOfShards() == 1 || collection.isSatellite())
             ? Parallelism::Serial
             : Parallelism::Undefined;
}

void GatherNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  for (auto& it : _elements) {
    it.var = Variable::replace(it.var, replacements);
  }
}

void GatherNode::replaceAttributeAccess(ExecutionNode const* self,
                                        Variable const* searchVariable,
                                        std::span<std::string_view> attribute,
                                        Variable const* replaceVariable,
                                        size_t /*index*/) {
  for (auto& it : _elements) {
    if (it.var == searchVariable) {
      // if the attribute path is  $var.a.b and we replace $var.a by $other,
      // the attribute path should become just `b`, i.e. $other.b.

      auto it2 = attribute.begin();
      auto it1 = it.attributePath.begin();

      bool isPrefix = true;
      while (it2 != attribute.end()) {
        if (it1 == it.attributePath.end() || *it1 != *it2) {
          // this path does not match the prefix
          isPrefix = false;
          break;
        }
        ++it1;
        ++it2;
      }

      if (!isPrefix) {
        continue;
      }

      // it1 now points to the remainder. Remove the prefix.
      it.attributePath.erase(it.attributePath.cbegin(), it1);

      it.var = replaceVariable;
    }
  }
}

void GatherNode::getVariablesUsedHere(VarSet& vars) const {
  for (auto const& p : _elements) {
    vars.emplace(p.var);
  }
}

size_t GatherNode::getMemoryUsedBytes() const { return sizeof(*this); }

SingleRemoteOperationNode::SingleRemoteOperationNode(
    ExecutionPlan* plan, ExecutionNodeId id, NodeType mode,
    bool replaceIndexNode, std::string const& key, Collection const* collection,
    ModificationOptions const& options, Variable const* in, Variable const* out,
    Variable const* OLD, Variable const* NEW)
    : ExecutionNode(plan, id),
      CollectionAccessingNode(collection),
      _replaceIndexNode(replaceIndexNode),
      _key(key),
      _mode(mode),
      _inVariable(in),
      _outVariable(out),
      _outVariableOld(OLD),
      _outVariableNew(NEW),
      _options(options) {
  if (_mode == NodeType::INDEX) {  // select
    TRI_ASSERT(!_key.empty());
    TRI_ASSERT(_inVariable == nullptr);
    TRI_ASSERT(_outVariable != nullptr);
    TRI_ASSERT(_outVariableOld == nullptr);
    TRI_ASSERT(_outVariableNew == nullptr);
  } else if (_mode == NodeType::REMOVE) {
    TRI_ASSERT(!_key.empty());
    TRI_ASSERT(_inVariable == nullptr);
    TRI_ASSERT(_outVariableNew == nullptr);
  } else if (_mode == NodeType::INSERT) {
    TRI_ASSERT(_key.empty());
  } else if (_mode != NodeType::UPDATE && _mode != NodeType::REPLACE) {
    TRI_ASSERT(false);
  }
}

/// @brief creates corresponding SingleRemoteOperationNode
std::unique_ptr<ExecutionBlock> SingleRemoteOperationNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();

  TRI_ASSERT(previousNode != nullptr);

  RegisterId in = variableToRegisterOptionalId(_inVariable);
  RegisterId out = variableToRegisterOptionalId(_outVariable);
  RegisterId outputNew = variableToRegisterOptionalId(_outVariableNew);
  RegisterId outputOld = variableToRegisterOptionalId(_outVariableOld);

  OperationOptions options = ModificationExecutorHelpers::convertOptions(
      _options, _outVariableNew, _outVariableOld);

  auto readableInputRegisters = RegIdSet{};
  if (in.isValid()) {
    readableInputRegisters.emplace(in);
  }
  auto writableOutputRegisters = RegIdSet{};
  if (out.isValid()) {
    writableOutputRegisters.emplace(out);
  }
  if (outputNew.isValid()) {
    writableOutputRegisters.emplace(outputNew);
  }
  if (outputOld.isValid()) {
    writableOutputRegisters.emplace(outputOld);
  }

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  auto executorInfos = SingleRemoteModificationInfos(
      &engine, in, outputNew, outputOld, out, _plan->getAst()->query(),
      std::move(options), collection(),
      ConsultAqlWriteFilter(_options.consultAqlWriteFilter),
      IgnoreErrors(_options.ignoreErrors),
      IgnoreDocumentNotFound(_options.ignoreDocumentNotFound), _key,
      this->hasParent(), this->_replaceIndexNode);

  if (_mode == NodeType::INDEX) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<IndexTag>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::INSERT) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Insert>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::REMOVE) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Remove>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::REPLACE) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Replace>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::UPDATE) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Update>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::UPSERT) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Upsert>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else {
    TRI_ASSERT(false);
    return nullptr;
  }
}

/// @brief doToVelocyPack, for SingleRemoteOperationNode
void SingleRemoteOperationNode::doToVelocyPack(VPackBuilder& nodes,
                                               unsigned flags) const {
  CollectionAccessingNode::toVelocyPackHelperPrimaryIndex(nodes);

  // add collection information
  CollectionAccessingNode::toVelocyPack(nodes, flags);

  nodes.add("mode", VPackValue(ExecutionNode::getTypeString(_mode)));
  nodes.add("replaceIndexNode", VPackValue(_replaceIndexNode));

  if (!_key.empty()) {
    nodes.add("key", VPackValue(_key));
  }

  // add out variables
  bool isAnyVarUsedLater = false;
  if (_outVariableOld != nullptr) {
    nodes.add(VPackValue("outVariableOld"));
    _outVariableOld->toVelocyPack(nodes);
    isAnyVarUsedLater |= isVarUsedLater(_outVariableOld);
  }
  if (_outVariableNew != nullptr) {
    nodes.add(VPackValue("outVariableNew"));
    _outVariableNew->toVelocyPack(nodes);
    isAnyVarUsedLater |= isVarUsedLater(_outVariableNew);
  }

  if (_inVariable != nullptr) {
    nodes.add(VPackValue("inVariable"));
    _inVariable->toVelocyPack(nodes);
  }

  if (_outVariable != nullptr) {
    nodes.add(VPackValue("outVariable"));
    _outVariable->toVelocyPack(nodes);
    isAnyVarUsedLater |= isVarUsedLater(_outVariable);
  }
  nodes.add(StaticStrings::ProducesResult, VPackValue(isAnyVarUsedLater));
  nodes.add(VPackValue("modificationFlags"));
  _options.toVelocyPack(nodes);

  nodes.add("projections", VPackValue(VPackValueType::Array));
  // TODO: support projections?
  nodes.close();
}

/// @brief estimateCost
CostEstimate SingleRemoteOperationNode::estimateCost() const {
  CostEstimate estimate = _dependencies[0]->getCost();
  return estimate;
}

AsyncPrefetchEligibility SingleRemoteOperationNode::canUseAsyncPrefetching()
    const noexcept {
  return AsyncPrefetchEligibility::kDisableGlobally;
}

std::vector<Variable const*> SingleRemoteOperationNode::getVariablesSetHere()
    const {
  std::vector<Variable const*> vec;

  if (_outVariable) {
    vec.push_back(_outVariable);
  }
  if (_outVariableNew) {
    vec.push_back(_outVariableNew);
  }
  if (_outVariableOld) {
    vec.push_back(_outVariableOld);
  }

  return vec;
}

void SingleRemoteOperationNode::getVariablesUsedHere(VarSet& vars) const {
  if (_inVariable) {
    vars.emplace(_inVariable);
  }
}

size_t SingleRemoteOperationNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}
