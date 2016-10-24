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
#include "Aql/CollectionScanner.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

EnumerateCollectionBlock::EnumerateCollectionBlock(
    ExecutionEngine* engine, EnumerateCollectionNode const* ep)
    : ExecutionBlock(engine, ep),
      _collection(ep->_collection),
      _mmdr(new ManagedDocumentResult(transaction())),
      _scanner(_trx, _mmdr.get(), _collection->getName(), ep->_random),
      _position(0),
      _mustStoreResult(true) {
}

/// @brief initialize fetching of documents
void EnumerateCollectionBlock::initializeDocuments() {
  _scanner.reset();
  _documents.clear();
  _position = 0;
}

/// @brief skip instead of fetching
bool EnumerateCollectionBlock::skipDocuments(size_t toSkip, size_t& skipped) {
  DEBUG_BEGIN_BLOCK();  
  throwIfKilled();  // check if we were aborted
  uint64_t skippedHere = 0;

  int res = _scanner.forward(toSkip, skippedHere);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  skipped += skippedHere;

  _documents.clear();
  _position = 0;

  _engine->_stats.scannedFull += static_cast<int64_t>(skippedHere);

  if (skippedHere < toSkip) {
    // We could not skip enough _scanner is exhausted
    return false;
  }
  // _scanner might have more elements
  return true;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

/// @brief continue fetching of documents
bool EnumerateCollectionBlock::moreDocuments(size_t hint) {
  DEBUG_BEGIN_BLOCK(); 
  // if (hint < DefaultBatchSize()) {
  //   hint = DefaultBatchSize();
  // }

  throwIfKilled();  // check if we were aborted

  TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _scanner.scan(_documents, hint);

  _position = 0;

  size_t count = _documents.size();

  if (count == 0) {
    return false;
  }

  _engine->_stats.scannedFull += static_cast<int64_t>(count);

  return true;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

int EnumerateCollectionBlock::initialize() {
  DEBUG_BEGIN_BLOCK();  
  auto ep = static_cast<EnumerateCollectionNode const*>(_exeNode);
  _mustStoreResult = ep->isVarUsedLater(ep->_outVariable);

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
  initializeDocuments();
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

  do {
    needMore = false;

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
        _done = true;
        return nullptr;
      }
      _pos = 0;  // this is in the first block
      initializeDocuments();
    }

    // If we get here, we do have _buffer.front()
    cur = _buffer.front();
    
    // Advance read position:
    if (_position >= _documents.size()) {
      // we have exhausted our local documents buffer
      // fetch more documents into our buffer
      if (!moreDocuments(atMost)) {
        // nothing more to read, re-initialize fetching of documents
        needMore = true;
        initializeDocuments();

        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        }
      }
    }
  } while (needMore);

  TRI_ASSERT(cur != nullptr);
  size_t curRegs = cur->getNrRegs();
  
  size_t available = static_cast<size_t>(_documents.size() - _position);
  size_t toSend = (std::min)(atMost, available);
  RegisterId nrRegs =
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];

  std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrRegs));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());

  // only copy 1st row of registers inherited from previous frame(s)
  inheritRegisters(cur, res.get(), _pos);
    
  auto col = _collection->getCollection();
  LogicalCollection* c = col.get();

  for (size_t j = 0; j < toSend; j++) {
    if (_mustStoreResult) {
      // The result is in the first variable of this depth,
      // we do not need to do a lookup in getPlanNode()->_registerPlan->varInfo,
      // but can just take cur->getNrRegs() as registerId:
      TRI_voc_rid_t revisionId = _documents[_position].revisionId();
      if (c->readRevision(_trx, *_mmdr, revisionId)) {
        uint8_t const* vpack = _mmdr->vpack(); 
        res->setValue(j, static_cast<arangodb::aql::RegisterId>(curRegs), AqlValue(vpack, AqlValueFromManagedDocument()));
      }
      // No harm done, if the setValue throws!
    }
    
    if (j > 0) {
      // re-use already copied AQLValues
      res->copyValuesFromFirstRow(j, static_cast<RegisterId>(curRegs));
    }

    ++_position;
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

  if (_done) {
    return skipped;
  }

  if (!_documents.empty()) {
    if (_position < _documents.size()) {
      // We still have unread documents in the _documents buffer
      // Just skip them
      size_t couldSkip = static_cast<size_t>(_documents.size() - _position);
      if (atMost <= couldSkip) {
        // More in buffer than to skip.

        // move forward the iterator
        _position += atMost;
        return atMost;
      }
      // Skip entire buffer
      _documents.clear();
      _position = 0;
      skipped += couldSkip;
    }
  }

  // No _documents buffer. But could Skip more
  // Fastforward the _scanner
  TRI_ASSERT(_documents.empty());

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;  // this is in the first block
      initializeDocuments();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    if (!skipDocuments(atMost - skipped, skipped)) {
      // nothing more to read, re-initialize fetching of documents
      initializeDocuments();
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        delete cur;
        _pos = 0;
      }
    }
  }
  // We skipped atLeast documents
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK(); 
}
