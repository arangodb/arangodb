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

#include "EnumerateCollectionBlock.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ServerState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

EnumerateCollectionBlock::EnumerateCollectionBlock(
    ExecutionEngine* engine, EnumerateCollectionNode const* ep)
    : ExecutionBlock(engine, ep), 
      DocumentProducingBlock(ep, _trx),
      _collection(ep->collection()),
      _cursor(
          _trx->indexScan(_collection->name(),
                          (ep->_random ? transaction::Methods::CursorType::ANY
                                       : transaction::Methods::CursorType::ALL))),
      _inflight(0) {
  TRI_ASSERT(_cursor->ok());

  buildCallback();

  if (ServerState::instance()->isRunningInCluster() && _collection->isSatellite()) {
    auto logicalCollection = _collection->getCollection();
    auto cid = logicalCollection->planId();
    auto& dbName = logicalCollection->vocbase().name();
    double maxWait = _engine->getQuery()->queryOptions().satelliteSyncWait;
    bool inSync = false;
    unsigned long waitInterval = 10000;
    double startTime = TRI_microtime();
    double now = startTime;
    double endTime = startTime + maxWait;

    while (!inSync) {
      auto collectionInfoCurrent = ClusterInfo::instance()->getCollectionCurrent(
        dbName, std::to_string(cid));
      auto followers = collectionInfoCurrent->servers(_collection->name());
      inSync = std::find(followers.begin(), followers.end(),
                         ServerState::instance()->getId()) != followers.end();
      if (!inSync) {
        if (endTime - now < waitInterval) {
          waitInterval = static_cast<unsigned long>(endTime - now);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(waitInterval));
      }
      now = TRI_microtime();
      if (now > endTime) {
        break;
      }
    }

    if (!inSync) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
          "collection " + _collection->name() + " did not come into sync in time (" + std::to_string(maxWait) +")");
    }
  }
}

std::pair<ExecutionState, arangodb::Result> EnumerateCollectionBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  _cursor->reset();

  return res;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
EnumerateCollectionBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  TRI_ASSERT(_cursor != nullptr);
  // Invariants:
  //   As soon as we notice that _totalCount == 0, we set _done = true.
  //   Otherwise, outside of this method (or skipSome), _documents is
  //   either empty (at the beginning, with _posInDocuments == 0)
  //   or is non-empty and _posInDocuments < _documents.size()
  if (_done) {
    TRI_ASSERT(_inflight == 0);
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  RegisterId const nrInRegs = getNrInputRegisters();
  RegisterId const nrOutRegs = getNrOutputRegisters();

  std::unique_ptr<AqlItemBlock> res;

  do {
    size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
    BufferState bufferState = getBlockIfNeeded(toFetch);
    if (bufferState == BufferState::WAITING) {
      return {ExecutionState::WAITING, nullptr};
    }
    if (bufferState == BufferState::NO_MORE_BLOCKS) {
      TRI_ASSERT(_inflight == 0);
      _done = true;
      TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
      traceGetSomeEnd(nullptr, ExecutionState::DONE);
      return {ExecutionState::DONE, nullptr};
    }

    // If we get here, we do have _buffer.front()
    AqlItemBlock* cur = _buffer.front();

    TRI_ASSERT(cur != nullptr);
    TRI_ASSERT(cur->getNrRegs() == nrInRegs);

    // _cursor->hasMore() should only be false here if the collection is empty
    if (_cursor->hasMore()) {
      res.reset(requestBlock(atMost, nrOutRegs));
      // automatically freed if we throw
      TRI_ASSERT(nrInRegs <= res->getNrRegs());

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(cur, res.get(), _pos);

      throwIfKilled();  // check if we were aborted

      TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      bool cursorHasMore;
      if (produceResult()) {
        // properly build up results by fetching the actual documents
        // using nextDocument()
        cursorHasMore = _cursor->nextDocument(
          [&](LocalDocumentId const &, VPackSlice slice) {
            _documentProducer(res.get(), slice, nrInRegs, _inflight, 0);
          }, atMost
        );
      } else {
        // performance optimization: we do not need the documents at all,
        // so just call next()
        cursorHasMore = _cursor->next(
          [&](LocalDocumentId const &) {
            _documentProducer(
              res.get(), VPackSlice::nullSlice(), nrInRegs, _inflight, 0
            );
          }, atMost
        );
      }
      if (!cursorHasMore) {
        TRI_ASSERT(!_cursor->hasMore());
      }
    }

    if (!_cursor->hasMore()) {
      // we have exhausted this cursor
      // re-initialize fetching of documents
      _cursor->reset();
      AqlItemBlock* removedBlock = advanceCursor(1, 0);
      returnBlockUnlessNull(removedBlock);
    }
  } while (_inflight == 0);

  _engine->_stats.scannedFull += static_cast<int64_t>(_inflight);
  TRI_ASSERT(res != nullptr);

  if (_inflight < atMost) {
    // The collection did not have enough results
    res->shrink(_inflight);
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  _inflight = 0;
  traceGetSomeEnd(res.get(), getHasMoreState());
  return {getHasMoreState(), std::move(res)};
}

std::pair<ExecutionState, size_t> EnumerateCollectionBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    TRI_ASSERT(_inflight == 0);
    traceSkipSomeEnd(_inflight, ExecutionState::DONE);
    return {ExecutionState::DONE, _inflight};
  }

  TRI_ASSERT(_cursor != nullptr);

  while (_inflight < atMost) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost - _inflight);
      auto upstreamRes = getBlock(toFetch);
      if (upstreamRes.first == ExecutionState::WAITING) {
        traceSkipSomeEnd(0, ExecutionState::WAITING);
        return {ExecutionState::WAITING, 0};
      }
      _upstreamState = upstreamRes.first;
      if (!upstreamRes.second) {
        _done = true;
        size_t skipped = _inflight;
        _inflight = 0;
        traceSkipSomeEnd(skipped, ExecutionState::DONE);
        return {ExecutionState::DONE, skipped};
      }
      _pos = 0;  // this is in the first block
      _cursor->reset();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();
    uint64_t skippedHere = 0;

    if (_cursor->hasMore()) {
      _cursor->skip(atMost - _inflight, skippedHere);
    }

    _inflight += skippedHere;

    if (_inflight < atMost) {
      TRI_ASSERT(!_cursor->hasMore());
      // not skipped enough re-initialize fetching of documents
      _cursor->reset();
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      }
    }
  }

  _engine->_stats.scannedFull += static_cast<int64_t>(_inflight);
  size_t skipped = _inflight;
  _inflight = 0;
  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}
