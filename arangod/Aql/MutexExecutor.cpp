////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "MutexExecutor.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IdExecutor.h"
#include "Aql/MutexNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedQueryState.h"
#include "Aql/Stats.h"

#include "Logger/LogMacros.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

MutexExecutorInfos::MutexExecutorInfos(
    std::vector<std::string> clientIds)
    : ClientsExecutorInfos(std::move(clientIds)) {}

//auto MutexExecutorInfos::getResponsibleClient(arangodb::velocypack::Slice value) const
//    -> ResultT<std::string> {
//  std::string shardId;
//  int res = _logCol->getResponsibleShard(value, true, shardId);
//
//  if (res != TRI_ERROR_NO_ERROR) {
//    return Result{res};
//  }
//
//  TRI_ASSERT(!shardId.empty());
//  if (_type == ScatterNode::ScatterType::SERVER) {
//    // Special case for server based distribution.
//    shardId = _collection->getServerForShard(shardId);
//    TRI_ASSERT(!shardId.empty());
//  }
//  return shardId;
//}

MutexExecutor::ClientBlockData::ClientBlockData(ExecutionEngine& engine,
                                                MutexNode const* node,
                                                RegisterInfos const& registerInfos)
    : _blockManager(engine.itemBlockManager()), registerInfos(registerInfos) {
  // We only get shared ptrs to const data. so we need to copy here...
  auto executorInfos = IdExecutorInfos{false, 0, "", false};
  auto idExecutorRegisterInfos =
      RegisterInfos{{},
                    {},
                    registerInfos.numberOfInputRegisters(),
                    registerInfos.numberOfOutputRegisters(),
                    registerInfos.registersToClear(),
                    registerInfos.registersToKeep()};
  // NOTE: Do never change this type! The execute logic below requires this and only this type.
  _executor = std::make_unique<ExecutionBlockImpl<IdExecutor<ConstFetcher>>>(
      &engine, node, std::move(idExecutorRegisterInfos), std::move(executorInfos));
}

auto MutexExecutor::ClientBlockData::clear() -> void {
  _queue.clear();
  _executorHasMore = false;
}

auto MutexExecutor::ClientBlockData::addBlock(SharedAqlItemBlockPtr block,
                                                   std::vector<size_t> usedIndexes) -> void {
  _queue.emplace_back(block, std::move(usedIndexes));
}

auto MutexExecutor::ClientBlockData::addSkipResult(SkipResult const& skipResult) -> void {
  TRI_ASSERT(_skipped.subqueryDepth() == 1 ||
             _skipped.subqueryDepth() == skipResult.subqueryDepth());
  _skipped.merge(skipResult, false);
}

auto MutexExecutor::ClientBlockData::hasDataFor(AqlCall const& call) -> bool {
  return _executorHasMore || !_queue.empty();
}

/**
 * @brief This call will join as many blocks as available from the queue
 *        and return them in a SingleBlock. We then use the IdExecutor
 *        to hand out the data contained in these blocks
 *        We do on purpose not give any kind of guarantees on the sizing of
 *        this block to be flexible with the implementation, and find a good
 *        trade-off between blocksize and block copy operations.
 *
 * @return SharedAqlItemBlockPtr a joind block from the queue.
 */
auto MutexExecutor::ClientBlockData::popJoinedBlock()
    -> std::tuple<SharedAqlItemBlockPtr, SkipResult> {
  // There are some optimizations available in this implementation.
  // Namely we could apply good logic to cut the blocks at shadow rows
  // in order to allow the IDexecutor to hand them out en-block.
  // However we might leverage the restriction to stop at ShadowRows
  // at one point anyways, and this Executor has no business with ShadowRows.
  size_t numRows = 0;
  for (auto const& [block, choosen] : _queue) {
    numRows += choosen.size();
    if (numRows >= ExecutionBlock::DefaultBatchSize) {
      // Avoid to put too many rows into this block.
      break;
    }
  }

  SharedAqlItemBlockPtr newBlock =
      _blockManager.requestBlock(numRows, registerInfos.numberOfOutputRegisters());
  // We create a block, with correct register information
  // but we do not allow outputs to be written.
  RegIdSet const noOutputRegisters{};
  OutputAqlItemRow output{newBlock, noOutputRegisters,
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear()};
  while (!output.isFull()) {
    // If the queue is empty our sizing above would not be correct
    TRI_ASSERT(!_queue.empty());
    auto const& [block, choosen] = _queue.front();
    TRI_ASSERT(output.numRowsLeft() >= choosen.size());
    for (auto const& i : choosen) {
      // We do not really care what we copy. However
      // the API requires to know what it is.
      if (block->isShadowRow(i)) {
        ShadowAqlItemRow toCopy{block, i};
        output.moveRow(toCopy);
      } else {
        InputAqlItemRow toCopy{block, i};
        output.copyRow(toCopy);
      }
      output.advanceRow();
    }
    // All required rows copied.
    // Drop block form queue.
    _queue.pop_front();
  }
  SkipResult skip = _skipped;
  _skipped.reset();
  return {newBlock, skip};
}

auto MutexExecutor::ClientBlockData::execute(AqlCallStack callStack, ExecutionState upstreamState)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  TRI_ASSERT(_executor != nullptr);
  // Make sure we actually have data before you call execute
  TRI_ASSERT(hasDataFor(callStack.peek()));
  if (!_executorHasMore) {
    // This cast is guaranteed, we create this a couple lines above and only
    // this executor is used here.
    // Unfortunately i did not get a version compiled were i could only forward
    // declare the templates in header.
    auto casted =
        static_cast<ExecutionBlockImpl<IdExecutor<ConstFetcher>>*>(_executor.get());
    TRI_ASSERT(casted != nullptr);
    auto [block, skipped] = popJoinedBlock();
    // We will at least get one block, otherwise the hasDataFor would
    // be required to return false!
    TRI_ASSERT(block != nullptr);

    casted->injectConstantBlock(block, skipped);
    _executorHasMore = true;
  }
  auto [state, skipped, result] = _executor->execute(callStack);

  // We have all data locally cannot wait here.
  TRI_ASSERT(state != ExecutionState::WAITING);

  if (state == ExecutionState::DONE) {
    // This executor is finished, including shadowrows
    // We are going to reset it on next call
    _executorHasMore = false;

    // Also we need to adjust the return states
    // as this state only represents one single block
    if (!_queue.empty()) {
      state = ExecutionState::HASMORE;
    } else {
      state = upstreamState;
    }
  }
  return {state, skipped, result};
}

MutexExecutor::MutexExecutor(MutexExecutorInfos const& infos)
  : _infos(infos), _numClient(0) {}

auto MutexExecutor::distributeBlock(SharedAqlItemBlockPtr block, SkipResult skipped,
                                         std::unordered_map<std::string, ClientBlockData>& blockMap)
    -> void {
  std::unordered_map<std::string, std::vector<std::size_t>> choosenMap;
  choosenMap.reserve(blockMap.size());
  for (size_t i = 0; i < block->size(); ++i) {
    if (block->isShadowRow(i)) {
      // ShadowRows need to be added to all Clients
      for (auto const& [key, value] : blockMap) {
        choosenMap[key].emplace_back(i);
      }
    } else {
      auto client = getClient(block, i);
      // We can only have clients we are prepared for
      TRI_ASSERT(blockMap.find(client) != blockMap.end());
      choosenMap[client].emplace_back(i);
    }
  }
  // We cannot have more in choosen than we have blocks
  TRI_ASSERT(choosenMap.size() <= blockMap.size());
  for (auto const& [key, value] : choosenMap) {
    TRI_ASSERT(blockMap.find(key) != blockMap.end());
    auto target = blockMap.find(key);
    if (target == blockMap.end()) {
      // Impossible, just avoid UB.
      LOG_TOPIC("c9163", ERR, Logger::AQL)
          << "Tried to distribute data to shard " << key
          << " which is not part of the query. Ignoring.";
      continue;
    }
    target->second.addBlock(block, std::move(value));
  }

  // Add the skipResult to all clients.
  // It needs to be fetched once for every client.
  for (auto& [key, map] : blockMap) {
    map.addSkipResult(skipped);
  }
}

std::string MutexExecutor::getClient(SharedAqlItemBlockPtr block, size_t rowIndex) {
//  InputAqlItemRow row{block, rowIndex};
//  AqlValue val = row.getValue(_infos.registerId());
  return _infos.clientIds()[(_numClient++) % _infos.nrClients()];
}

ExecutionBlockImpl<MutexExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                      MutexNode const* node,
                                                      RegisterInfos registerInfos,
                                                      MutexExecutorInfos&& executorInfos)
    : BlocksWithClientsImpl(engine, node, std::move(registerInfos),
                            std::move(executorInfos)) {}
