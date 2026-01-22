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

#include "BlocksWithClients.h"

#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/MutexNode.h"
#include "Aql/ExecutionStats.h"
#include "Aql/Executor/DistributeExecutor.h"
#include "Aql/Executor/MutexExecutor.h"
#include "Aql/Executor/ScatterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SkipResult.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

ClientsExecutorInfos::ClientsExecutorInfos(std::vector<std::string> clientIds)
    : _clientIds(std::move(clientIds)) {
  TRI_ASSERT(!_clientIds.empty());
}

auto ClientsExecutorInfos::nrClients() const noexcept -> size_t {
  return _clientIds.size();
}

auto ClientsExecutorInfos::clientIds() const noexcept
    -> std::vector<std::string> const& {
  return _clientIds;
}

template<class Executor>
BlocksWithClientsImpl<Executor>::BlocksWithClientsImpl(
    ExecutionEngine* engine, ExecutionNode const* ep,
    RegisterInfos registerInfos, typename Executor::Infos executorInfos)
    : ExecutionBlock(engine, ep),
      BlocksWithClients(),
      _type(ScatterNode::ScatterType::SHARD),
      _registerInfos(std::move(registerInfos)),
      _executorInfos(std::move(executorInfos)),
      _executor{_executorInfos},
      _clientBlockData{} {
  size_t nrClients = executorInfos.nrClients();
  _shardIdMap.reserve(nrClients);
  auto const& shardIds = _executorInfos.clientIds();
  for (size_t i = 0; i < nrClients; i++) {
    _shardIdMap.try_emplace(shardIds[i], i);
  }

  _clientBlockData.reserve(shardIds.size());

  if constexpr (std::is_same<MutexExecutor, Executor>::value) {
    auto* mutex = ExecutionNode::castTo<MutexNode const*>(ep);
    TRI_ASSERT(mutex != nullptr);

    for (auto const& id : shardIds) {
      _clientBlockData.try_emplace(id, typename Executor::ClientBlockData{
                                           *engine, mutex, _registerInfos});
    }
  } else {
    auto* scatter = ExecutionNode::castTo<ScatterNode const*>(ep);
    TRI_ASSERT(scatter != nullptr);
    _type = scatter->getScatterType();

    for (auto const& id : shardIds) {
      _clientBlockData.try_emplace(id, typename Executor::ClientBlockData{
                                           *engine, scatter, _registerInfos});
    }
  }
}

/// @brief initializeCursor
template<class Executor>
auto BlocksWithClientsImpl<Executor>::initializeCursor(
    InputAqlItemRow const& input) -> std::pair<ExecutionState, Result> {
  for (auto& [key, list] : _clientBlockData) {
    list.clear();
  }
  return ExecutionBlock::initializeCursor(input);
}

/// @brief getClientId: get the number <clientId> (used internally)
/// corresponding to <shardId>
template<class Executor>
size_t BlocksWithClientsImpl<Executor>::getClientId(
    std::string const& shardId) const {
  if (shardId.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "got empty distribution id");
  }
  auto it = _shardIdMap.find(shardId);
  if (it == _shardIdMap.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        absl::StrCat("AQL: unknown distribution id ", shardId));
  }
  return it->second;
}

template<class Executor>
std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>
BlocksWithClientsImpl<Executor>::execute(AqlCallStack const& /*stack*/) {
  // This will not be implemented here!
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

template<class Executor>
auto BlocksWithClientsImpl<Executor>::executeForClient(
    AqlCallStack stack, std::string const& clientId)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  if constexpr (std::is_same<MutexExecutor, Executor>::value) {
    _executor.acquireLock();
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool old = false;
  TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, true));
  TRI_ASSERT(_isBlockInUse);
#endif

  auto guard = scopeGuard([&]() noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    bool old = true;
    TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, false));
    TRI_ASSERT(!_isBlockInUse);
#endif

    if constexpr (std::is_same<MutexExecutor, Executor>::value) {
      _executor.releaseLock();
    }
  });

  traceExecuteBegin(stack, clientId);
  auto res = executeWithoutTraceForClient(std::move(stack), clientId);
  traceExecuteEnd(res, clientId);
  return res;
}

template<class Executor>
auto BlocksWithClientsImpl<Executor>::executeWithoutTraceForClient(
    AqlCallStack stack, std::string const& clientId)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  TRI_ASSERT(!clientId.empty());
  if (ADB_UNLIKELY(clientId.empty())) {
    // Security bailout to avoid UB
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "got empty distribution id");
  }

  auto it = _clientBlockData.find(clientId);
  TRI_ASSERT(it != _clientBlockData.end());
  if (ADB_UNLIKELY(it == _clientBlockData.end())) {
    // Security bailout to avoid UB
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        absl::StrCat("AQL: unknown distribution id ", clientId));
  }

  // This call is only used internally.
  auto callList = stack.popCall();
  auto call = callList.peekNextCall();

  auto& dataContainer = it->second;
  /*
   * In this block we need to handle the following behavior:
   * 1) Call without a hardLimit:
   * - Pull what is already on the dataContainer,
   * - if dataContainer does not have more data, fetch more from upstream.
   *
   * 2) Call with a hardLimit = 0, without fullCount:
   * - This indicates this lane is complete and does not need more data.
   * - Clear what is still active in the dataContainer. (Do not drop ShadowRows)
   * - If all other lanes already reported a hardLimit, and upstream still
   * HASMORE, then send hardLimit to upstream.
   *
   * 3) Call with hardLimit = 0 and fullCount:
   * - This indicates this lane is ready to skip and count.
   * - I do not think that this is possible in practice. Therefore this will be
   * asserted, but in prod handled like case 1.
   * - As case 1 is slower then necessary, but always correct.
   *
   * Also NOTE: We are only doing this in the main query, not in subqueries. In
   * the main query we can simply return DONE with an empty block. For
   * subqueries we would need to return a ShadowRow. However, the row is not yet
   * available, if upstream is not fully consumed. We can only fix this by
   * returning WAITING here, and waking up the caller, as soon as the ShadowRow
   * is available.
   */
  if (stack.empty() && call.hasHardLimit() && call.getLimit() == 0 &&
      !call.fullCount) {
    dataContainer.setSeenHardLimit();
    // Need to handle this case separately, as we do not want to fetch more
    // data from upstream.
    // We will not be able to fetch any more rows.
    // Clear what is still active in the dataContainer. (Do not drop ShadowRows)
    if (dataContainer.hasDataFor(call)) {
      stack.pushCall(callList);
      while (dataContainer.hasDataFor(call)) {
        // Just clear out what is still on the queue.
        std::ignore = dataContainer.execute(stack, _upstreamState);
      }
      stack.popCall();
    }
    if (allLanesComplete() && _upstreamState != ExecutionState::DONE) {
      // We send a hardLimit to the dependency to skip over remaining rows.
      auto state = hardLimitDependency(stack);
      if (state == ExecutionState::WAITING) {
        return {state, SkipResult{}, nullptr};
      }
      _upstreamState = state;
    }
    // Report everything is consumed.
    return {ExecutionState::DONE, SkipResult{}, nullptr};
  }

  // We are done, with everything, we will not be able to fetch any more
  // We do not have anymore data locally.
  // Need to fetch more from upstream
  while (true) {
    while (!dataContainer.hasDataFor(call)) {
      if (_upstreamState == ExecutionState::DONE) {
        // We are done, with everything, we will not be able to fetch any more
        // rows
        return {_upstreamState, SkipResult{}, nullptr};
      }

      auto state = fetchMore(stack);
      if (state == ExecutionState::WAITING) {
        return {state, SkipResult{}, nullptr};
      }
      _upstreamState = state;
    }
    {
      // If we get here we have data and can return it.
      // However the call might force us to drop everything (e.g. hardLimit ==
      // 0) So we need to refetch data eventually.
      stack.pushCall(callList);
      auto [state, skipped, result] =
          dataContainer.execute(stack, _upstreamState);
      if (state == ExecutionState::DONE || !skipped.nothingSkipped() ||
          result != nullptr) {
        // We have a valid result.
        return {state, skipped, std::move(result)};
      }
      stack.popCall();
    }
  }
}

template<class Executor>
auto BlocksWithClientsImpl<Executor>::hardLimitDependency(AqlCallStack stack)
    -> ExecutionState {
  if (_engine->getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  // NOTE: We do not handle limits / skip here
  // They can differ between different calls to this executor.
  // We may need to revisit this for performance reasons.
  stack.pushCall(AqlCallList{AqlCall{0, false, 0, AqlCall::LimitType::HARD}});

  TRI_ASSERT(_dependencies.size() == 1);
  auto [state, skipped, block] = _dependencies[0]->execute(stack);

  // We could need the row in a different block, and once skipped
  // we cannot get it back.
  TRI_ASSERT(skipped.getSkipCount() == 0);

  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // Waiting -> no block
  TRI_ASSERT(state != ExecutionState::WAITING || block == nullptr);

  if (state != ExecutionState::WAITING && block != nullptr) {
    // We need to report everything that is not waiting
    // Here this could be a ShadowRow!
    TRI_ASSERT(false) << "Right now we cannot get here, as the optimization is "
                         "not yet active on subqueries.";
    _executor.distributeBlock(block, skipped, _clientBlockData);
  }

  return state;
}

template<class Executor>
auto BlocksWithClientsImpl<Executor>::fetchMore(AqlCallStack stack)
    -> ExecutionState {
  if (_engine->getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  // NOTE: We do not handle limits / skip here
  // They can differ between different calls to this executor.
  // We may need to revisit this for performance reasons.
  stack.pushCall(AqlCallList{AqlCall{}});

  TRI_ASSERT(_dependencies.size() == 1);
  auto [state, skipped, block] = _dependencies[0]->execute(stack);

  // We could need the row in a different block, and once skipped
  // we cannot get it back.
  TRI_ASSERT(skipped.getSkipCount() == 0);

  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // Waiting -> no block
  TRI_ASSERT(state != ExecutionState::WAITING || block == nullptr);

  if (state != ExecutionState::WAITING) {
    // We need to report everything that is not waiting
    _executor.distributeBlock(block, skipped, _clientBlockData);
  }

  return state;
}

template<class Executor>
auto BlocksWithClientsImpl<Executor>::allLanesComplete() const noexcept
    -> bool {
  return std::ranges::none_of(_clientBlockData, [](const auto& entry) {
    return !entry.second.gotHardLimit();
  });
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
template<class Executor>
auto BlocksWithClientsImpl<Executor>::remainingRowsForClient(
    std::string const& clientId) const -> uint64_t {
  auto it = _clientBlockData.find(clientId);
  TRI_ASSERT(it != _clientBlockData.end())
      << "Test setup issue, clientId " << clientId << " is not registered";
  return it->second.remainingRows();
}
#endif

template class ::arangodb::aql::BlocksWithClientsImpl<ScatterExecutor>;
template class ::arangodb::aql::BlocksWithClientsImpl<DistributeExecutor>;
template class ::arangodb::aql::BlocksWithClientsImpl<MutexExecutor>;
