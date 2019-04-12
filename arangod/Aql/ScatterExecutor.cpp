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

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<ScatterExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                        ScatterNode const* node,
                                                        ExecutorInfos&& infos,
                                                        std::vector<std::string> const& shardIds)
    : BlockWithClients(engine, node, shardIds),
      _infos(std::move(infos)),
      _query(*engine->getQuery()) {
  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<ScatterExecutor>::traceGetSomeEnd(
    ExecutionState state, SharedAqlItemBlockPtr result) {
  ExecutionBlock::traceGetSomeEnd(result.get(), state);
  return {state, std::move(result)};
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<ScatterExecutor>::traceSkipSomeEnd(
    ExecutionState state, size_t skipped) {
  ExecutionBlock::traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

/// @brief initializeCursor
std::pair<ExecutionState, Result> ExecutionBlockImpl<ScatterExecutor>::initializeCursor(
    InputAqlItemRow const& input) {
  // local clean up
  _posForClient.clear();

  for (size_t i = 0; i < _nrClients; i++) {
    _posForClient.emplace_back(0, 0);
  }

  return BlockWithClients::initializeCursor(input);
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
