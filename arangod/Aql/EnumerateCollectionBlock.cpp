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
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

using Json = arangodb::basics::Json;

EnumerateCollectionBlock::EnumerateCollectionBlock(
    ExecutionEngine* engine, EnumerateCollectionNode const* ep)
    : ExecutionBlock(engine, ep),
      _collection(ep->_collection),
      _scanner(nullptr),
      _documentsSize(0),
      _posInDocuments(0),
      _random(ep->_random),
      _mustStoreResult(true) {
  _scanner = new CollectionScanner(_trx, _collection->getName(), _random);
}

EnumerateCollectionBlock::~EnumerateCollectionBlock() { delete _scanner; }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize fetching of documents
////////////////////////////////////////////////////////////////////////////////

void EnumerateCollectionBlock::initializeDocuments() {
  _scanner->reset();
  VPackSlice none;
  _documents = none;
  _posInDocuments = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skip instead of fetching
////////////////////////////////////////////////////////////////////////////////

bool EnumerateCollectionBlock::skipDocuments(size_t toSkip, size_t& skipped) {
  throwIfKilled();  // check if we were aborted
  uint64_t skippedHere = 0;

  int res = _scanner->forward(toSkip, skippedHere);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  skipped += skippedHere;

  VPackSlice none;
  _documents = none;
  _posInDocuments = 0;

  _engine->_stats.scannedFull += static_cast<int64_t>(skippedHere);

  if (skippedHere < toSkip) {
    // We could not skip enough _scanner is exhausted
    return false;
  }
  // _scanner might have more elements
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief continue fetching of documents
////////////////////////////////////////////////////////////////////////////////

bool EnumerateCollectionBlock::moreDocuments(size_t hint) {
  if (hint < DefaultBatchSize) {
    hint = DefaultBatchSize;
  }

  throwIfKilled();  // check if we were aborted

  TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _documents = _scanner->scan(hint);
  TRI_ASSERT(_documents.isArray());
  VPackValueLength count = _documents.length();

  if (count == 0) {
    VPackSlice none;
    _documents = none;
    return false;
  }

  _engine->_stats.scannedFull += static_cast<int64_t>(count);

  _documentsSize = static_cast<size_t>(count);
  _posInDocuments = 0;

  return true;
}

int EnumerateCollectionBlock::initialize() {
  auto ep = static_cast<EnumerateCollectionNode const*>(_exeNode);
  _mustStoreResult = ep->isVarUsedLater(ep->_outVariable);

  return ExecutionBlock::initialize();
}

int EnumerateCollectionBlock::initializeCursor(AqlItemBlock* items,
                                               size_t pos) {
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  initializeDocuments();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* EnumerateCollectionBlock::getSome(size_t,  // atLeast,
                                                size_t atMost) {
  // Invariants:
  //   As soon as we notice that _totalCount == 0, we set _done = true.
  //   Otherwise, outside of this method (or skipSome), _documents is
  //   either empty (at the beginning, with _posInDocuments == 0)
  //   or is non-empty and _posInDocuments < _documents.size()
  if (_done) {
    return nullptr;
  }

  if (_buffer.empty()) {
    size_t toFetch = (std::min)(DefaultBatchSize, atMost);
    if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
      _done = true;
      return nullptr;
    }
    _pos = 0;  // this is in the first block
    initializeDocuments();
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  size_t const curRegs = cur->getNrRegs();

  // Get more documents from collection if _documents is empty:
  if (_posInDocuments >= _documentsSize) {
    if (!moreDocuments(atMost)) {
      _done = true;
      return nullptr;
    }
  }

  size_t available = _documentsSize - _posInDocuments;
  size_t toSend = (std::min)(atMost, available);
  RegisterId nrRegs =
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];

  std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrRegs));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());

  // only copy 1st row of registers inherited from previous frame(s)1
  inheritRegisters(cur, res.get(), _pos);

  for (size_t j = 0; j < toSend; j++) {
    if (j > 0) {
      // re-use already copied aqlvalues
      for (RegisterId i = 0; i < curRegs; i++) {
        res->setValue(j, i, res->getValueReference(0, i));
        // Note: if this throws, then all values will be deleted
        // properly since the first one is.
      }
    }

    if (_mustStoreResult) {
      // The result is in the first variable of this depth,
      // we do not need to do a lookup in getPlanNode()->_registerPlan->varInfo,
      // but can just take cur->getNrRegs() as registerId:
      res->setValue(j, static_cast<arangodb::aql::RegisterId>(curRegs),
                    AqlValue$(_documents.at(_posInDocuments)));
      // No harm done, if the setValue throws!
    }

    ++_posInDocuments;
  }

  // Advance read position:
  if (_posInDocuments >= _documentsSize) {
    // we have exhausted our local documents buffer
    // fetch more documents into our buffer
    if (!moreDocuments(atMost)) {
      // nothing more to read, re-initialize fetching of documents
      initializeDocuments();

      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      }
    }
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  return res.release();
}

size_t EnumerateCollectionBlock::skipSome(size_t atLeast, size_t atMost) {
  size_t skipped = 0;

  if (_done) {
    return skipped;
  }

  if (!_documents.isNone()) {
    if (_posInDocuments < _documentsSize) {
      // We still have unread documents in the _documents buffer
      // Just skip them
      size_t couldSkip = _documentsSize - _posInDocuments;
      if (atMost <= couldSkip) {
        // More in buffer then to skip.
        _posInDocuments += atMost;
        return atMost;
      }
      // Skip entire buffer
      VPackSlice none;
      _documents = none;
      _posInDocuments = 0;
      skipped += couldSkip;
    }
  }

  // No _documents buffer. But could Skip more
  // Fastforward the _scanner
  TRI_ASSERT(_documents.isNone());

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
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
}
