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
      std::vector<std::deque<AqlItemBlock*>>& gatherBlockBuffer,
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
  std::vector<std::deque<AqlItemBlock*>>& _gatherBlockBuffer;
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
#ifdef USE_IRESEARCH
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
      std::vector<std::deque<AqlItemBlock*>>& gatherBlockBuffer,
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
      std::vector<std::deque<AqlItemBlock*>>& gatherBlockBuffer,
      std::vector<SortRegister>& sortRegisters) noexcept
    : OurLessThan(trx, gatherBlockBuffer, sortRegisters) {
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

/// @brief initializeCursor: reset _doneForClient
int BlockWithClients::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();

  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _doneForClient.clear();
  _doneForClient.reserve(_nrClients);

  for (size_t i = 0; i < _nrClients; i++) {
    _doneForClient.push_back(false);
  }

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief shutdown
int BlockWithClients::shutdown(int errorCode) {
  DEBUG_BEGIN_BLOCK();

  _doneForClient.clear();

  if (_wasShutdown) {
    return TRI_ERROR_NO_ERROR;
  }
  int res = ExecutionBlock::shutdown(errorCode);
  _wasShutdown = true;
  return res;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getSomeForShard
AqlItemBlock* BlockWithClients::getSomeForShard(size_t atMost,
                                                std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = 0;
  AqlItemBlock* result = nullptr;

  int out =
      getOrSkipSomeForShard(atMost, false, result, skipped, shardId);

  if (out == TRI_ERROR_NO_ERROR) {
    return result;
  }

  if (result != nullptr) {
    delete result;
  }

  THROW_ARANGO_EXCEPTION(out);

  DEBUG_END_BLOCK();
}

/// @brief skipSomeForShard
size_t BlockWithClients::skipSomeForShard(size_t atMost, std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = 0;
  AqlItemBlock* result = nullptr;
  int out =
      getOrSkipSomeForShard(atMost, true, result, skipped, shardId);
  TRI_ASSERT(result == nullptr);
  if (out != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(out);
  }
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief skipForShard
bool BlockWithClients::skipForShard(size_t number, std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = skipSomeForShard(number, shardId);
  size_t nr = skipped;
  while (nr != 0 && skipped < number) {
    nr = skipSomeForShard(number - skipped, shardId);
    skipped += nr;
  }
  if (nr == 0) {
    return true;
  }
  return !hasMoreForShard(shardId);

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getClientId: get the number <clientId> (used internally)
/// corresponding to <shardId>
size_t BlockWithClients::getClientId(std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();
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

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief initializeCursor
int ScatterBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();

  int res = BlockWithClients::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // local clean up
  _posForClient.clear();

  for (size_t i = 0; i < _nrClients; i++) {
    _posForClient.emplace_back(0, 0);
  }
  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief initializeCursor
int ScatterBlock::shutdown(int errorCode) {
  DEBUG_BEGIN_BLOCK();
  int res = BlockWithClients::shutdown(errorCode);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // local clean up
  _posForClient.clear();

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief hasMoreForShard: any more for shard <shardId>?
bool ScatterBlock::hasMoreForShard(std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();
  
  TRI_ASSERT(_nrClients != 0);

  size_t const clientId = getClientId(shardId);

  // reference to "a finish mark" for a client
  TRI_ASSERT(_doneForClient.size() > clientId);
  std::vector<bool>::reference doneForClient = _doneForClient[clientId];

  if (doneForClient) {
    return false;
  }

  TRI_ASSERT(_posForClient.size() > clientId);
  std::pair<size_t, size_t> const& pos = _posForClient[clientId];
  // (i, j) where i is the position in _buffer, and j is the position in
  // _buffer[i] we are sending to <clientId>

  if (pos.first > _buffer.size()) {
    if (!ExecutionBlock::getBlock(DefaultBatchSize())) {
      doneForClient = true;
      return false;
    }
  }
  return true;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getOrSkipSomeForShard
int ScatterBlock::getOrSkipSomeForShard(size_t atMost,
                                        bool skipping, AqlItemBlock*& result,
                                        size_t& skipped,
                                        std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();
  TRI_ASSERT(result == nullptr && skipped == 0);
  TRI_ASSERT(atMost > 0);

  size_t const clientId = getClientId(shardId);

  // reference to "a finish mark" for a client
  TRI_ASSERT(_doneForClient.size() > clientId);
  std::vector<bool>::reference doneForClient = _doneForClient[clientId];

  if (doneForClient) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_posForClient.size() > clientId);
  std::pair<size_t, size_t>& pos = _posForClient[clientId];

  // pull more blocks from dependency if necessary . . .
  if (pos.first >= _buffer.size()) {
    if (!getBlock(atMost)) {
      doneForClient = true;
      return TRI_ERROR_NO_ERROR;
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

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
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
int DistributeBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();

  int res = BlockWithClients::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // local clean up
  _distBuffer.clear();
  _distBuffer.reserve(_nrClients);

  for (size_t i = 0; i < _nrClients; i++) {
    _distBuffer.emplace_back();
  }

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief shutdown
int DistributeBlock::shutdown(int errorCode) {
  DEBUG_BEGIN_BLOCK();
  int res = BlockWithClients::shutdown(errorCode);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // local clean up
  _distBuffer.clear();

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief hasMore: any more for any shard?
bool DistributeBlock::hasMoreForShard(std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();

  size_t const clientId = getClientId(shardId);

  // reference to "a finish mark" for a client
  TRI_ASSERT(_doneForClient.size() > clientId);
  std::vector<bool>::reference doneForClient = _doneForClient[clientId];

  if (doneForClient) {
    return false;
  }

  TRI_ASSERT(_distBuffer.size() > clientId);
  if (!_distBuffer[clientId].empty()) {
    return true;
  }

  if (!getBlockForClient(DefaultBatchSize(), clientId)) {
    doneForClient = true;
    return false;
  }
  return true;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getOrSkipSomeForShard
int DistributeBlock::getOrSkipSomeForShard(size_t atMost,
                                           bool skipping, AqlItemBlock*& result,
                                           size_t& skipped,
                                           std::string const& shardId) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin(atMost);
  TRI_ASSERT(result == nullptr && skipped == 0);
  TRI_ASSERT(atMost > 0);

  size_t const clientId = getClientId(shardId);
  TRI_ASSERT(_doneForClient.size() > clientId);

  // reference to "a finish mark" for a client
  std::vector<bool>::reference doneForClient = _doneForClient[clientId];

  if (doneForClient) {
    traceGetSomeEnd(result);
    return TRI_ERROR_NO_ERROR;
  }

  std::deque<std::pair<size_t, size_t>>& buf = _distBuffer[clientId];

  if (buf.empty()) {
    if (!getBlockForClient(atMost, clientId)) {
      doneForClient = true;
      traceGetSomeEnd(result);
      return TRI_ERROR_NO_ERROR;
    }
  }

  skipped = (std::min)(buf.size(), atMost);

  if (skipping) {
    for (size_t i = 0; i < skipped; i++) {
      buf.pop_front();
    }
    traceGetSomeEnd(result);
    return TRI_ERROR_NO_ERROR;
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
    result = collector.steal();
  }

  // _buffer is left intact, deleted and cleared at shutdown

  traceGetSomeEnd(result);
  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getBlockForClient: try to get atMost pairs into
/// _distBuffer.at(clientId), this means we have to look at every row in the
/// incoming blocks until they run out or we find enough rows for clientId. We
/// also keep track of blocks which should be sent to other clients than the
/// current one.
bool DistributeBlock::getBlockForClient(size_t atMost, size_t clientId) {
  DEBUG_BEGIN_BLOCK();
  if (_buffer.empty()) {
    _index = 0;  // position in _buffer
    _pos = 0;    // position in _buffer.at(_index)
  }

  // it should be the case that buf.at(clientId) is empty
  auto& buf = _distBuffer[clientId];

  while (buf.size() < atMost) {
    if (_index == _buffer.size()) {
      if (!ExecutionBlock::getBlock(atMost)) {
        if (buf.size() == 0) {
          _doneForClient[clientId] = true;
          return false;
        }
        break;
      }
    }

    AqlItemBlock* cur = _buffer[_index];

    while (_pos < cur->size() && buf.size() < atMost) {
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

  return true;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief sendToClient: for each row of the incoming AqlItemBlock use the
/// attributes <shardKeys> of the Aql value <val> to determine to which shard
/// the row should be sent and return its clientId
size_t DistributeBlock::sendToClient(AqlItemBlock* cur) {
  DEBUG_BEGIN_BLOCK();

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
  bool usesDefaultShardingAttributes;
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collInfo = _collection->getCollection();

  int res = clusterInfo->getResponsibleShard(collInfo.get(), value, true,
      shardId, usesDefaultShardingAttributes);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(!shardId.empty());

  return getClientId(shardId);

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief create a new document key, argument is unused here
#ifndef USE_ENTERPRISE
std::string DistributeBlock::createKey(VPackSlice) const {
  auto collInfo = _collection->getCollection();
  return collInfo->keyGenerator()->generate();
}
#endif

/// @brief local helper to throw an exception if a HTTP request went wrong
static bool throwExceptionAfterBadSyncRequest(ClusterCommResult* res,
                                              bool isShutdown) {
  DEBUG_BEGIN_BLOCK();
  if (res->status == CL_COMM_TIMEOUT ||
      res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res->getErrorCode(),
                                   res->stringifyErrorMessage());
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

    if (isShutdown && errorNum == TRI_ERROR_QUERY_NOT_FOUND) {
      // this error may happen on shutdown and is thus tolerated
      // pass the info to the caller who can opt to ignore this error
      return true;
    }

    // In this case a proper HTTP error was reported by the DBserver,
    if (errorNum > 0 && !errorMessage.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(errorNum, errorMessage);
    }

    // default error
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
  }

  return false;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
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
          en->isResponsibleForInitializeCursor()) {
  TRI_ASSERT(!queryId.empty());
  TRI_ASSERT(
      (arangodb::ServerState::instance()->isCoordinator() && ownName.empty()) ||
      (!arangodb::ServerState::instance()->isCoordinator() &&
       !ownName.empty()));
}

/// @brief local helper to send a request
std::unique_ptr<ClusterCommResult> RemoteBlock::sendRequest(
    arangodb::rest::RequestType type, std::string const& urlPart,
    std::string const& body) const {
  DEBUG_BEGIN_BLOCK();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens on controlled shutdown
    return std::make_unique<ClusterCommResult>();
  }

  // Later, we probably want to set these sensibly:
  ClientTransactionID const clientTransactionId = std::string("AQL");
  CoordTransactionID const coordTransactionId = TRI_NewTickServer();
  std::unordered_map<std::string, std::string> headers;
  if (!_ownName.empty()) {
    headers.emplace("Shard-Id", _ownName);
  }
    
  std::string url = std::string("/_db/") +
    arangodb::basics::StringUtils::urlEncode(_engine->getQuery()->trx()->vocbase().name()) + 
    urlPart + _queryId;

  ++_engine->_stats.requests;
  {
    JobGuard guard(SchedulerFeature::SCHEDULER);
    guard.block();

    return cc->syncRequest(clientTransactionId, coordTransactionId, _server, type,
                           std::move(url), body, headers, defaultTimeOut);
  }

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief initializeCursor, could be called multiple times
int RemoteBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  // For every call we simply forward via HTTP

  if (!_isResponsibleForInitializeCursor) {
    // do nothing...
    return TRI_ERROR_NO_ERROR;
  }
  
  if (items == nullptr) {
    // we simply ignore the initialCursor request, as the remote side
    // will initialize the cursor lazily
    return TRI_ERROR_NO_ERROR;
  } 

  VPackOptions options(VPackOptions::Defaults);
  options.buildUnindexedArrays = true;
  options.buildUnindexedObjects = true;

  VPackBuilder builder(&options);
  builder.openObject();
 
  builder.add("exhausted", VPackValue(false));
  builder.add("error", VPackValue(false));
  builder.add("pos", VPackValue(pos));
  builder.add(VPackValue("items"));
  builder.openObject();
  items->toVelocyPack(_engine->getQuery()->trx(), builder);
  builder.close();
  
  builder.close();

  std::string bodyString(builder.slice().toJson());

  std::unique_ptr<ClusterCommResult> res = sendRequest(
      rest::RequestType::PUT, "/_api/aql/initializeCursor/", bodyString);
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  {
    std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(
        responseBodyBuf.c_str(), responseBodyBuf.length());

    VPackSlice slice = builder->slice();
  
    if (slice.hasKey("code")) {
      return slice.get("code").getNumericValue<int>();
    }
    return TRI_ERROR_INTERNAL;
  }

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief shutdown, will be called exactly once for the whole query
int RemoteBlock::shutdown(int errorCode) {
  DEBUG_BEGIN_BLOCK();

  // For every call we simply forward via HTTP

  std::unique_ptr<ClusterCommResult> res =
      sendRequest(rest::RequestType::PUT, "/_api/aql/shutdown/",
                  std::string("{\"code\":" + std::to_string(errorCode) + "}"));
  try {
    if (throwExceptionAfterBadSyncRequest(res.get(), true)) {
      // artificially ignore error in case query was not found during shutdown
      return TRI_ERROR_NO_ERROR;
    }
  } catch (arangodb::basics::Exception const& ex) {
    if (ex.code() == TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE) {
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
    }
    throw;
  }

  StringBuffer const& responseBodyBuf(res->result->getBody());
  std::shared_ptr<VPackBuilder> builder =
      VPackParser::fromJson(responseBodyBuf.c_str(), responseBodyBuf.length());
  VPackSlice slice = builder->slice();
   
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
  }

  if (slice.hasKey("code")) {
    return slice.get("code").getNumericValue<int>();
  }
  return TRI_ERROR_INTERNAL;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getSome
AqlItemBlock* RemoteBlock::getSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  // For every call we simply forward via HTTP
  
  traceGetSomeBegin(atMost);
  VPackBuilder builder;
  builder.openObject();
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  std::string bodyString(builder.slice().toJson());

  std::unique_ptr<ClusterCommResult> res =
      sendRequest(rest::RequestType::PUT, "/_api/aql/getSome/", bodyString);
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  std::shared_ptr<VPackBuilder> responseBodyBuilder =
      res->result->getBodyVelocyPack();
  VPackSlice responseBody = responseBodyBuilder->slice();

  if (VelocyPackHelper::getBooleanValue(responseBody, "exhausted", true)) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  auto r = std::make_unique<AqlItemBlock>(_engine->getQuery()->resourceMonitor(), responseBody);
  traceGetSomeEnd(r.get());
  return r.release();

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief skipSome
size_t RemoteBlock::skipSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  // For every call we simply forward via HTTP

  VPackBuilder builder;
  builder.openObject();
  builder.add("atMost", VPackValue(atMost));
  builder.close();

  std::string bodyString(builder.slice().toJson());

  std::unique_ptr<ClusterCommResult> res =
      sendRequest(rest::RequestType::PUT, "/_api/aql/skipSome/", bodyString);
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  {
    std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(
        responseBodyBuf.c_str(), responseBodyBuf.length());
    VPackSlice slice = builder->slice();

    if (!slice.hasKey(StaticStrings::Error) || slice.get(StaticStrings::Error).getBoolean()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
    }
    size_t skipped = 0;
    if (slice.hasKey("skipped")) {
      skipped = slice.get("skipped").getNumericValue<size_t>();
    }
    return skipped;
  }

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief hasMore
bool RemoteBlock::hasMore() {
  DEBUG_BEGIN_BLOCK();
  // For every call we simply forward via HTTP
  std::unique_ptr<ClusterCommResult> res =
      sendRequest(rest::RequestType::GET, "/_api/aql/hasMore/", std::string());
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  std::shared_ptr<VPackBuilder> builder =
      VPackParser::fromJson(responseBodyBuf.c_str(), responseBodyBuf.length());
  VPackSlice slice = builder->slice();

  if (!slice.hasKey(StaticStrings::Error) || slice.get(StaticStrings::Error).getBoolean()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
  }
  bool hasMore = true;
  if (slice.hasKey("hasMore")) {
    hasMore = slice.get("hasMore").getBoolean();
  }
  return hasMore;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

// -----------------------------------------------------------------------------
// -- SECTION --                                            UnsortingGatherBlock
// -----------------------------------------------------------------------------

/// @brief shutdown: need our own method since our _buffer is different
int UnsortingGatherBlock::shutdown(int errorCode) {
  DEBUG_BEGIN_BLOCK();
  // don't call default shutdown method since it does the wrong thing to
  // _gatherBlockBuffer
  int ret = TRI_ERROR_NO_ERROR;
  for (auto* dependency : _dependencies) {
    int res = dependency->shutdown(errorCode);

    if (res != TRI_ERROR_NO_ERROR) {
      ret = res;
    }
  }

  return ret;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief initializeCursor
int UnsortingGatherBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _atDep = 0;
  _done = _dependencies.empty();

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief hasMore: true if any position of _buffer hasMore and false
/// otherwise.
bool UnsortingGatherBlock::hasMore() {
  DEBUG_BEGIN_BLOCK();
  if (_done || _dependencies.empty()) {
    return false;
  }

  for (auto* dependency : _dependencies) {
    if (dependency->hasMore()) {
      return true;
    }
  }

  _done = true;
  return false;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getSome
AqlItemBlock* UnsortingGatherBlock::getSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin(atMost);

  _done = _dependencies.empty();

  if (_done) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  // the simple case . . .
  auto* res = _dependencies[_atDep]->getSome(atMost);

  while (!res && _atDep < _dependencies.size() - 1) {
    _atDep++;
    res = _dependencies[_atDep]->getSome(atMost);
  }

  _done = (nullptr == res);

  traceGetSomeEnd(res);

  return res;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief skipSome
size_t UnsortingGatherBlock::skipSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();

  if (_done) {
    return 0;
  }

  // the simple case . . .
  auto skipped = _dependencies[_atDep]->skipSome(atMost);

  while (skipped == 0 && _atDep < _dependencies.size() - 1) {
    _atDep++;
    skipped = _dependencies[_atDep]->skipSome(atMost);
  }

  _done = (skipped == 0);

  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
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
    case GatherNode::SortMode::MinElement:
      _strategy = std::make_unique<HeapSorting>(
        _trx, _gatherBlockBuffer, _sortRegisters
      );
      break;
    case GatherNode::SortMode::Heap:
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

/// @brief shutdown: need our own method since our _buffer is different
int SortingGatherBlock::shutdown(int errorCode) {
  DEBUG_BEGIN_BLOCK();
  // don't call default shutdown method since it does the wrong thing to
  // _gatherBlockBuffer
  int ret = TRI_ERROR_NO_ERROR;
  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    int res = (*it)->shutdown(errorCode);

    if (res != TRI_ERROR_NO_ERROR) {
      ret = res;
    }
  }

  if (ret != TRI_ERROR_NO_ERROR) {
    return ret;
  }

  for (std::deque<AqlItemBlock*>& x : _gatherBlockBuffer) {
    for (AqlItemBlock* y : x) {
      delete y;
    }
    x.clear();
  }
  _gatherBlockBuffer.clear();
  _gatherBlockPos.clear();

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief initializeCursor
int SortingGatherBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  for (std::deque<AqlItemBlock*>& x : _gatherBlockBuffer) {
    for (AqlItemBlock* y : x) {
      delete y;
    }
    x.clear();
  }
  _gatherBlockBuffer.clear();
  _gatherBlockPos.clear();
  _gatherBlockBuffer.reserve(_dependencies.size());
  _gatherBlockPos.reserve(_dependencies.size());
  for (size_t i = 0; i < _dependencies.size(); i++) {
    _gatherBlockBuffer.emplace_back();
    _gatherBlockPos.emplace_back(i, 0);
  }

  _strategy->reset();

  _done = _dependencies.empty();
  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief hasMore: true if any position of _buffer hasMore and false
/// otherwise.
bool SortingGatherBlock::hasMore() {
  DEBUG_BEGIN_BLOCK();
  if (_done || _dependencies.empty()) {
    return false;
  }

  for (size_t i = 0; i < _gatherBlockBuffer.size(); i++) {
    if (!_gatherBlockBuffer[i].empty()) {
      return true;
    } else if (getBlock(i, DefaultBatchSize())) {
      _gatherBlockPos[i] = std::make_pair(i, 0);
      return true;
    }
  }

  _done = true;
  return false;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getSome
AqlItemBlock* SortingGatherBlock::getSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin(atMost);

  if (_dependencies.empty()) {
    _done = true;
  }

  if (_done) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  // the non-simple case . . .
  size_t available = 0;  // nr of available rows
  size_t index = 0;      // an index of a non-empty buffer

  // pull more blocks from dependencies . . .
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockBuffer.size() == _gatherBlockPos.size());

  for (size_t i = 0; i < _dependencies.size(); ++i) {
    if (_gatherBlockBuffer[i].empty()) {
      if (getBlock(i, atMost)) {
        index = i;
        _gatherBlockPos[i] = std::make_pair(i, 0);
      }
    } else {
      index = i;
    }

    auto const& cur = _gatherBlockBuffer[i];
    if (!cur.empty()) {
      TRI_ASSERT(cur[0]->size() >= _gatherBlockPos[i].second);
      available += cur[0]->size() - _gatherBlockPos[i].second;
      for (size_t j = 1; j < cur.size(); ++j) {
        available += cur[j]->size();
      }
    }
  }

  if (available == 0) {
    _done = true;
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  size_t toSend = (std::min)(available, atMost);  // nr rows in outgoing block

  // the following is similar to AqlItemBlock's slice method . . .
  std::vector<std::unordered_map<AqlValue, AqlValue>> cache;
  cache.resize(_gatherBlockBuffer.size());

  TRI_ASSERT(!_gatherBlockBuffer.at(index).empty());
  AqlItemBlock* example = _gatherBlockBuffer[index].front();
  size_t nrRegs = example->getNrRegs();

  // automatically deleted if things go wrong
  std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, static_cast<arangodb::aql::RegisterId>(nrRegs)));

  _strategy->prepare(_gatherBlockPos);

  for (size_t i = 0; i < toSend; i++) {
    // get the next smallest row from the buffer . . .
    auto const val = _strategy->nextValue();
    auto& blocks = _gatherBlockBuffer[val.first];
    auto& blocksPos = _gatherBlockPos[val.first];

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

    // renew the _gatherBlockPos and clean up the buffer if necessary
    if (++blocksPos.second == blocks.front()->size()) {
      TRI_ASSERT(!blocks.empty());
      AqlItemBlock* cur = blocks.front();
      returnBlock(cur);
      blocks.pop_front();
      blocksPos.second = 0; // reset position within a dependency

      if (blocks.empty()) {
        // if we pulled everything from the buffer, we need to fetch
        // more data for the shard for which we have no more local
        // values.
        getBlock(val.first, atMost);
        cache[val.first].clear();
        // note that if getBlock() returns false here, this is not
        // a problem, because the sort function used takes care of
        // this
      }
    }
  }

  traceGetSomeEnd(res.get());
  return res.release();

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief skipSome
size_t SortingGatherBlock::skipSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();

  if (_done) {
    return 0;
  }

  // the non-simple case . . .
  size_t available = 0;  // nr of available rows
  TRI_ASSERT(_dependencies.size() != 0);

  // pull more blocks from dependencies . . .
  for (size_t i = 0; i < _dependencies.size(); i++) {
    if (_gatherBlockBuffer[i].empty()) {
      if (getBlock(i, atMost)) {
        _gatherBlockPos[i] = std::make_pair(i, 0);
      }
    }

    auto cur = _gatherBlockBuffer[i];
    if (!cur.empty()) {
      available += cur[0]->size() - _gatherBlockPos[i].second;
      for (size_t j = 1; j < cur.size(); j++) {
        available += cur[j]->size();
      }
    }
  }

  if (available == 0) {
    _done = true;
    return 0;
  }

  size_t const skipped = (std::min)(available, atMost);  // nr rows in outgoing block

  _strategy->prepare(_gatherBlockPos);

  for (size_t i = 0; i < skipped; i++) {
    // get the next smallest row from the buffer . . .
    auto const val = _strategy->nextValue();
    auto& blocks = _gatherBlockBuffer[val.first];
    auto& blocksPos = _gatherBlockPos[val.first];

    // renew the _gatherBlockPos and clean up the buffer if necessary
    if (++blocksPos.second == blocks.front()->size()) {
      TRI_ASSERT(!blocks.empty());
      AqlItemBlock* cur = blocks.front();
      returnBlock(cur);
      blocks.pop_front();
      blocksPos.second = 0; // reset position within a dependency
    }
  }

  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
/// non-simple case only
bool SortingGatherBlock::getBlock(size_t i, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  TRI_ASSERT(i < _dependencies.size());

  std::unique_ptr<AqlItemBlock> docs(_dependencies[i]->getSome(atMost));
  if (docs && docs->size() > 0) {
    _gatherBlockBuffer[i].emplace_back(docs.get());
    docs.release();
    return true;
  }

  return false;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}
