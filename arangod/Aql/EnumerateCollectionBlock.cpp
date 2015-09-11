////////////////////////////////////////////////////////////////////////////////
/// @brief AQL EnumerateCollectionBlock
///
/// @file 
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/EnumerateCollectionBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/CollectionScanner.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                    class EnumerateCollectionBlock
// -----------------------------------------------------------------------------

EnumerateCollectionBlock::EnumerateCollectionBlock (ExecutionEngine* engine,
                                                    EnumerateCollectionNode const* ep)
  : ExecutionBlock(engine, ep),
    _collection(ep->_collection),
    _scanner(nullptr),
    _posInDocuments(0),
    _random(ep->_random),
    _mustStoreResult(true) {

  auto trxCollection = _trx->trxCollection(_collection->cid());
  if (trxCollection != nullptr) {
    _trx->orderDitch(trxCollection);
  }

  if (_random) {
    // random scan
    _scanner = new RandomCollectionScanner(_trx, trxCollection);
  }
  else {
    // default: linear scan
    _scanner = new LinearCollectionScanner(_trx, trxCollection);
  }
}

EnumerateCollectionBlock::~EnumerateCollectionBlock () {
  delete _scanner;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize fetching of documents
////////////////////////////////////////////////////////////////////////////////

void EnumerateCollectionBlock::initializeDocuments () {
  _scanner->reset();
  _documents.clear();
  _posInDocuments = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief continue fetching of documents
////////////////////////////////////////////////////////////////////////////////

bool EnumerateCollectionBlock::moreDocuments (size_t hint) {
  if (hint < DefaultBatchSize) {
    hint = DefaultBatchSize;
  }

  throwIfKilled(); // check if we were aborted

  TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  std::vector<TRI_doc_mptr_copy_t> newDocs;
  newDocs.reserve(hint);

  int res = _scanner->scan(newDocs, hint);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  if (newDocs.empty()) {
    return false;
  }

  _engine->_stats.scannedFull += static_cast<int64_t>(newDocs.size());

  _documents.swap(newDocs);
  _posInDocuments = 0;

  return true;
}

int EnumerateCollectionBlock::initialize () {
  auto ep = static_cast<EnumerateCollectionNode const*>(_exeNode);
  _mustStoreResult = ep->isVarUsedLater(ep->_outVariable);
  
  return ExecutionBlock::initialize();
}

int EnumerateCollectionBlock::initializeCursor (AqlItemBlock* items, 
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

AqlItemBlock* EnumerateCollectionBlock::getSome (size_t, // atLeast,
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
    if (! ExecutionBlock::getBlock(toFetch, toFetch)) {
      _done = true;
      return nullptr;
    }
    _pos = 0;           // this is in the first block
    initializeDocuments();
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  size_t const curRegs = cur->getNrRegs();

  // Get more documents from collection if _documents is empty:
  if (_posInDocuments >= _documents.size()) {
    if (! moreDocuments(atMost)) {
      _done = true;
      return nullptr;
    }
  }

  size_t available = _documents.size() - _posInDocuments;
  size_t toSend = (std::min)(atMost, available);
  RegisterId nrRegs = getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];

  std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrRegs));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());

  // only copy 1st row of registers inherited from previous frame(s)1
  inheritRegisters(cur, res.get(), _pos);

  // set our collection for our output register
  res->setDocumentCollection(static_cast<triagens::aql::RegisterId>(curRegs), _trx->documentCollection(_collection->cid()));

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
      res->setShaped(j, 
                     static_cast<triagens::aql::RegisterId>(curRegs),
                     reinterpret_cast<TRI_df_marker_t const*>(_documents[_posInDocuments].getDataPtr()));
      // No harm done, if the setValue throws!
    }

    ++_posInDocuments;
  }

  // Advance read position:
  if (_posInDocuments >= _documents.size()) {
    // we have exhausted our local documents buffer
    // fetch more documents into our buffer
    if (! moreDocuments(atMost)) {
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

size_t EnumerateCollectionBlock::skipSome (size_t atLeast, size_t atMost) {
  size_t skipped = 0;

  if (_done) {
    return skipped;
  }

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;           // this is in the first block
      initializeDocuments();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    // Get more documents from collection if _documents is empty:
    if (_posInDocuments >= _documents.size()) {
      if (! moreDocuments(atMost)) {
        _done = true;
        return skipped;
      }
    }

    if (atMost >= skipped + _documents.size() - _posInDocuments) {
      skipped += _documents.size() - _posInDocuments;

      // fetch more documents into our buffer
      if (! moreDocuments(atMost - skipped)) {
        // nothing more to read, re-initialize fetching of documents
        initializeDocuments();
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          delete cur;
          _pos = 0;
        }
      }
    }
    else {
      _posInDocuments += atMost - skipped;
      skipped = atMost;
    }
  }
  return skipped;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
