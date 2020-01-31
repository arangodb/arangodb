////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ScatterExecutor.h"

#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;

auto ScatterExecutor::ClientBlockData::clear() -> void { _queue.clear(); }

auto ScatterExecutor::ClientBlockData::addBlock(SharedAqlItemBlockPtr block) -> void {
  _queue.emplace_back(block);
}

auto ScatterExecutor::ClientBlockData::hasDataFor(AqlCall const& call) -> bool {
  return !_queue.empty();
}

auto ScatterExecutor::ClientBlockData::dropBlock() -> void {
  _queue.pop_front();
  _firstBlockPos = 0;
}

auto ScatterExecutor::ClientBlockData::firstBlockRows() -> size_t {
  if (_queue.empty()) {
    return 0;
  }
  auto& block = _queue.front();
  TRI_ASSERT(_firstBlockPos <= block->size());
  return block->size() - _firstBlockPos;
}

auto ScatterExecutor::ClientBlockData::execute(AqlCall call, ExecutionState upstreamState)
    -> std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> {
  // Make sure we actually have data before you call execute
  TRI_ASSERT(hasDataFor(call));
  size_t skipped = 0;
  SharedAqlItemBlockPtr result = nullptr;
  while (!_queue.empty()) {
    auto const block = _queue.front();
    if (!block->hasShadowRows()) {
      size_t toSkip = call.getOffset();
      size_t toProduce = call.getLimit();
      if (toSkip > 0) {
        if (toSkip >= firstBlockRows()) {
          skipped += firstBlockRows();
          call.didSkip(firstBlockRows());
          dropBlock();
        } else {
          _firstBlockPos += toSkip;
          skipped += toSkip;
          call.didSkip(toSkip);
        }
      } else if (toProduce > 0) {
        if (result != nullptr) {
          // we have produced a block.
          // NOTE: we need to late break
          // here in order to improve fullCount / hardLimit
          // It can simply count all blocks we already have,
          // nevertheless it is not given that we are done after
          break;
        }
        if (toProduce >= firstBlockRows()) {
          call.didProduce(firstBlockRows());
          if (_firstBlockPos != 0) {
            // We need to slice
            result = block->slice(_firstBlockPos, block->size());
          } else {
            result = block;
          }
          // block fully used
          dropBlock();
        } else {
          // We do not use the full block, but need to slice
          result = block->slice(_firstBlockPos, _firstBlockPos + toProduce);
          _firstBlockPos += toProduce;
          call.didProduce(toProduce);
        }
      } else if (call.hasHardLimit()) {
        if (call.needsFullCount()) {
          // We need to count what we dropped.
          skipped += firstBlockRows();
          call.didSkip(firstBlockRows());
        }
        dropBlock();
      } else {
        // We have fulfilled this call
        break;
      }
    }
  }
  if (_queue.empty()) {
    return {upstreamState, skipped, result};
  } else {
    return {ExecutionState::HASMORE, skipped, result};
  }

  // TODO IMPLEMENT ME
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

ScatterExecutor::ScatterExecutor(){};

auto ScatterExecutor::distributeBlock(SharedAqlItemBlockPtr block,
                                      std::unordered_map<std::string, ClientBlockData>& blockMap) const
    -> void {
  // Scatter returns every block on every client as is.
  for (auto& [id, list] : blockMap) {
    list.addBlock(block);
  }
}

ExecutionBlockImpl<ScatterExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                        ScatterNode const* node,
                                                        ExecutorInfos&& infos,
                                                        std::vector<std::string> const& shardIds)
    : BlocksWithClientsImpl(engine, node, shardIds),
      _infos(std::move(infos)),
      _query(*engine->getQuery()) {
  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
}

/// @brief getSomeForShard
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<ScatterExecutor>::getSomeForShard(
    size_t atMost, std::string const& shardId) {
  traceGetSomeBegin(atMost);
  auto result = getSomeForShardWithoutTrace(atMost, shardId);
  return traceGetSomeEnd(result.first, std::move(result.second));
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<ScatterExecutor>::getSomeForShardWithoutTrace(
    size_t atMost, std::string const& shardId) {
  // NOTE: We do not need to retain these, the getOrSkipSome is required to!
  size_t skipped = 0;
  SharedAqlItemBlockPtr result = nullptr;
  auto out = getOrSkipSomeForShard(atMost, false, result, skipped, shardId);
  if (out.first == ExecutionState::WAITING) {
    return {out.first, nullptr};
  }
  if (!out.second.ok()) {
    THROW_ARANGO_EXCEPTION(out.second);
  }
  return {out.first, std::move(result)};
}

/// @brief skipSomeForShard
std::pair<ExecutionState, size_t> ExecutionBlockImpl<ScatterExecutor>::skipSomeForShard(
    size_t atMost, std::string const& shardId) {
  traceSkipSomeBegin(atMost);
  auto result = skipSomeForShardWithoutTrace(atMost, shardId);
  return traceSkipSomeEnd(result.first, result.second);
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<ScatterExecutor>::skipSomeForShardWithoutTrace(
    size_t atMost, std::string const& shardId) {
  // NOTE: We do not need to retain these, the getOrSkipSome is required to!
  size_t skipped = 0;
  SharedAqlItemBlockPtr result = nullptr;
  auto out = getOrSkipSomeForShard(atMost, true, result, skipped, shardId);
  if (out.first == ExecutionState::WAITING) {
    return {out.first, 0};
  }
  TRI_ASSERT(result == nullptr);
  if (!out.second.ok()) {
    THROW_ARANGO_EXCEPTION(out.second);
  }
  return {out.first, skipped};
}

/// @brief getOrSkipSomeForShard
std::pair<ExecutionState, arangodb::Result> ExecutionBlockImpl<ScatterExecutor>::getOrSkipSomeForShard(
    size_t atMost, bool skipping, SharedAqlItemBlockPtr& result,
    size_t& skipped, std::string const& shardId) {
  TRI_ASSERT(result == nullptr && skipped == 0);
  TRI_ASSERT(atMost > 0);

  size_t const clientId = getClientId(shardId);

  if (!hasMoreForClientId(clientId)) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  TRI_ASSERT(_posForClient.size() > clientId);
  std::pair<size_t, size_t>& pos = _posForClient[clientId];

  // pull more blocks from dependency if necessary . . .
  if (pos.first >= _buffer.size()) {
    auto res = getBlock(atMost);
    if (res.first == ExecutionState::WAITING) {
      return {res.first, TRI_ERROR_NO_ERROR};
    }
    if (!res.second) {
      TRI_ASSERT(res.first == ExecutionState::DONE);
      return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
    }
  }

  auto& blockForClient = _buffer[pos.first];

  size_t available = blockForClient->size() - pos.second;
  // available should be non-zero

  skipped = (std::min)(available, atMost);  // nr rows in outgoing block

  if (!skipping) {
    result = blockForClient->slice(pos.second, pos.second + skipped);
  }

  // increment the position . . .
  pos.second += skipped;

  // check if we're done at current block in buffer . . .
  if (pos.second == blockForClient->size()) {
    pos.first++;     // next block
    pos.second = 0;  // reset the position within a block

    // check if we can pop the front of the buffer . . .
    bool popit = true;
    for (size_t i = 0; i < _nrClients; i++) {
      if (_posForClient[i].first == 0) {
        popit = false;
        break;
      }
    }
    if (popit) {
      _buffer.pop_front();
      // update the values in first coord of _posForClient
      for (size_t i = 0; i < _nrClients; i++) {
        _posForClient[i].first--;
      }
    }
  }

  return {getHasMoreStateForClientId(clientId), TRI_ERROR_NO_ERROR};
}

bool ExecutionBlockImpl<ScatterExecutor>::hasMoreForClientId(size_t clientId) const {
  TRI_ASSERT(_nrClients != 0);

  TRI_ASSERT(clientId < _posForClient.size());
  std::pair<size_t, size_t> pos = _posForClient.at(clientId);
  // (i, j) where i is the position in _buffer, and j is the position in
  // _buffer[i] we are sending to <clientId>

  if (pos.first <= _buffer.size()) {
    return true;
  }
  return _upstreamState == ExecutionState::HASMORE;
}

ExecutionState ExecutionBlockImpl<ScatterExecutor>::getHasMoreStateForClientId(size_t clientId) const {
  if (hasMoreForClientId(clientId)) {
    return ExecutionState::HASMORE;
  }
  return ExecutionState::DONE;
}
