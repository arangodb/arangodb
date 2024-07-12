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

#include "GatherNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/EmptyExecutorInfos.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/Executor/ParallelUnsortedGatherExecutor.h"
#include "Aql/Executor/SortingGatherExecutor.h"
#include "Aql/Executor/UnsortedGatherExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SortRegister.h"
#include "Aql/types.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"

#include <memory>
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

}  // namespace

auto arangodb::aql::toString(GatherNode::Parallelism value) noexcept
    -> std::string_view {
  switch (value) {
    case GatherNode::Parallelism::Parallel:
      return kParallelismParallel;
    case GatherNode::Parallelism::Serial:
      return kParallelismSerial;
    case GatherNode::Parallelism::Undefined:
      return kParallelismUndefined;
    default:
      LOG_TOPIC("c9367", FATAL, Logger::AQL)
          << "Invalid value for parallelism: "
          << static_cast<std::underlying_type_t<GatherNode::Parallelism>>(
                 value);
      FATAL_ERROR_ABORT();
  }
}

auto arangodb::aql::toString(GatherNode::SortMode mode) noexcept
    -> std::string_view {
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
      it.toVelocyPack(nodes);
    }
  }
}

/// @brief clone ExecutionNode recursively
ExecutionNode* GatherNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  auto other = std::make_unique<GatherNode>(plan, _id, _sortmode, _parallelism);
  other->setConstrainedSortLimit(constrainedSortLimit());
  return cloneHelper(std::move(other), withDependencies);
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
    it.replaceVariables(replacements);
  }
}

void GatherNode::replaceAttributeAccess(ExecutionNode const* self,
                                        Variable const* searchVariable,
                                        std::span<std::string_view> attribute,
                                        Variable const* replaceVariable,
                                        size_t /*index*/) {
  for (auto& it : _elements) {
    it.replaceAttributeAccess(searchVariable, attribute, replaceVariable);
  }
}

void GatherNode::getVariablesUsedHere(VarSet& vars) const {
  for (auto const& p : _elements) {
    vars.emplace(p.var);
  }
}

size_t GatherNode::getMemoryUsedBytes() const { return sizeof(*this); }
