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

#include "DistributeExecutor.h"

#include "Aql/Collection.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<DistributeExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, DistributeNode const* node, ExecutorInfos&& infos,
    std::vector<std::string> const& shardIds, Collection const* collection,
    RegisterId regId, RegisterId alternativeRegId, bool allowSpecifiedKeys,
    bool allowKeyConversionToObject, bool createKeys)
    : BlockWithClients(engine, node, shardIds),
      _infos(std::move(infos)),
      _query(*engine->getQuery()),
      _collection(collection),
      _index(0),
      _regId(regId),
      _alternativeRegId(alternativeRegId),
      _allowSpecifiedKeys(allowSpecifiedKeys),
      _allowKeyConversionToObject(allowKeyConversionToObject),
      _createKeys(createKeys) {
  _usesDefaultSharding = collection->usesDefaultSharding();
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<DistributeExecutor>::traceGetSomeEnd(
    ExecutionState state, SharedAqlItemBlockPtr result) {
  ExecutionBlock::traceGetSomeEnd(result.get(), state);
  return {state, std::move(result)};
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<DistributeExecutor>::traceSkipSomeEnd(
    ExecutionState state, size_t skipped) {
  ExecutionBlock::traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

/// @brief initializeCursor
std::pair<ExecutionState, Result> ExecutionBlockImpl<DistributeExecutor>::initializeCursor(
    InputAqlItemRow const& input) {
  // local clean up
  _distBuffer.clear();
  _distBuffer.reserve(_nrClients);

  for (size_t i = 0; i < _nrClients; i++) {
    _distBuffer.emplace_back();
  }

  return BlockWithClients::initializeCursor(input);
}

/// @brief getSomeForShard
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<DistributeExecutor>::getSomeForShard(
    size_t atMost, std::string const& shardId) {
  traceGetSomeBegin(atMost);
  auto result = getSomeForShardWithoutTrace(atMost, shardId);
  return traceGetSomeEnd(result.first, std::move(result.second));
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<DistributeExecutor>::getSomeForShardWithoutTrace(
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
std::pair<ExecutionState, size_t> ExecutionBlockImpl<DistributeExecutor>::skipSomeForShard(
    size_t atMost, std::string const& shardId) {
  traceSkipSomeBegin(atMost);
  auto result = skipSomeForShardWithoutTrace(atMost, shardId);
  return traceSkipSomeEnd(result.first, result.second);
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<DistributeExecutor>::skipSomeForShardWithoutTrace(
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
std::pair<ExecutionState, arangodb::Result> ExecutionBlockImpl<DistributeExecutor>::getOrSkipSomeForShard(
    size_t atMost, bool skipping, SharedAqlItemBlockPtr& result,
    size_t& skipped, std::string const& shardId) {
  TRI_ASSERT(result == nullptr && skipped == 0);
  TRI_ASSERT(atMost > 0);

  size_t clientId = getClientId(shardId);

  if (!hasMoreForClientId(clientId)) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  std::deque<std::pair<size_t, size_t>>& buf = _distBuffer.at(clientId);

  if (buf.empty()) {
    auto res = getBlockForClient(atMost, clientId);
    if (res.first == ExecutionState::WAITING) {
      return {res.first, TRI_ERROR_NO_ERROR};
    }
    if (!res.second) {
      // Upstream is empty!
      TRI_ASSERT(res.first == ExecutionState::DONE);
      return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
    }
  }

  skipped = (std::min)(buf.size(), atMost);

  if (skipping) {
    for (size_t i = 0; i < skipped; i++) {
      buf.pop_front();
    }
    return {getHasMoreStateForClientId(clientId), TRI_ERROR_NO_ERROR};
  }

  BlockCollector collector(&_engine->itemBlockManager());
  std::vector<size_t> chosen;

  size_t i = 0;
  while (i < skipped) {
    size_t const n = buf.front().first;
    while (buf.front().first == n && i < skipped) {
      chosen.emplace_back(buf.front().second);
      buf.pop_front();
      i++;

      // make sure we are not overreaching over the end of the buffer
      if (buf.empty()) {
        break;
      }
    }

    SharedAqlItemBlockPtr more{_buffer[n]->slice(chosen, 0, chosen.size())};
    collector.add(std::move(more));

    chosen.clear();
  }

  // Skipping was handle before
  TRI_ASSERT(!skipping);
  result = collector.steal();

  // _buffer is left intact, deleted and cleared at shutdown

  return {getHasMoreStateForClientId(clientId), TRI_ERROR_NO_ERROR};
}

/// @brief hasMore: any more for any shard?
bool ExecutionBlockImpl<DistributeExecutor>::hasMoreForShard(std::string const& shardId) const {
  return hasMoreForClientId(getClientId(shardId));
}

ExecutionState ExecutionBlockImpl<DistributeExecutor>::getHasMoreStateForClientId(size_t clientId) const {
  if (hasMoreForClientId(clientId)) {
    return ExecutionState::HASMORE;
  }
  return ExecutionState::DONE;
}

bool ExecutionBlockImpl<DistributeExecutor>::hasMoreForClientId(size_t clientId) const {
  // We have more for a client ID if
  // we still have some information in the local buffer
  // or if there is still some information from upstream

  TRI_ASSERT(_distBuffer.size() > clientId);
  if (!_distBuffer[clientId].empty()) {
    return true;
  }
  return _upstreamState == ExecutionState::HASMORE;
}

/// @brief getBlockForClient: try to get atMost pairs into
/// _distBuffer.at(clientId), this means we have to look at every row in the
/// incoming blocks until they run out or we find enough rows for clientId. We
/// also keep track of blocks which should be sent to other clients than the
/// current one.
std::pair<ExecutionState, bool> ExecutionBlockImpl<DistributeExecutor>::getBlockForClient(
    size_t atMost, size_t clientId) {
  if (_buffer.empty()) {
    _index = 0;  // position in _buffer
    _pos = 0;    // position in _buffer.at(_index)
  }

  // it should be the case that buf.at(clientId) is empty
  auto& buf = _distBuffer[clientId];

  while (buf.size() < atMost) {
    if (_index == _buffer.size()) {
      auto res = ExecutionBlock::getBlock(atMost);
      if (res.first == ExecutionState::WAITING) {
        return {res.first, false};
      }
      if (!res.second) {
        TRI_ASSERT(res.first == ExecutionState::DONE);
        if (buf.empty()) {
          TRI_ASSERT(getHasMoreStateForClientId(clientId) == ExecutionState::DONE);
          return {ExecutionState::DONE, false};
        }
        break;
      }
    }

    SharedAqlItemBlockPtr cur = _buffer[_index];

    while (_pos < cur->size()) {
      // this may modify the input item buffer in place
      size_t const id = sendToClient(cur);

      _distBuffer[id].emplace_back(_index, _pos++);
    }

    if (_pos == cur->size()) {
      _pos = 0;
      _index++;
    } else {
      break;
    }
  }

  return {getHasMoreStateForClientId(clientId), true};
}

/// @brief sendToClient: for each row of the incoming AqlItemBlock use the
/// attributes <shardKeys> of the Aql value <val> to determine to which shard
/// the row should be sent and return its clientId
size_t ExecutionBlockImpl<DistributeExecutor>::sendToClient(SharedAqlItemBlockPtr cur) {
  // inspect cur in row _pos and check to which shard it should be sent . .
  AqlValue val = cur->getValueReference(_pos, _regId);

  VPackSlice input = val.slice();  // will throw when wrong type

  bool usedAlternativeRegId = false;

  if (input.isNull() && _alternativeRegId != ExecutionNode::MaxRegisterId) {
    // value is set, but null
    // check if there is a second input register available (UPSERT makes use of
    // two input registers,
    // one for the search document, the other for the insert document)
    val = cur->getValueReference(_pos, _alternativeRegId);

    input = val.slice();  // will throw when wrong type
    usedAlternativeRegId = true;
  }

  VPackSlice value = input;
  bool hasCreatedKeyAttribute = false;

  if (input.isString() && _allowKeyConversionToObject) {
    _keyBuilder.clear();
    _keyBuilder.openObject(true);
    _keyBuilder.add(StaticStrings::KeyString, input);
    _keyBuilder.close();

    // clear the previous value
    cur->destroyValue(_pos, _regId);

    // overwrite with new value
    cur->emplaceValue(_pos, _regId, _keyBuilder.slice());

    value = _keyBuilder.slice();
    hasCreatedKeyAttribute = true;
  } else if (!input.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_ASSERT(value.isObject());

  if (_createKeys) {
    bool buildNewObject = false;
    // we are responsible for creating keys if none present

    if (_usesDefaultSharding) {
      // the collection is sharded by _key...
      if (!hasCreatedKeyAttribute && !value.hasKey(StaticStrings::KeyString)) {
        // there is no _key attribute present, so we are responsible for
        // creating one
        buildNewObject = true;
      }
    } else {
      // the collection is not sharded by _key
      if (hasCreatedKeyAttribute || value.hasKey(StaticStrings::KeyString)) {
        // a _key was given, but user is not allowed to specify _key
        if (usedAlternativeRegId || !_allowSpecifiedKeys) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY);
        }
      } else {
        buildNewObject = true;
      }
    }

    if (buildNewObject) {
      _keyBuilder.clear();
      _keyBuilder.openObject(true);
      _keyBuilder.add(StaticStrings::KeyString, VPackValue(createKey(value)));
      _keyBuilder.close();

      _objectBuilder.clear();
      VPackCollection::merge(_objectBuilder, input, _keyBuilder.slice(), true);

      // clear the previous value and overwrite with new value:
      if (usedAlternativeRegId) {
        cur->destroyValue(_pos, _alternativeRegId);
        cur->emplaceValue(_pos, _alternativeRegId, _objectBuilder.slice());
      } else {
        cur->destroyValue(_pos, _regId);
        cur->emplaceValue(_pos, _regId, _objectBuilder.slice());
      }
      value = _objectBuilder.slice();
    }
  }

  std::string shardId;
  auto collInfo = _collection->getCollection();

  int res = collInfo->getResponsibleShard(value, true, shardId);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(!shardId.empty());

  return getClientId(shardId);
}

/// @brief create a new document key
std::string ExecutionBlockImpl<DistributeExecutor>::createKey(VPackSlice input) const {
  return _collection->getCollection()->createKey(input);
}
