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
#include "StorageEngine/DocumentIdentifierToken.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

EnumerateCollectionBlock::EnumerateCollectionBlock(
    ExecutionEngine* engine, EnumerateCollectionNode const* ep)
    : ExecutionBlock(engine, ep), 
      DocumentProducingBlock(ep, _trx),
      _collection(ep->_collection),
      _mmdr(new ManagedDocumentResult),
      _cursor(
          _trx->indexScan(_collection->getName(),
                          (ep->_random ? transaction::Methods::CursorType::ANY
                                       : transaction::Methods::CursorType::ALL),
                          _mmdr.get(), 0, UINT64_MAX, 1000, false)) {
  TRI_ASSERT(_cursor->successful());
}

int EnumerateCollectionBlock::initialize() {
  DEBUG_BEGIN_BLOCK();

  if (_collection->isSatellite()) {
    auto logicalCollection = _collection->getCollection();
    auto cid = logicalCollection->planId();
    auto dbName = logicalCollection->dbName();

    double maxWait = _engine->getQuery()->queryOptions().satelliteSyncWait;
    bool inSync = false;
    unsigned long waitInterval = 10000;
    double startTime = TRI_microtime();
    double now = startTime;
    double endTime = startTime + maxWait;

    while (!inSync) {
      auto collectionInfoCurrent = ClusterInfo::instance()->getCollectionCurrent(
        dbName, std::to_string(cid));
      auto followers = collectionInfoCurrent->servers(_collection->getName());
      inSync = std::find(followers.begin(), followers.end(),
                         ServerState::instance()->getId()) != followers.end();
      if (!inSync) {
        if (endTime - now < waitInterval) {
          waitInterval = static_cast<unsigned long>(endTime - now);
        }
        usleep((TRI_usleep_t)waitInterval);
      }
      now = TRI_microtime();
      if (now > endTime) {
        break;
      }
    }

    if (!inSync) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
          "collection " + _collection->name + " did not come into sync in time (" + std::to_string(maxWait) +")");
    }
  }

  return ExecutionBlock::initialize();

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

int EnumerateCollectionBlock::initializeCursor(AqlItemBlock* items,
                                               size_t pos) {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  DEBUG_BEGIN_BLOCK();
  _cursor->reset();
  DEBUG_END_BLOCK();

  return TRI_ERROR_NO_ERROR;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getSome
AqlItemBlock* EnumerateCollectionBlock::getSome(size_t,  // atLeast,
                                                size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();

  TRI_ASSERT(_cursor.get() != nullptr);
  // Invariants:
  //   As soon as we notice that _totalCount == 0, we set _done = true.
  //   Otherwise, outside of this method (or skipSome), _documents is
  //   either empty (at the beginning, with _posInDocuments == 0)
  //   or is non-empty and _posInDocuments < _documents.size()
  if (_done) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }
    
  bool needMore;
  AqlItemBlock* cur = nullptr;
  size_t send = 0;
  std::unique_ptr<AqlItemBlock> res;
  do {
    do {
      needMore = false;

      if (_buffer.empty()) {
        size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
        if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
          _done = true;
          return nullptr;
        }
        _pos = 0;  // this is in the first block
        _cursor->reset();
      }

      // If we get here, we do have _buffer.front()
      cur = _buffer.front();

      if (!_cursor->hasMore()) {
        needMore = true;
        // we have exhausted this cursor
        // re-initialize fetching of documents
        _cursor->reset();
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        }
      }
    } while (needMore);

    TRI_ASSERT(cur != nullptr);
    TRI_ASSERT(_cursor->hasMore());

    size_t curRegs = cur->getNrRegs();

    RegisterId nrRegs =
        getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];

    res.reset(requestBlock(atMost, nrRegs));
    // automatically freed if we throw
    TRI_ASSERT(curRegs <= res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    throwIfKilled();  // check if we were aborted
    
    TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
   
    bool tmp;
    if (produceResult()) {
      // properly build up results by fetching the actual documents
      // using nextDocument()
      tmp = _cursor->nextDocument([&](DocumentIdentifierToken const&, VPackSlice slice) {
        _documentProducer(res.get(), slice, curRegs, send, 0);
      }, atMost);
    } else {
      // performance optimization: we do not need the documents at all,
      // so just call next()
      tmp = _cursor->next([&](DocumentIdentifierToken const&) {
        _documentProducer(res.get(), VPackSlice::nullSlice(), curRegs, send, 0);
      }, atMost);
    }
    if (!tmp) {
      TRI_ASSERT(!_cursor->hasMore());
    }

    // If the collection is actually empty we cannot forward an empty block
  } while (send == 0);
  _engine->_stats.scannedFull += static_cast<int64_t>(send);
  TRI_ASSERT(res != nullptr);

  if (send < atMost) {
    // The collection did not have enough results
    res->shrink(send, false);
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  traceGetSomeEnd(res.get());

  return res.release();

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

size_t EnumerateCollectionBlock::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = 0;
  TRI_ASSERT(_cursor != nullptr);

  if (_done) {
    return skipped;
  }

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;  // this is in the first block
      _cursor->reset();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();
    uint64_t skippedHere = 0;

    if (_cursor->hasMore()) {
      int res = _cursor->skip(atMost - skipped, skippedHere);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    skipped += skippedHere;

    if (skipped < atLeast) {
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

  _engine->_stats.scannedFull += static_cast<int64_t>(skipped);
  // We skipped atLeast documents
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}
