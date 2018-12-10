////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterBlocks.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlTransaction.h"
#include "Aql/BlockCollector.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionStats.h"
#include "Aql/Query.h"
#include "Aql/WakeupQueryCallback.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;
using StringBuffer = arangodb::basics::StringBuffer;


namespace {

/// @brief OurLessThan: comparison method for elements of SortingGatherBlock
class OurLessThan {
 public:
  OurLessThan(
      arangodb::transaction::Methods* trx,
      std::vector<std::deque<AqlItemBlock*>> const& gatherBlockBuffer,
      std::vector<SortRegister>& sortRegisters) noexcept
    : _trx(trx),
      _gatherBlockBuffer(gatherBlockBuffer),
      _sortRegisters(sortRegisters) {
  }

  bool operator()(
    std::pair<size_t, size_t> const& a,
    std::pair<size_t, size_t> const& b
  ) const;

 private:
  arangodb::transaction::Methods* _trx;
  std::vector<std::deque<AqlItemBlock*>> const& _gatherBlockBuffer;
  std::vector<SortRegister>& _sortRegisters;
}; // OurLessThan

bool OurLessThan::operator()(
    std::pair<size_t, size_t> const& a,
    std::pair<size_t, size_t> const& b
) const {
  // nothing in the buffer is maximum!
  if (_gatherBlockBuffer[a.first].empty()) {
    return false;
  }

  if (_gatherBlockBuffer[b.first].empty()) {
    return true;
  }

  TRI_ASSERT(!_gatherBlockBuffer[a.first].empty());
  TRI_ASSERT(!_gatherBlockBuffer[b.first].empty());

  for (auto const& reg : _sortRegisters) {
    auto const& lhs = _gatherBlockBuffer[a.first].front()->getValueReference(a.second, reg.reg);
    auto const& rhs = _gatherBlockBuffer[b.first].front()->getValueReference(b.second, reg.reg);
    auto const& attributePath = reg.attributePath;

    // Fast path if there is no attributePath:
    int cmp;

    if (attributePath.empty()) {
#if 0 // #ifdef USE_IRESEARCH
      TRI_ASSERT(reg.comparator);
      cmp = (*reg.comparator)(reg.scorer.get(), _trx, lhs, rhs);
#else
    cmp = AqlValue::Compare(_trx, lhs, rhs, true);
#endif
    } else {
      // Take attributePath into consideration:
      bool mustDestroyA;
      AqlValue aa = lhs.get(_trx, attributePath, mustDestroyA, false);
      AqlValueGuard guardA(aa, mustDestroyA);
      bool mustDestroyB;
      AqlValue bb = rhs.get(_trx, attributePath, mustDestroyB, false);
      AqlValueGuard guardB(bb, mustDestroyB);
      cmp = AqlValue::Compare(_trx, aa, bb, true);
    }

    if (cmp < 0) {
      return reg.asc;
    } else if (cmp > 0) {
      return !reg.asc;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @class HeapSorting
/// @brief "Heap" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class HeapSorting final : public SortingStrategy, private OurLessThan  {
 public:
  HeapSorting(
      arangodb::transaction::Methods* trx,
      std::vector<std::deque<AqlItemBlock*>> const& gatherBlockBuffer,
      std::vector<SortRegister>& sortRegisters) noexcept
    : OurLessThan(trx, gatherBlockBuffer, sortRegisters) {
  }

  virtual ValueType nextValue() override {
    TRI_ASSERT(!_heap.empty());
    std::push_heap(_heap.begin(), _heap.end(), *this); // re-insert element
    std::pop_heap(_heap.begin(), _heap.end(), *this); // remove element from _heap but not from vector
    return _heap.back();
  }

  virtual void prepare(std::vector<ValueType>& blockPos) override {
    TRI_ASSERT(!blockPos.empty());

    if (_heap.size() == blockPos.size()) {
      return;
    }

    _heap.clear();
    std::copy(blockPos.begin(), blockPos.end(), std::back_inserter(_heap));
    std::make_heap(_heap.begin(), _heap.end()-1, *this); // remain last element out of heap to maintain invariant
    TRI_ASSERT(!_heap.empty());
  }

  virtual void reset() noexcept override {
    _heap.clear();
  }

  bool operator()(
      std::pair<size_t, size_t> const& lhs,
      std::pair<size_t, size_t> const& rhs
  ) const {
    return OurLessThan::operator()(rhs, lhs);
  }

 private:
  std::vector<std::reference_wrapper<ValueType>> _heap;
}; // HeapSorting

////////////////////////////////////////////////////////////////////////////////
/// @class MinElementSorting
/// @brief "MinElement" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class MinElementSorting final : public SortingStrategy, public OurLessThan {
 public:
  MinElementSorting(
      arangodb::transaction::Methods* trx,
      std::vector<std::deque<AqlItemBlock*>> const& gatherBlockBuffer,
      std::vector<SortRegister>& sortRegisters) noexcept
    : OurLessThan(trx, gatherBlockBuffer, sortRegisters),
      _blockPos(nullptr) {
  }

  virtual ValueType nextValue() override {
    TRI_ASSERT(_blockPos);
    return *(std::min_element(_blockPos->begin(), _blockPos->end(), *this));
  }

  virtual void prepare(std::vector<ValueType>& blockPos) override {
    _blockPos = &blockPos;
  }

  virtual void reset() noexcept override {
    _blockPos = nullptr;
  }

 private:
  std::vector<ValueType> const* _blockPos;
};

}

BlockWithClients::BlockWithClients(ExecutionEngine* engine,
                                   ExecutionNode const* ep,
                                   std::vector<std::string> const& shardIds)
    : ExecutionBlock(engine, ep), _nrClients(shardIds.size()), _wasShutdown(false) {
  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
}

std::pair<ExecutionState, Result> BlockWithClients::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  return res;
}

/// @brief shutdown
std::pair<ExecutionState, Result> BlockWithClients::shutdown(int errorCode) {
  if (_wasShutdown) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }
  auto res = ExecutionBlock::shutdown(errorCode);
  if (res.first == ExecutionState::WAITING) {
    return res;
  }
  _wasShutdown = true;
  return res;
}

/// @brief getSomeForShard
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
BlockWithClients::getSomeForShard(size_t atMost, std::string const& shardId) {
  traceGetSomeBegin(atMost);

  // NOTE: We do not need to retain these, the getOrSkipSome is required to!
  size_t skipped = 0;
  std::unique_ptr<AqlItemBlock> result = nullptr;
  auto out = getOrSkipSomeForShard(atMost, false, result, skipped, shardId);
  if (out.first == ExecutionState::WAITING) {
    traceGetSomeEnd(result.get(), out.first);
    return {out.first, nullptr};
  }
  if (!out.second.ok()) {
    THROW_ARANGO_EXCEPTION(out.second);
  }
  traceGetSomeEnd(result.get(), out.first);
  return {out.first, std::move(result)};
}

/// @brief skipSomeForShard
std::pair<ExecutionState, size_t> BlockWithClients::skipSomeForShard(
    size_t atMost, std::string const& shardId) {
  // NOTE: We do not need to retain these, the getOrSkipSome is required to!
  size_t skipped = 0;
  std::unique_ptr<AqlItemBlock> result = nullptr;
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

/// @brief getClientId: get the number <clientId> (used internally)
/// corresponding to <shardId>
size_t BlockWithClients::getClientId(std::string const& shardId) {
  if (shardId.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "got empty shard id");
  }

  auto it = _shardIdMap.find(shardId);
  if (it == _shardIdMap.end()) {
    std::string message("AQL: unknown shard id ");
    message.append(shardId);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }
  return ((*it).second);
}

/// @brief initializeCursor
std::pair<ExecutionState, Result> ScatterBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  // local clean up
  _posForClient.clear();

  for (size_t i = 0; i < _nrClients; i++) {
    _posForClient.emplace_back(0, 0);
  }

  return BlockWithClients::initializeCursor(items, pos);
}

ExecutionState ScatterBlock::getHasMoreStateForClientId(size_t clientId) {
  if (hasMoreForClientId(clientId)) {
    return ExecutionState::HASMORE;
  }
  return ExecutionState::DONE;
}

bool ScatterBlock::hasMoreForClientId(size_t clientId) {
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

/// @brief hasMoreForShard: any more for shard <shardId>?
bool ScatterBlock::hasMoreForShard(std::string const& shardId) {
  return hasMoreForClientId(getClientId(shardId));
}

ExecutionState ScatterBlock::getHasMoreStateForShard(
    std::string const& shardId) {
  return getHasMoreStateForClientId(getClientId(shardId));
}

/// @brief getOrSkipSomeForShard
std::pair<ExecutionState, arangodb::Result> ScatterBlock::getOrSkipSomeForShard(
    size_t atMost, bool skipping, std::unique_ptr<AqlItemBlock>& result, size_t& skipped,
    std::string const& shardId) {
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
    result.reset(blockForClient->slice(pos.second, pos.second + skipped));
  }

  // increment the position . . .
  pos.second += skipped;

  // check if we're done at current block in buffer . . .
  if (pos.second == blockForClient->size()) {
    pos.first++; // next block
    pos.second = 0; // reset the position within a block

    // check if we can pop the front of the buffer . . .
    bool popit = true;
    for (size_t i = 0; i < _nrClients; i++) {
      if (_posForClient[i].first == 0) {
        popit = false;
        break;
      }
    }
    if (popit) {
      delete _buffer.front();
      _buffer.pop_front();
      // update the values in first coord of _posForClient
      for (size_t i = 0; i < _nrClients; i++) {
        _posForClient[i].first--;
      }
    }
  }

  return {getHasMoreStateForClientId(clientId), TRI_ERROR_NO_ERROR};
}

DistributeBlock::DistributeBlock(ExecutionEngine* engine,
                                 DistributeNode const* ep,
                                 std::vector<std::string> const& shardIds,
                                 Collection const* collection)
    : BlockWithClients(engine, ep, shardIds),
      _collection(collection),
      _index(0),
      _regId(ExecutionNode::MaxRegisterId),
      _alternativeRegId(ExecutionNode::MaxRegisterId),
      _allowSpecifiedKeys(false) {
  // get the variable to inspect . . .
  VariableId varId = ep->_variable->id;

  // get the register id of the variable to inspect . . .
  auto it = ep->getRegisterPlan()->varInfo.find(varId);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  _regId = (*it).second.registerId;

  TRI_ASSERT(_regId < ExecutionNode::MaxRegisterId);

  if (ep->_alternativeVariable != ep->_variable) {
    // use second variable
    auto it = ep->getRegisterPlan()->varInfo.find(ep->_alternativeVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    _alternativeRegId = (*it).second.registerId;

    TRI_ASSERT(_alternativeRegId < ExecutionNode::MaxRegisterId);
  }

  _usesDefaultSharding = collection->usesDefaultSharding();
  _allowSpecifiedKeys = ep->_allowSpecifiedKeys;
}

/// @brief initializeCursor
std::pair<ExecutionState, Result> DistributeBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  // local clean up
  _distBuffer.clear();
  _distBuffer.reserve(_nrClients);

  for (size_t i = 0; i < _nrClients; i++) {
    _distBuffer.emplace_back();
  }

  return BlockWithClients::initializeCursor(items, pos);
}

ExecutionState DistributeBlock::getHasMoreStateForClientId(size_t clientId) {
  if (hasMoreForClientId(clientId)) {
    return ExecutionState::HASMORE;
  }
  return ExecutionState::DONE;
}

bool DistributeBlock::hasMoreForClientId(size_t clientId) {
  // We have more for a client ID if
  // we still have some information in the local buffer
  // or if there is still some information from upstream

  TRI_ASSERT(_distBuffer.size() > clientId);
  if (!_distBuffer[clientId].empty()) {
    return true;
  }
  return _upstreamState == ExecutionState::HASMORE;
}

/// @brief hasMore: any more for any shard?
bool DistributeBlock::hasMoreForShard(std::string const& shardId) {
  return hasMoreForClientId(getClientId(shardId));
}

ExecutionState DistributeBlock::getHasMoreStateForShard(
    std::string const& shardId) {
  return getHasMoreStateForClientId(getClientId(shardId));
}

/// @brief getOrSkipSomeForShard
std::pair<ExecutionState, arangodb::Result>
DistributeBlock::getOrSkipSomeForShard(size_t atMost, bool skipping,
                                       std::unique_ptr<AqlItemBlock>& result,
                                       size_t& skipped,
                                       std::string const& shardId) {
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

  BlockCollector collector(&_engine->_itemBlockManager);
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

    std::unique_ptr<AqlItemBlock> more(_buffer[n]->slice(chosen, 0, chosen.size()));
    collector.add(std::move(more));

    chosen.clear();
  }

  if (!skipping) {
    result.reset(collector.steal());
  }

  // _buffer is left intact, deleted and cleared at shutdown

  return {getHasMoreStateForClientId(clientId), TRI_ERROR_NO_ERROR};
}

/// @brief getBlockForClient: try to get atMost pairs into
/// _distBuffer.at(clientId), this means we have to look at every row in the
/// incoming blocks until they run out or we find enough rows for clientId. We
/// also keep track of blocks which should be sent to other clients than the
/// current one.
std::pair<ExecutionState, bool> DistributeBlock::getBlockForClient(
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

    AqlItemBlock* cur = _buffer[_index];

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
size_t DistributeBlock::sendToClient(AqlItemBlock* cur) {
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

  if (input.isString() &&
      ExecutionNode::castTo<DistributeNode const*>(_exeNode)
          ->_allowKeyConversionToObject) {
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

  if (ExecutionNode::castTo<DistributeNode const*>(_exeNode)->_createKeys) {
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
std::string DistributeBlock::createKey(VPackSlice input) const {
  return _collection->getCollection()->createKey(input);
}

arangodb::Result RemoteBlock::handleCommErrors(ClusterCommResult* res) const {
  if (res->status == CL_COMM_TIMEOUT ||
      res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    return { res->getErrorCode(), res->stringifyErrorMessage() };
  }
  if (res->status == CL_COMM_ERROR) {
    std::string errorMessage = std::string("Error message received from shard '") +
                     std::string(res->shardID) +
                     std::string("' on cluster node '") +
                     std::string(res->serverID) + std::string("': ");


    int errorNum = TRI_ERROR_INTERNAL;
    if (res->result != nullptr) {
      errorNum = TRI_ERROR_NO_ERROR;
      arangodb::basics::StringBuffer const& responseBodyBuf(res->result->getBody());
      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(
          responseBodyBuf.c_str(), responseBodyBuf.length());
      VPackSlice slice = builder->slice();

      if (!slice.hasKey(StaticStrings::Error) || slice.get(StaticStrings::Error).getBoolean()) {
        errorNum = TRI_ERROR_INTERNAL;
      }

      if (slice.isObject()) {
        VPackSlice v = slice.get(StaticStrings::ErrorNum);
        if (v.isNumber()) {
          if (v.getNumericValue<int>() != TRI_ERROR_NO_ERROR) {
            /* if we've got an error num, error has to be true. */
            TRI_ASSERT(errorNum == TRI_ERROR_INTERNAL);
            errorNum = v.getNumericValue<int>();
          }
        }

        v = slice.get(StaticStrings::ErrorMessage);
        if (v.isString()) {
          errorMessage += v.copyString();
        } else {
          errorMessage += std::string("(no valid error in response)");
        }
      }
    }
    // In this case a proper HTTP error was reported by the DBserver,
    if (errorNum > 0 && !errorMessage.empty()) {
      return {errorNum, errorMessage};
    }

    // default error
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION};
  }

  TRI_ASSERT(res->status == CL_COMM_SENT);

  return {TRI_ERROR_NO_ERROR};
}

/**
 * @brief Steal the last returned body. Will throw an error if
 *        there has been an error of any kind, e.g. communication
 *        or error created by remote server.
 *        Will reset the lastResponse, so after this call we are
 *        ready to send a new request.
 *
 * @return A shared_ptr containing the remote response.
 */
std::shared_ptr<VPackBuilder> RemoteBlock::stealResultBody() {
  if (!_lastError.ok()) {
    THROW_ARANGO_EXCEPTION(_lastError);
  }
  // We have an open result still.
  // Result is the response which is an object containing the ErrorCode
  std::shared_ptr<VPackBuilder> responseBodyBuilder = _lastResponse->getBodyVelocyPack();
  _lastResponse.reset();
  return responseBodyBuilder;
}

/// @brief timeout
double const RemoteBlock::defaultTimeOut = 3600.0;

/// @brief creates a remote block
RemoteBlock::RemoteBlock(ExecutionEngine* engine, RemoteNode const* en,
                         std::string const& server, std::string const& ownName,
                         std::string const& queryId)
    : ExecutionBlock(engine, en),
      _server(server),
      _ownName(ownName),
      _queryId(queryId),
      _isResponsibleForInitializeCursor(
          en->isResponsibleForInitializeCursor()),
      _lastResponse(nullptr),
      _lastError(TRI_ERROR_NO_ERROR) {
  TRI_ASSERT(!queryId.empty());
  TRI_ASSERT(
      (arangodb::ServerState::instance()->isCoordinator() && ownName.empty()) ||
      (!arangodb::ServerState::instance()->isCoordinator() &&
       !ownName.empty()));
}

Result RemoteBlock::sendAsyncRequest(
    arangodb::rest::RequestType type, std::string const& urlPart,
    std::shared_ptr<std::string const> body) {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  // Later, we probably want to set these sensibly:
  CoordTransactionID const coordTransactionId = TRI_NewTickServer();
  std::unordered_map<std::string, std::string> headers;
  if (!_ownName.empty()) {
    headers.emplace("Shard-Id", _ownName);
  }
    
  std::string url = std::string("/_db/") +
    arangodb::basics::StringUtils::urlEncode(_engine->getQuery()->trx()->vocbase().name()) + 
    urlPart + _queryId;

  ++_engine->_stats.requests;
  std::shared_ptr<ClusterCommCallback> callback =
      std::make_shared<WakeupQueryCallback>(this, _engine->getQuery());

  // TODO Returns OperationID do we need it in any way?
  cc->asyncRequest(coordTransactionId, _server, type,
                   std::move(url), body, headers, callback, defaultTimeOut,
                   true);

  return {TRI_ERROR_NO_ERROR};
}

/// @brief initializeCursor, could be called multiple times
std::pair<ExecutionState, Result> RemoteBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  // For every call we simply forward via HTTP

  if (!_isResponsibleForInitializeCursor) {
    // do nothing...
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (items == nullptr) {
    // we simply ignore the initialCursor request, as the remote side
    // will initialize the cursor lazily
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (_lastResponse != nullptr || _lastError.fail()) {
    // We have an open result still.
    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();

    // Result is the response which is an object containing the ErrorCode
    VPackSlice slice = responseBodyBuilder->slice();
    if (slice.hasKey("code")) {
      return {ExecutionState::DONE, slice.get("code").getNumericValue<int>()};
    }
    return {ExecutionState::DONE, TRI_ERROR_INTERNAL};
  }

  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuilder builder(&options);
  builder.openObject();

  // Backwards Compatibility 3.3
  builder.add("exhausted", VPackValue(false));
  // Used in 3.4.0 onwards
  builder.add("done", VPackValue(false));
  builder.add("error", VPackValue(false));
  builder.add("pos", VPackValue(pos));
  builder.add(VPackValue("items"));
  builder.openObject();
  items->toVelocyPack(_engine->getQuery()->trx(), builder);
  builder.close();

  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(
      rest::RequestType::PUT, "/_api/aql/initializeCursor/", bodyString);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
}

bool RemoteBlock::handleAsyncResult(ClusterCommResult* result) {
  // TODO Handle exceptions thrown while we are in this code
  // Query will not be woken up again.
  _lastError = handleCommErrors(result);
  if (_lastError.ok()) {
    _lastResponse = result->result;
  }
  return true;
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> RemoteBlock::shutdown(int errorCode) {
  /* We need to handle this here in ASYNC case
    if (isShutdown && errorNum == TRI_ERROR_QUERY_NOT_FOUND) {
      // this error may happen on shutdown and is thus tolerated
      // pass the info to the caller who can opt to ignore this error
      return true;
    }
  */

  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = _lastError;
    _lastError.reset();
    // we were called with an error need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());

    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice slice = responseBodyBuilder->slice();
    if (slice.isObject()) {
      if (slice.hasKey("stats")) {
        ExecutionStats newStats(slice.get("stats"));
        _engine->_stats.add(newStats);
      }

      // read "warnings" attribute if present and add it to our query
      VPackSlice warnings = slice.get("warnings");
      if (warnings.isArray()) {
        auto query = _engine->getQuery();
        for (auto const& it : VPackArrayIterator(warnings)) {
          if (it.isObject()) {
            VPackSlice code = it.get("code");
            VPackSlice message = it.get("message");
            if (code.isNumber() && message.isString()) {
              query->registerWarning(code.getNumericValue<int>(),
                                     message.copyString().c_str());
            }
          }
        }
      }
      if (slice.hasKey("code")) {
        return {ExecutionState::DONE, slice.get("code").getNumericValue<int>()};
      }
    }

    return {ExecutionState::DONE, TRI_ERROR_INTERNAL};
  }

  // For every call we simply forward via HTTP
  VPackBuilder bodyBuilder;
  bodyBuilder.openObject();
  bodyBuilder.add("code", VPackValue(errorCode));
  bodyBuilder.close();

  auto bodyString =
      std::make_shared<std::string const>(bodyBuilder.slice().toJson());

  auto res = sendAsyncRequest(rest::RequestType::PUT, "/_api/aql/shutdown/",
                              bodyString);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> RemoteBlock::getSome(size_t atMost) {
  // For every call we simply forward via HTTP
  traceGetSomeBegin(atMost);

  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = _lastError;
    _lastError.reset();
    // we were called with an error need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());
    // We do not have an error but a result, all is good
    // We have an open result still.
    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();
    // Result is the response which will be a serialized AqlItemBlock

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice responseBody = responseBodyBuilder->slice();

    ExecutionState state = ExecutionState::HASMORE;
    if (VelocyPackHelper::getBooleanValue(responseBody, "done", true)) {
      state = ExecutionState::DONE;
    }
    if (responseBody.hasKey("data")) {
      auto r = std::make_unique<AqlItemBlock>(
          _engine->getQuery()->resourceMonitor(), responseBody);
      traceGetSomeEnd(r.get(), state);
      return {state, std::move(r)};
    }
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  // We need to send a request here
  VPackBuilder builder;
  builder.openObject();
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(rest::RequestType::PUT, "/_api/aql/getSome/",
                              bodyString);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  traceGetSomeEnd(nullptr, ExecutionState::WAITING);
  return {ExecutionState::WAITING, nullptr};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> RemoteBlock::skipSome(size_t atMost) {
  if (_lastError.fail()) {
    TRI_ASSERT(_lastResponse == nullptr);
    Result res = _lastError;
    _lastError.reset();
    // we were called with an error need to throw it.
    THROW_ARANGO_EXCEPTION(res);
  }
  
  traceSkipSomeBegin(atMost);
  
  if (_lastResponse != nullptr) {
    TRI_ASSERT(_lastError.ok());

    // We have an open result still.
    // Result is the response which will be a serialized AqlItemBlock
    std::shared_ptr<VPackBuilder> responseBodyBuilder = stealResultBody();

    // both must be reset before return or throw
    TRI_ASSERT(_lastError.ok() && _lastResponse == nullptr);

    VPackSlice slice = responseBodyBuilder->slice();

    if (!slice.hasKey(StaticStrings::Error) ||
        slice.get(StaticStrings::Error).getBoolean()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
    }
    size_t skipped = 0;
    VPackSlice s = slice.get("skipped");
    if (s.isNumber()) {
      int64_t value = s.getNumericValue<int64_t>();
      if (value < 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "skipped cannot be negative");
      }
      skipped = s.getNumericValue<size_t>();
    }

    // TODO Check if we can get better with HASMORE/DONE
    if (skipped == 0) {
      traceSkipSomeEnd(skipped, ExecutionState::DONE);
      return {ExecutionState::DONE, skipped};
    }
    traceSkipSomeEnd(skipped, ExecutionState::HASMORE);
    return {ExecutionState::HASMORE, skipped};
  }

  // For every call we simply forward via HTTP

  VPackBuilder builder;
  builder.openObject();
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  auto bodyString = std::make_shared<std::string const>(builder.slice().toJson());

  auto res = sendAsyncRequest(rest::RequestType::PUT, "/_api/aql/skipSome/",
                              bodyString);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  traceSkipSomeEnd(0, ExecutionState::WAITING);
  return {ExecutionState::WAITING, 0};
}

// -----------------------------------------------------------------------------
// -- SECTION --                                            UnsortingGatherBlock
// -----------------------------------------------------------------------------

/// @brief initializeCursor
std::pair<ExecutionState, arangodb::Result> UnsortingGatherBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING || !res.second.ok()) {
    return res;
  }

  _atDep = 0;
  _done = _dependencies.empty();

  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> UnsortingGatherBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  _done = _dependencies.empty();

  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  // the simple case ...
  auto res = _dependencies[_atDep]->getSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    traceGetSomeEnd(nullptr, ExecutionState::WAITING);
    return res;
  }

  while (res.second == nullptr && _atDep < _dependencies.size() - 1) {
    _atDep++;
    res = _dependencies[_atDep]->getSome(atMost);
    if (res.first == ExecutionState::WAITING) {
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return res;
    }
  }

  _done = (nullptr == res.second);

  traceGetSomeEnd(res.second.get(), getHasMoreState());
  return {getHasMoreState(), std::move(res.second)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> UnsortingGatherBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  // the simple case . . .
  auto res = _dependencies[_atDep]->skipSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    traceSkipSomeEnd(res.second, ExecutionState::WAITING);
    return res;
  }

  while (res.second == 0 && _atDep < _dependencies.size() - 1) {
    _atDep++;
    res = _dependencies[_atDep]->skipSome(atMost);
    if (res.first == ExecutionState::WAITING) {
      traceSkipSomeEnd(res.second, ExecutionState::WAITING);
      return res;
    }
  }

  _done = (res.second == 0);

  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(res.second, state);
  return {state, res.second};
}

// -----------------------------------------------------------------------------
// -- SECTION --                                              SortingGatherBlock
// -----------------------------------------------------------------------------


SortingGatherBlock::SortingGatherBlock(
    ExecutionEngine& engine,
    GatherNode const& en)
  : ExecutionBlock(&engine, &en) {
  TRI_ASSERT(!en.elements().empty());

  switch (en.sortMode()) {
    case GatherNode::SortMode::Heap:
      _strategy = std::make_unique<HeapSorting>(
        _trx, _gatherBlockBuffer, _sortRegisters
      );
      break;
    case GatherNode::SortMode::MinElement:
      _strategy = std::make_unique<MinElementSorting>(
        _trx, _gatherBlockBuffer, _sortRegisters
      );
      break;
    default:
      TRI_ASSERT(false);
      break;
  }
  TRI_ASSERT(_strategy);

  // We know that planRegisters has been run, so
  // getPlanNode()->_registerPlan is set up
  SortRegister::fill(
    *en.plan(),
    *en.getRegisterPlan(),
    en.elements(),
    _sortRegisters
  );
}
  
SortingGatherBlock::~SortingGatherBlock() {
  clearBuffers();
}

void SortingGatherBlock::clearBuffers() noexcept {
  for (std::deque<AqlItemBlock*>& it : _gatherBlockBuffer) {
    for (AqlItemBlock* b : it) {
      delete b;
    }
    it.clear();
  }
}

/// @brief initializeCursor
std::pair<ExecutionState, arangodb::Result>
SortingGatherBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING || !res.second.ok()) {
    return res;
  }

  clearBuffers();

  TRI_ASSERT(!_dependencies.empty());

  if (_gatherBlockBuffer.empty()) {
    // only do this initialization once
    _gatherBlockBuffer.reserve(_dependencies.size());
    _gatherBlockPos.reserve(_dependencies.size());
    _gatherBlockPosDone.reserve(_dependencies.size());

    for (size_t i = 0; i < _dependencies.size(); ++i) {
      _gatherBlockBuffer.emplace_back();
      _gatherBlockPos.emplace_back(i, 0);
      _gatherBlockPosDone.push_back(false);
    }
  } else {
    for (size_t i = 0; i < _dependencies.size(); i++) {
      TRI_ASSERT(_gatherBlockBuffer[i].empty());
      _gatherBlockPos[i].second = 0;
      _gatherBlockPosDone[i] = false;
    }
  }
  
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPosDone.size() == _dependencies.size());

  _strategy->reset();

  _done = _dependencies.empty();

  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

/**
 * @brief Fills all _gatherBlockBuffer entries. Is repeatable during WAITING.
 *
 *
 * @param atMost The amount of data requested per block.
 * @param nonEmptyIndex an index of a non-empty GatherBlock buffer
 *
 * @return Will return {WAITING, 0} if it had to request new data from upstream.
 *         If everything is in place: all buffers are either filled with at
 *         least "atMost" rows, or the upstream block is DONE.
 *         Will return {DONE, SUM(_gatherBlockBuffer)} on success.
 */
std::pair<ExecutionState, size_t> SortingGatherBlock::fillBuffers(
    size_t atMost) {
  size_t available = 0;
  
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());

  // In the future, we should request all blocks in parallel. But not everything
  // is yet thread safe for that to work, so we have to return immediately on
  // the first WAITING we encounter.
  for (size_t i = 0; i < _dependencies.size(); i++) {
    // reset position to 0 if we're going to fetch a new block.
    // this doesn't hurt, even if we don't get one.
    if (_gatherBlockBuffer[i].empty()) {
      _gatherBlockPos[i].second = 0;
    }
    ExecutionState state;
    bool blockAppended;
    std::tie(state, blockAppended) = getBlocks(i, atMost);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, 0};
    }

    available += availableRows(i);
  }

  return {ExecutionState::DONE, available};
}

/// @brief Returns the number of unprocessed rows in the buffer i.
size_t SortingGatherBlock::availableRows(size_t i) const {
  size_t available = 0;

  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(i < _dependencies.size());

  auto const& blocks = _gatherBlockBuffer[i];
  size_t curRowIdx = _gatherBlockPos[i].second;

  if (!blocks.empty()) {
    TRI_ASSERT(blocks[0]->size() >= curRowIdx);
    // the first block may already be partially processed
    available += blocks[0]->size() - curRowIdx;
  }

  // add rows from all additional blocks
  for (size_t j = 1; j < blocks.size(); ++j) {
    available += blocks[j]->size();
  }

  return available;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
SortingGatherBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_dependencies.empty()) {
    _done = true;
  }

  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  // the non-simple case . . .

  // pull more blocks from dependencies . . .
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockBuffer.size() == _gatherBlockPos.size());
  
  size_t available = 0;
  {
    ExecutionState blockState;
    std::tie(blockState, available) = fillBuffers(atMost);
    if (blockState == ExecutionState::WAITING) {
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return {blockState, nullptr};
    }
  }

  if (available == 0) {
    _done = true;
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  size_t toSend = (std::min)(available, atMost);  // nr rows in outgoing block

  // the following is similar to AqlItemBlock's slice method . . .
  std::vector<std::unordered_map<AqlValue, AqlValue>> cache;
  cache.resize(_dependencies.size());

  size_t nrRegs = getNrInputRegisters();

  // automatically deleted if things go wrong
  std::unique_ptr<AqlItemBlock> res(
      requestBlock(toSend, static_cast<arangodb::aql::RegisterId>(nrRegs)));

  _strategy->prepare(_gatherBlockPos);

  for (size_t i = 0; i < toSend; i++) {
    // get the next smallest row from the buffer . . .
    auto const val = _strategy->nextValue();
    auto const& blocks = _gatherBlockBuffer[val.first];

    // copy the row in to the outgoing block . . .
    for (RegisterId col = 0; col < nrRegs; col++) {
      TRI_ASSERT(!blocks.empty());
      AqlValue const& x = blocks.front()->getValueReference(val.second, col);
      if (!x.isEmpty()) {
        if (x.requiresDestruction()) {
          // complex value, with ownership transfer
          auto it = cache[val.first].find(x);

          if (it == cache[val.first].end()) {
            AqlValue y = x.clone();
            try {
              res->setValue(i, col, y);
            } catch (...) {
              y.destroy();
              throw;
            }
            cache[val.first].emplace(x, y);
          } else {
            res->setValue(i, col, (*it).second);
          }
        } else {
          // simple value, no ownership transfer needed
          res->setValue(i, col, x);
        }
      }
    }

    nextRow(val.first);
  }

  traceGetSomeEnd(res.get(), getHasMoreState());
  return {getHasMoreState(), std::move(res)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> SortingGatherBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  // the non-simple case . . .
  TRI_ASSERT(!_dependencies.empty());

  size_t available = 0;
  {
    ExecutionState blockState;
    std::tie(blockState, available) = fillBuffers(atMost);
    if (blockState == ExecutionState::WAITING) {
      traceSkipSomeEnd(0, ExecutionState::WAITING);
      return {blockState, 0};
    }
  }

  if (available == 0) {
    _done = true;
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  size_t const skipped = (std::min)(available, atMost);  // nr rows in outgoing block

  _strategy->prepare(_gatherBlockPos);

  for (size_t i = 0; i < skipped; i++) {
    // get the next smallest row from the buffer . . .
    auto const val = _strategy->nextValue();

    nextRow(val.first);
  }

  // Maybe we can optimize here DONE/HASMORE
  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

/// @brief Step to the next row in line in the buffers of dependency i, i.e.,
/// updates _gatherBlockBuffer and _gatherBlockPos. If necessary, steps to the
/// next block and removes the previous one. Will not fetch more blocks.
void SortingGatherBlock::nextRow(size_t i) {
  TRI_ASSERT(i < _dependencies.size());
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());

  auto& blocks = _gatherBlockBuffer[i];
  auto& blocksPos = _gatherBlockPos[i];
  if (++blocksPos.second == blocks.front()->size()) {
    TRI_ASSERT(!blocks.empty());
    AqlItemBlock* cur = blocks.front();
    returnBlock(cur);
    blocks.pop_front();
    blocksPos.second = 0; // reset position within a dependency
  }
}

/// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
/// non-simple case only
/// Assures that either atMost rows are actually available in buffer i, or
/// the dependency is DONE.
std::pair<ExecutionState, bool> SortingGatherBlock::getBlocks(size_t i,
                                                              size_t atMost) {
  TRI_ASSERT(i < _dependencies.size());
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPosDone.size() == _dependencies.size());

  if (_gatherBlockPosDone[i]) {
    return {ExecutionState::DONE, false};
  }

  bool blockAppended = false;
  size_t rowsAvailable = availableRows(i);
  ExecutionState state = ExecutionState::HASMORE;

  // repeat until either
  // - enough rows are fetched
  // - dep[i] is DONE
  // - dep[i] is WAITING
  while (state == ExecutionState::HASMORE && rowsAvailable < atMost) {
    std::unique_ptr<AqlItemBlock> itemBlock;
    std::tie(state, itemBlock) = _dependencies[i]->getSome(atMost);

    // Assert that state == WAITING => itemBlock == nullptr
    TRI_ASSERT(state != ExecutionState::WAITING || itemBlock == nullptr);

    if (state == ExecutionState::DONE) {
      _gatherBlockPosDone[i] = true;
    }

    if (itemBlock && itemBlock->size() > 0) {
      rowsAvailable += itemBlock->size();
      _gatherBlockBuffer[i].emplace_back(itemBlock.get());
      itemBlock.release();
      blockAppended = true;
    }
  }

  TRI_ASSERT(state == ExecutionState::WAITING ||
             state == ExecutionState::DONE || rowsAvailable >= atMost);

  return {state, blockAppended};
}

/// @brief timeout
double const SingleRemoteOperationBlock::defaultTimeOut = 3600.0;

/// @brief creates a remote block
SingleRemoteOperationBlock::SingleRemoteOperationBlock(ExecutionEngine* engine,
                                                       SingleRemoteOperationNode const* en
                                                       )
    : ExecutionBlock(engine, static_cast<ExecutionNode const*>(en)),
      _collection(en->collection()),
      _key(en->key())
{
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
}

namespace {
std::unique_ptr<VPackBuilder>
merge(VPackSlice document, std::string const& key, TRI_voc_rid_t revision){
  auto builder = std::make_unique<VPackBuilder>() ;
  {
    VPackObjectBuilder guard(builder.get());
    TRI_SanitizeObject(document, *builder);
    VPackSlice keyInBody = document.get(StaticStrings::KeyString);

    if (keyInBody.isNone() ||
        keyInBody.isNull() ||
        (keyInBody.isString() && keyInBody.copyString() != key) ||
        ((revision != 0) && (TRI_ExtractRevisionId(document) != revision))
        ) {
      // We need to rewrite the document with the given revision and key:
      builder->add(StaticStrings::KeyString, VPackValue(key));
      if (revision != 0) {
        builder->add(StaticStrings::RevString, VPackValue(TRI_RidToString(revision)));
      }
    }
  }
  return builder;
}
}

bool SingleRemoteOperationBlock::getOne(arangodb::aql::AqlItemBlock* aqlres,
                                        size_t outputCounter) {
  int possibleWrites = 0; // TODO - get real statistic values!
  auto node = ExecutionNode::castTo<SingleRemoteOperationNode const*>(getPlanNode());
  auto out = node->_outVariable;
  auto in = node->_inVariable;
  auto OLD = node->_outVariableOld;
  auto NEW = node->_outVariableNew;

  RegisterId inRegId  = ExecutionNode::MaxRegisterId;
  RegisterId outRegId = ExecutionNode::MaxRegisterId;
  RegisterId oldRegId = ExecutionNode::MaxRegisterId;
  RegisterId newRegId = ExecutionNode::MaxRegisterId;

  if (in != nullptr) {
    auto itIn = node->getRegisterPlan()->varInfo.find(in->id);
    TRI_ASSERT(itIn != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    inRegId = (*itIn).second.registerId;
  }

  if (_key.empty() && in == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, "missing document reference");
  }

  if (out != nullptr) {
    auto itOut = node->getRegisterPlan()->varInfo.find(out->id);
    TRI_ASSERT(itOut != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    outRegId = (*itOut).second.registerId;
  }

  if (OLD != nullptr) {
    auto itOld = node->getRegisterPlan()->varInfo.find(OLD->id);
    TRI_ASSERT(itOld != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOld).second.registerId < ExecutionNode::MaxRegisterId);
    oldRegId = (*itOld).second.registerId;
  }

  if (NEW != nullptr) {
    auto itNew = node->getRegisterPlan()->varInfo.find(NEW->id);
    TRI_ASSERT(itNew != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itNew).second.registerId < ExecutionNode::MaxRegisterId);
    newRegId = (*itNew).second.registerId;
  }

  VPackBuilder inBuilder;
  VPackSlice inSlice = VPackSlice::emptyObjectSlice();
  if (in) {// IF NOT REMOVE OR SELECT
    if (_buffer.size() < 1) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, "missing document reference in Register");
    }
    AqlValue const& inDocument = _buffer.front()->getValueReference(_pos, inRegId);
    inBuilder.add(inDocument.slice());
    inSlice = inBuilder.slice();
  }

  auto const& nodeOps = node->_options;

  OperationOptions opOptions;
  opOptions.ignoreRevs = nodeOps.ignoreRevs;
  opOptions.keepNull = !nodeOps.nullMeansRemove;
  opOptions.mergeObjects = nodeOps.mergeObjects;
  opOptions.returnNew = !!NEW;
  opOptions.returnOld = (!!OLD) || out;
  opOptions.waitForSync = nodeOps.waitForSync;
  opOptions.silent = false;
  opOptions.overwrite = nodeOps.overwrite;

  std::unique_ptr<VPackBuilder> mergedBuilder;
  if (!_key.empty()) {
    mergedBuilder = merge(inSlice, _key, 0);
    inSlice = mergedBuilder->slice();
  }

  OperationResult result;
  if (node->_mode == ExecutionNode::NodeType::INDEX) {
    result = _trx->document(_collection->name(), inSlice, opOptions);
  } else if (node->_mode == ExecutionNode::NodeType::INSERT) {
    if (opOptions.returnOld && !opOptions.overwrite) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN,
                                     "OLD is only available when using INSERT with the overwrite option");
    }
    result = _trx->insert(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::REMOVE) {
    result = _trx->remove(_collection->name(), inSlice , opOptions);
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::REPLACE) {
    if (node->_replaceIndexNode && in == nullptr) {
      // we have a FOR .. IN FILTER doc._key == ... REPLACE - no WITH.
      // in this case replace needs to behave as if it was UPDATE.
      result = _trx->update(_collection->name(), inSlice, opOptions);
    } else {
      result = _trx->replace(_collection->name(), inSlice, opOptions);
    }
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::UPDATE) {
    result = _trx->update(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  }

  // check operation result
  if (!result.ok()) {
    if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
        (( node->_mode == ExecutionNode::NodeType::INDEX) ||
         ( node->_mode == ExecutionNode::NodeType::UPDATE && node->_replaceIndexNode) ||
         ( node->_mode == ExecutionNode::NodeType::REMOVE && node->_replaceIndexNode) ||
         ( node->_mode == ExecutionNode::NodeType::REPLACE && node->_replaceIndexNode) ))
      {
        // document not there is not an error in this situation.
        // FOR ... FILTER ... REMOVE wouldn't invoke REMOVE in first place, so don't throw an excetpion.
        return false;
      } else if (!nodeOps.ignoreErrors) { // TODO remove if
      THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
    }

    if (node->_mode == ExecutionNode::NodeType::INDEX) {
      return false;
    }
  }

  _engine->_stats.writesExecuted += possibleWrites;
  _engine->_stats.scannedIndex++;

  if (!(out || OLD || NEW)) {
    return node->hasParent();
  }

  // Fill itemblock
  // create block that can hold a result with one entry and a number of variables
  // corresponding to the amount of out variables

  // only copy 1st row of registers inherited from previous frame(s)
  TRI_ASSERT(result.ok());
  VPackSlice outDocument = VPackSlice::nullSlice();
  if (result.buffer) {
    outDocument = result.slice().resolveExternal();
  }

  VPackSlice oldDocument = VPackSlice::nullSlice();
  VPackSlice newDocument = VPackSlice::nullSlice();
  if (outDocument.isObject()) {
    if (NEW && outDocument.hasKey("new")) {
      newDocument = outDocument.get("new");
    }
    if (outDocument.hasKey("old")) {
      outDocument = outDocument.get("old");
      if (OLD) {
        oldDocument = outDocument;
      }
    }
  }
  
  TRI_ASSERT(out || OLD || NEW);

  // place documents as in the out variable slots of the result
  if (out) {
    aqlres->emplaceValue(outputCounter, static_cast<arangodb::aql::RegisterId>(outRegId), outDocument);
  }

  if (OLD) {
    TRI_ASSERT(opOptions.returnOld);
    aqlres->emplaceValue(outputCounter, static_cast<arangodb::aql::RegisterId>(oldRegId), oldDocument);
  }

  if (NEW) {
    TRI_ASSERT(opOptions.returnNew);
    aqlres->emplaceValue(outputCounter, static_cast<arangodb::aql::RegisterId>(newRegId), newDocument);
  }

  throwIfKilled();  // check if we were aborted

  TRI_IF_FAILURE("SingleRemoteOperationBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return true;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> SingleRemoteOperationBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_done) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return { ExecutionState::DONE, nullptr};
  }

  RegisterId nrRegs = getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];
  std::unique_ptr<AqlItemBlock> aqlres(requestBlock(atMost, nrRegs));

  int outputCounter = 0;
  if (_buffer.empty()) {
    size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
    ExecutionState state = ExecutionState::HASMORE;
    bool blockAppended = false;

    std::tie(state, blockAppended) = ExecutionBlock::getBlock(toFetch);
    if(state == ExecutionState::WAITING) {
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return {state, nullptr};
    }
    if (!blockAppended) {
      _done = true;
      traceGetSomeEnd(nullptr, ExecutionState::DONE);
      return { ExecutionState::DONE, nullptr};
    }
    _pos = 0;  // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  arangodb::aql::AqlItemBlock* cur = _buffer.front();
  TRI_ASSERT(cur != nullptr);
  size_t n = cur->size();
  for (size_t i = 0; i < n; i++) {
    inheritRegisters(cur, aqlres.get(), _pos);
    if (getOne(aqlres.get(), outputCounter)) {
      outputCounter++;
    }
    _done = true;
    _pos++;
  }
  _buffer.pop_front();  // does not throw
  returnBlock(cur);
  _pos = 0;
  if (outputCounter == 0) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return { ExecutionState::DONE, nullptr};
  }
  aqlres->shrink(outputCounter);

  // Clear out registers no longer needed later:
  clearRegisters(aqlres.get());
  traceGetSomeEnd(aqlres.get(), ExecutionState::DONE);
  return { ExecutionState::DONE, std::move(aqlres) };
}

/// @brief skipSome
std::pair<ExecutionState, size_t> SingleRemoteOperationBlock::skipSome(size_t atMost) {
  TRI_ASSERT(false); // as soon as we need to support LIMIT change me.
  return { ExecutionState::DONE, 0};
}
