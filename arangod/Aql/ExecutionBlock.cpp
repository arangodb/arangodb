////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionBlocks, the execution engine
///
/// @file arangod/Aql/ExecutionBlock.cpp
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

#include "Aql/ExecutionBlock.h"
#include "Aql/CollectionScanner.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/json-utilities.h"
#include "Basics/Exceptions.h"
#include "Dispatcher/DispatcherThread.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/HashIndex.h"
#include "Indexes/SkiplistIndex2.h"
#include "V8/v8-globals.h"
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;
using StringBuffer = triagens::basics::StringBuffer;

// uncomment the following to get some debugging information
#if 0
#define ENTER_BLOCK try { (void) 0;
#define LEAVE_BLOCK } catch (...) { std::cout << "caught an exception in " << __FUNCTION__ << ", " << __FILE__ << ":" << __LINE__ << "!\n"; throw; }
#else
#define ENTER_BLOCK
#define LEAVE_BLOCK
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                            struct AggregatorGroup
// -----------------------------------------------------------------------------
      
AggregatorGroup::AggregatorGroup (bool count)
  : firstRow(0),
    lastRow(0),
    groupLength(0),
    rowsAreValid(false),
    count(count) {
}

AggregatorGroup::~AggregatorGroup () {
  //reset();
  for (auto& it : groupBlocks) {
    delete it;
  }
  for (auto& it : groupValues) {
    it.destroy();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void AggregatorGroup::initialize (size_t capacity) {
  // TRI_ASSERT(capacity > 0);

  groupValues.clear();
  collections.clear();

  if (capacity > 0) {
    groupValues.reserve(capacity);
    collections.reserve(capacity);

    for (size_t i = 0; i < capacity; ++i) {
      groupValues.emplace_back();
      collections.emplace_back(nullptr);
    }
  }

  groupLength = 0;
}

void AggregatorGroup::reset () {
  for (auto& it : groupBlocks) {
    delete it;
  }

  groupBlocks.clear();

  if (! groupValues.empty()) {
    for (auto& it : groupValues) {
      it.destroy();
    }
    groupValues[0].erase();   // only need to erase [0], because we have
                              // only copies of references anyway
  }

  groupLength = 0;
}

void AggregatorGroup::addValues (AqlItemBlock const* src,
                                 RegisterId groupRegister) {
  if (groupRegister == ExecutionNode::MaxRegisterId) {
    // nothing to do
    return;
  }

  if (rowsAreValid) {
    // emit group details
    TRI_ASSERT(firstRow <= lastRow);

    if (count) {
      groupLength += lastRow + 1 - firstRow;
    }
    else {
      auto block = src->slice(firstRow, lastRow + 1);
      try {
        TRI_IF_FAILURE("AggregatorGroup::addValues") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        groupBlocks.emplace_back(block);
      }
      catch (...) {
        delete block;
        throw;
      }
    }
  }

  firstRow = lastRow = 0;
  // the next statement ensures we don't add the same value (row) twice
  rowsAreValid = false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class ExecutionBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size value
////////////////////////////////////////////////////////////////////////////////

size_t const ExecutionBlock::DefaultBatchSize = 1000;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ExecutionBlock::ExecutionBlock (ExecutionEngine* engine,
                                ExecutionNode const* ep)
  : _engine(engine),
    _trx(engine->getQuery()->trx()), 
    _exeNode(ep), 
    _done(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ExecutionBlock::~ExecutionBlock () {
  for (auto& it : _buffer) {
    delete it;
  }

  _buffer.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the number of rows in a vector of blocks
////////////////////////////////////////////////////////////////////////////////

size_t ExecutionBlock::countBlocksRows (std::vector<AqlItemBlock*> const& blocks) const {
  size_t count = 0;
  for (auto const& it : blocks) {
    count += it->size();
  }
  return count;
}

bool ExecutionBlock::removeDependency (ExecutionBlock* ep) {
  auto it = _dependencies.begin();
  while (it != _dependencies.end()) {
    if (*it == ep) {
      _dependencies.erase(it);
      return true;
    }
    ++it;
  }
  return false;
}

int ExecutionBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  for (auto& d : _dependencies) {
    int res = d->initializeCursor(items, pos);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  for (auto& it : _buffer) {
    delete it;
  }
  _buffer.clear();

  _done = false;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was killed
////////////////////////////////////////////////////////////////////////////////

bool ExecutionBlock::isKilled () const {
  return _engine->getQuery()->killed();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was killed
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::throwIfKilled () {
  if (isKilled()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to walk an execution block recursively
////////////////////////////////////////////////////////////////////////////////

bool ExecutionBlock::walk (WalkerWorker<ExecutionBlock>* worker) {
  // Only do every node exactly once:
  if (worker->done(this)) {
    return false;
  }

  if (worker->before(this)) {
    return true;
  }

  // Now the children in their natural order:
  for (auto& c : _dependencies) {
    if (c->walk(worker)) {
      return true;
    }
  }
  // Now handle a subquery:
  if (_exeNode->getType() == ExecutionNode::SUBQUERY) {
    auto p = static_cast<SubqueryBlock*>(this);
    if (worker->enterSubquery(this, p->getSubquery())) {
      bool abort = p->getSubquery()->walk(worker);
      worker->leaveSubquery(this, p->getSubquery());
      if (abort) {
        return true;
      }
    }
  }
  worker->after(this);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int ExecutionBlock::initialize () {
  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    int res = (*it)->initialize();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, will be called exactly once for the whole query
////////////////////////////////////////////////////////////////////////////////

int ExecutionBlock::shutdown (int errorCode) {
  int ret = TRI_ERROR_NO_ERROR;

  for (auto& it : _buffer) {
    delete it;
  }
  _buffer.clear();

  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    int res;
    try {
      res = (*it)->shutdown(errorCode);
    }
    catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      ret = res;
    }
  }

  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome, gets some more items, semantic is as follows: not
/// more than atMost items may be delivered. The method tries to
/// return a block of at least atLeast items, however, it may return
/// less (for example if there are not enough items to come). However,
/// if it returns an actual block, it must contain at least one item.
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ExecutionBlock::getSome (size_t atLeast, size_t atMost) {
  std::unique_ptr<AqlItemBlock> result(getSomeWithoutRegisterClearout(atLeast, atMost));
  clearRegisters(result.get());
  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief request an AqlItemBlock from the memory manager
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ExecutionBlock::requestBlock (size_t nrItems, 
                                            RegisterId nrRegs) {
  return _engine->_itemBlockManager.requestBlock(nrItems, nrRegs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an AqlItemBlock to the memory manager
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::returnBlock (AqlItemBlock*& block) {
  _engine->_itemBlockManager.returnBlock(block);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resolve a collection name and return cid and document key
/// this is used for parsing _from, _to and _id values
////////////////////////////////////////////////////////////////////////////////
  
int ExecutionBlock::resolve (char const* handle, 
                             TRI_voc_cid_t& cid, 
                             std::string& key) const {
  char const* p = strchr(handle, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR);
  if (p == nullptr || *p == '\0') {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }
  
  if (*handle >= '0' && *handle <= '9') {
    cid = triagens::basics::StringUtils::uint64(handle, p - handle);
  }
  else {
    std::string const name(handle, p - handle);
    cid = _trx->resolver()->getCollectionIdCluster(name);
  }
                              
  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  key = std::string(p + 1);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::inheritRegisters (AqlItemBlock const* src,
                                       AqlItemBlock* dst,
                                       size_t srcRow,
                                       size_t dstRow) {
  RegisterId const n = src->getNrRegs();
  auto planNode = getPlanNode();

  for (RegisterId i = 0; i < n; i++) {
    if (planNode->_regsToClear.find(i) == planNode->_regsToClear.end()) {
      auto value = src->getValueReference(srcRow, i);

      if (! value.isEmpty()) {
        AqlValue a = value.clone();
        try {
          dst->setValue(dstRow, i, a);
        }
        catch (...) {
          a.destroy();
          throw;
        }
      }
      // copy collection
      dst->setDocumentCollection(i, src->getDocumentCollection(i));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::inheritRegisters (AqlItemBlock const* src,
                                       AqlItemBlock* dst,
                                       size_t row) {
  RegisterId const n = src->getNrRegs();
  auto planNode = getPlanNode();

  for (RegisterId i = 0; i < n; i++) {
    if (planNode->_regsToClear.find(i) == planNode->_regsToClear.end()) {
      auto value = src->getValueReference(row, i);

      if (! value.isEmpty()) {
        AqlValue a = value.clone();

        try {
          TRI_IF_FAILURE("ExecutionBlock::inheritRegisters") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          dst->setValue(0, i, a);
        }
        catch (...) {
          a.destroy();
          throw;
        }
      }
      // copy collection
      dst->setDocumentCollection(i, src->getDocumentCollection(i));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the following is internal to pull one more block and append it to
/// our _buffer deque. Returns true if a new block was appended and false if
/// the dependent node is exhausted.
////////////////////////////////////////////////////////////////////////////////

bool ExecutionBlock::getBlock (size_t atLeast, size_t atMost) {
  throwIfKilled(); // check if we were aborted

  std::unique_ptr<AqlItemBlock> docs(_dependencies[0]->getSome(atLeast, atMost));

  if (docs == nullptr) {
    return false;
  }

  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _buffer.emplace_back(docs.get());
  docs.release();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSomeWithoutRegisterClearout, same as above, however, this
/// is the actual worker which does not clear out registers at the end
/// the idea is that somebody who wants to call the generic functionality
/// in a derived class but wants to modify the results before the register
/// cleanup can use this method, internal use only
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ExecutionBlock::getSomeWithoutRegisterClearout (size_t atLeast, 
                                                              size_t atMost) {
  TRI_ASSERT(0 < atLeast && atLeast <= atMost);
  size_t skipped = 0;

  AqlItemBlock* result = nullptr;
  int out = getOrSkipSome(atLeast, atMost, false, result, skipped);

  if (out != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(out);
  }

  return result;
}

void ExecutionBlock::clearRegisters (AqlItemBlock* result) {
  // Clear out registers not needed later on:
  if (result != nullptr) {
    result->clearRegisters(getPlanNode()->_regsToClear);
  }
}

size_t ExecutionBlock::skipSome (size_t atLeast, size_t atMost) {
  TRI_ASSERT(0 < atLeast && atLeast <= atMost);
  size_t skipped = 0;

  AqlItemBlock* result = nullptr;
  int out = getOrSkipSome(atLeast, atMost, true, result, skipped);

  TRI_ASSERT(result == nullptr);

  if (out != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(out);
  }

  return skipped;
}

// skip exactly <number> outputs, returns <true> if _done after
// skipping, and <false> otherwise . . .
bool ExecutionBlock::skip (size_t number) {
  size_t skipped = skipSome(number, number);
  size_t nr = skipped;
  while (nr != 0 && skipped < number) {
    nr = skipSome(number - skipped, number - skipped);
    skipped += nr;
  }
  if (nr == 0) {
    return true;
  }
  return ! hasMore();
}

bool ExecutionBlock::hasMore () {
  if (_done) {
    return false;
  }
  if (! _buffer.empty()) {
    return true;
  }
  if (getBlock(DefaultBatchSize, DefaultBatchSize)) {
    _pos = 0;
    return true;
  }
  _done = true;
  return false;
}

int64_t ExecutionBlock::remaining () {
  int64_t sum = 0;
  for (auto const& it : _buffer) {
    sum += it->size();
  }
  return sum + _dependencies[0]->remaining();
}

int ExecutionBlock::getOrSkipSome (size_t atLeast,
                                   size_t atMost,
                                   bool skipping,
                                   AqlItemBlock*& result,
                                   size_t& skipped) {
  
  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  // if _buffer.size() is > 0 then _pos points to a valid place . . .
  std::vector<AqlItemBlock*> collector;

  auto freeCollector = [&collector]() {
    for (auto& x : collector) {
      delete x;
    }
    collector.clear();
  };

  try {
    while (skipped < atLeast) {
      if (_buffer.empty()) {
        if (skipping) {
          _dependencies[0]->skip(atLeast - skipped);
          skipped = atLeast;
          freeCollector();
          return TRI_ERROR_NO_ERROR;
        }
        else {
          if (! getBlock(atLeast - skipped, atMost - skipped)) {
            _done = true;
            break; // must still put things in the result from the collector . . .
          }
          _pos = 0;
        }
      }

      AqlItemBlock* cur = _buffer.front();

      if (cur->size() - _pos > atMost - skipped) {
        // The current block is too large for atMost:
        if (! skipping) { 
          std::unique_ptr<AqlItemBlock> more(cur->slice(_pos, _pos + (atMost - skipped)));

          TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          collector.emplace_back(more.get());
          more.release(); // do not delete it!
        }
        _pos += atMost - skipped;
        skipped = atMost;
      }
      else if (_pos > 0) {
        // The current block fits into our result, but it is already
        // half-eaten:
        if (! skipping) {
          std::unique_ptr<AqlItemBlock> more(cur->slice(_pos, cur->size()));

          TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          collector.emplace_back(more.get());
          more.release();
        }
        skipped += cur->size() - _pos;
        delete cur;
        _buffer.pop_front();
        _pos = 0;
      }
      else {
        // The current block fits into our result and is fresh:
        skipped += cur->size();
        if (! skipping) {
          // if any of the following statements throw, then cur is not lost,
          // as it is still contained in _buffer
          TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          collector.emplace_back(cur);
        }
        else {
          delete cur;
        }
        _buffer.pop_front();
        _pos = 0;
      }
    }
  }
  catch (...) {
    freeCollector();
    throw;
  }

  TRI_ASSERT(result == nullptr);

  if (! skipping) {
    if (collector.size() == 1) {
      result = collector[0];
      collector.clear();
    }
    else if (! collector.empty()) {
      try {
        TRI_IF_FAILURE("ExecutionBlock::getOrSkipSomeConcatenate") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        result = AqlItemBlock::concatenate(collector);
      }
      catch (...) {
        freeCollector();
        throw;
      }
    }
  }

  freeCollector();
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class SingletonBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor, store a copy of the register values coming from above
////////////////////////////////////////////////////////////////////////////////

int SingletonBlock::initializeCursor (AqlItemBlock* items, 
                                      size_t pos) {
  // Create a deep copy of the register values given to us:
  deleteInputVariables();
  
  if (items != nullptr) {
    auto en = static_cast<SingletonNode const*>(getPlanNode());
    auto const& registerPlan = en->getRegisterPlan()->varInfo;
    std::unordered_set<Variable const*> const& varsUsedLater = en->getVarsUsedLater();
    
    // build a whitelist with all the registers that we will copy from above
    std::unordered_set<RegisterId> whitelist;

    for (auto const& it : varsUsedLater) {
      auto it2 = registerPlan.find(it->id);

      if (it2 != registerPlan.end()) {
        whitelist.emplace((*it2).second.registerId);
      }
    }

    _inputRegisterValues = items->slice(pos, whitelist);
  }

  _done = false;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the singleton block
////////////////////////////////////////////////////////////////////////////////

int SingletonBlock::shutdown (int errorCode) {
  int res = ExecutionBlock::shutdown(errorCode);

  deleteInputVariables();

  return res;
}

int SingletonBlock::getOrSkipSome (size_t,   // atLeast,
                                   size_t atMost,
                                   bool skipping,
                                   AqlItemBlock*& result,
                                   size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  if (! skipping) {
    result = new AqlItemBlock(1, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]);

    try {
      if (_inputRegisterValues != nullptr) {
        skipped++;
        for (RegisterId reg = 0; reg < _inputRegisterValues->getNrRegs(); ++reg) {

          TRI_IF_FAILURE("SingletonBlock::getOrSkipSome") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          AqlValue a = _inputRegisterValues->getValue(0, reg);
          _inputRegisterValues->steal(a);

          try {
            TRI_IF_FAILURE("SingletonBlock::getOrSkipSomeSet") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }

            result->setValue(0, reg, a);
          }
          catch (...) {
            a.destroy();
            throw;
          }
          _inputRegisterValues->eraseValue(0, reg);
          // if the latter throws, it does not matter, since we have
          // already stolen the value
          result->setDocumentCollection(reg,
                                        _inputRegisterValues->getDocumentCollection(reg));

        }
      }
    }
    catch (...) {
      delete result;
      result = nullptr;
      throw;
    }
  }
  else {
    if (_inputRegisterValues != nullptr) {
      skipped++;
    }
  }

  _done = true;
  return TRI_ERROR_NO_ERROR;
}

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

// -----------------------------------------------------------------------------
// --SECTION--                                             class IndexRangeBlock
// -----------------------------------------------------------------------------

IndexRangeBlock::IndexRangeBlock (ExecutionEngine* engine,
                                  IndexRangeNode const* en)
  : ExecutionBlock(engine, en),
    _collection(en->collection()),
    _posInDocs(0),
    _anyBoundVariable(false),
    _skiplistIterator(nullptr),
    _edgeIndexIterator(nullptr),
    _hashIndexSearchValue({ 0, nullptr }),
    _hashNextElement(nullptr),
    _condition(new IndexOrCondition()),
    _posInRanges(0),
    _sortCoords(),
    _freeCondition(true),
    _hasV8Expression(false) {

  auto trxCollection = _trx->trxCollection(_collection->cid());

  if (trxCollection != nullptr) {
    _trx->orderDitch(trxCollection);
  }
    
  std::vector<std::vector<RangeInfo>> const& orRanges = en->_ranges;
  size_t const n = orRanges.size(); 

  for (size_t i = 0; i < n; i++) {
    _condition->emplace_back(IndexAndCondition());

    for (auto const& ri : en->_ranges[i]) {
      _condition->at(i).emplace_back(ri.clone());
    }
  }

  if (_condition->size() > 1) {
    removeOverlapsIndexOr(*_condition);
  }

  TRI_ASSERT(en->_index != nullptr);

  _allBoundsConstant.clear();
  _allBoundsConstant.reserve(orRanges.size());

  // Detect, whether all ranges are constant:
  for (size_t i = 0; i < orRanges.size(); i++) {
    bool isConstant = true;

    std::vector<RangeInfo> const& attrRanges = orRanges[i];
    for (auto const& r : attrRanges) {
      isConstant &= r.isConstant();
    }
    _anyBoundVariable |= ! isConstant;
    _allBoundsConstant.push_back(isConstant); // note: emplace_back() is not supported in C++11 but only from C++14
  }
}

IndexRangeBlock::~IndexRangeBlock () {
  destroyHashIndexSearchValues();

  for (auto& e : _allVariableBoundExpressions) {
    delete e;
  }

  if (_freeCondition && _condition != nullptr) {
    delete _condition;
  }
    
  if (_skiplistIterator != nullptr) {
    TRI_FreeSkiplistIterator(_skiplistIterator);
  }
 
  delete _edgeIndexIterator; 
}

bool IndexRangeBlock::useHighBounds () const {
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  return (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX);
}

bool IndexRangeBlock::hasV8Expression () const {
  for (auto const& expression : _allVariableBoundExpressions) {
    TRI_ASSERT(expression != nullptr);

    if (expression->isV8()) {
      return true;
    }
  }
  return false;
}

void IndexRangeBlock::buildExpressions () {
  bool const useHighBounds = this->useHighBounds(); 

  size_t posInExpressions = 0;
    
  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  AqlItemBlock* cur = _buffer.front();
        
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  std::unique_ptr<IndexOrCondition> newCondition;

  for (size_t i = 0; i < en->_ranges.size(); i++) {
    size_t const n = en->_ranges[i].size(); 
    // prefill with n default-constructed vectors
    std::vector<std::vector<RangeInfo>> collector(n);   

    // collect the evaluated bounds here
    for (size_t k = 0; k < n; k++) {
      auto const& r = en->_ranges[i][k];

      {
        // First create a new RangeInfo containing only the constant 
        // low and high bound of r:
        RangeInfo riConst(r._var, r._attr, r._lowConst, r._highConst, r.is1ValueRangeInfo());
        collector[k].emplace_back(std::move(riConst));
      }

      // Now work the actual values of the variable lows and highs into 
      // this constant range:
      for (auto const& l : r._lows) {
        Expression* e = _allVariableBoundExpressions[posInExpressions];
        TRI_ASSERT(e != nullptr);
        TRI_document_collection_t const* myCollection = nullptr; 
        AqlValue a = e->execute(_trx, cur, _pos, _inVars[posInExpressions], _inRegs[posInExpressions], &myCollection);
        posInExpressions++;

        Json bound;
        if (a._type == AqlValue::JSON) {
          bound = *(a._json);
          a.destroy();  // the TRI_json_t* of a._json has been stolen
        } 
        else if (a._type == AqlValue::SHAPED || a._type == AqlValue::DOCVEC) {
          bound = a.toJson(_trx, myCollection, true);
          a.destroy();  // the TRI_json_t* of a._json has been stolen
        } 
        else {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, 
              "AQL: computed a variable bound and got non-JSON");
        }

        if (! bound.isArray()) {
          if (useHighBounds) {
            auto b(bound.copy());

            RangeInfo ri(r._var, 
                         r._attr, 
                         RangeInfoBound(l.inclusive(), true, b), // will steal b's JSON
                         RangeInfoBound(), 
                         false);

            for (size_t j = 0; j < collector[k].size(); j++) {
              collector[k][j].fuse(ri);
            }
          }
          else {
            auto b1(bound.copy()); // first instance of bound
            auto b2(bound.copy()); // second instance of same bound

            RangeInfo ri(r._var, 
                         r._attr, 
                         RangeInfoBound(l.inclusive(), true, b1), // will steal b1's JSON
                         RangeInfoBound(l.inclusive(), true, b2), // will steal b2's JSON
                         false);

            for (size_t j = 0; j < collector[k].size(); j++) {
              collector[k][j].fuse(ri);
            }
          }
        } 
        else {
          std::vector<RangeInfo> riv; 
          riv.reserve(bound.size());

          for (size_t j = 0; j < bound.size(); j++) {
            auto b1(bound.at(static_cast<int>(j)).copy()); // first instance of bound
            auto b2(bound.at(static_cast<int>(j)).copy()); // second instance of same bound

            riv.emplace_back(RangeInfo(r._var, 
                                       r._attr, 
                                       RangeInfoBound(l.inclusive(), true, b1), // will steal b1's JSON
                                       RangeInfoBound(l.inclusive(), true, b2), // will steal b2's JSON
                                       true));
          }

          collector[k] = std::move(andCombineRangeInfoVecs(collector[k], riv));
        } 
      }

      if (useHighBounds) {
        for (auto const& h : r._highs) {
          Expression* e = _allVariableBoundExpressions[posInExpressions];
          TRI_ASSERT(e != nullptr);
          TRI_document_collection_t const* myCollection = nullptr; 
          AqlValue a = e->execute(_trx, cur, _pos, _inVars[posInExpressions], _inRegs[posInExpressions], &myCollection);
          posInExpressions++;

          Json bound;
          if (a._type == AqlValue::JSON) {
            bound = *(a._json);
            a.destroy();  // the TRI_json_t* of a._json has been stolen
          } 
          else if (a._type == AqlValue::SHAPED || a._type == AqlValue::DOCVEC) {
            bound = a.toJson(_trx, myCollection, true);
            a.destroy();  // the TRI_json_t* of a._json has been stolen
          } 
          else {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, 
                "AQL: computed a variable bound and got non-JSON");
          }
          if (! bound.isArray()) {
            auto b(bound.copy());
            RangeInfo ri(r._var, 
                          r._attr, 
                          RangeInfoBound(), 
                          RangeInfoBound(h.inclusive(), true, b), // will steal b's JSON
                          false);

            for (size_t j = 0; j < collector[k].size(); j++) {
              collector[k][j].fuse(ri);
            }
          } 
          else {
            std::vector<RangeInfo> riv; 
            riv.reserve(bound.size());

            for (size_t j = 0; j < bound.size(); j++) {
              auto b1(bound.at(static_cast<int>(j)).copy()); // first instance of bound
              auto b2(bound.at(static_cast<int>(j)).copy()); // second instance of same bound

              riv.emplace_back(RangeInfo(r._var, 
                                          r._attr, 
                                          RangeInfoBound(h.inclusive(), true, b1), // will steal b1's JSON
                                          RangeInfoBound(h.inclusive(), true, b2), // will steal b2's JSON
                                          true));
            } 

            collector[k] = std::move(andCombineRangeInfoVecs(collector[k], riv));
          } 
        }
      }
    }


    bool isEmpty = false;

    for (auto const& x : collector) {
      if (x.empty()) { 
        isEmpty = true;
        break;
      }
    }

    if (! isEmpty) { 
      // otherwise the condition is impossible to fulfill
      // the elements of the direct product of the collector are and
      // conditions which should be added to newCondition 

      // create cartesian product
      std::unique_ptr<IndexOrCondition> indexAnds(cartesian(collector));

      if (newCondition == nullptr) {
        newCondition.reset(indexAnds.release());
      }
      else {
        for (auto const& indexAnd : *indexAnds) {
          newCondition->emplace_back(std::move(indexAnd));
        }
      } 
    }

  }
    
  freeCondition(); 
    
  if (newCondition != nullptr) {
    _condition = newCondition.release();
    _freeCondition = true;
 
    // remove duplicates . . .
    removeOverlapsIndexOr(*_condition);
  }
  else {
    _condition = new IndexOrCondition;
    _freeCondition = true;
  }
}

int IndexRangeBlock::initialize () {
  ENTER_BLOCK
  int res = ExecutionBlock::initialize();

  if (res == TRI_ERROR_NO_ERROR) {
    if (_trx->orderDitch(_trx->trxCollection(_collection->cid())) == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  _allVariableBoundExpressions.clear();

  // instanciate expressions:
  auto instanciateExpression = [&] (RangeInfoBound const& b) -> void {
    AstNode const* a = b.getExpressionAst(_engine->getQuery()->ast());
    Expression* expression = nullptr;
 
    {
      // all new AstNodes are registered with the Ast in the Query
      std::unique_ptr<Expression> e(new Expression(_engine->getQuery()->ast(), a));

      TRI_IF_FAILURE("IndexRangeBlock::initialize") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      _allVariableBoundExpressions.emplace_back(e.get());
      expression = e.release();
    }

    // Prepare _inVars and _inRegs:
    _inVars.emplace_back();
    std::vector<Variable*>& inVarsCur = _inVars.back();
    _inRegs.emplace_back();
    std::vector<RegisterId>& inRegsCur = _inRegs.back();

    std::unordered_set<Variable*>&& inVars = expression->variables();

    for (auto const& v : inVars) {
      inVarsCur.emplace_back(v);
      auto it = getPlanNode()->getRegisterPlan()->varInfo.find(v->id);
      TRI_ASSERT(it != getPlanNode()->getRegisterPlan()->varInfo.end());
      TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
      inRegsCur.emplace_back(it->second.registerId);
    }
  };

  // Get the ranges from the node:
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  std::vector<std::vector<RangeInfo>> const& orRanges = en->_ranges;
  
  for (size_t i = 0; i < orRanges.size(); i++) {
    if (! _allBoundsConstant[i]) {
      try {
        for (auto const& r : orRanges[i]) {
          for (auto const& l : r._lows) {
            instanciateExpression(l);
          }

          if (useHighBounds()) {
            for (auto const& h : r._highs) {
              instanciateExpression(h);
            }
          }
        }
        TRI_IF_FAILURE("IndexRangeBlock::initializeExpressions") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      }
      catch (...) {
        for (auto& e : _allVariableBoundExpressions) {
          delete e;
        }
        _allVariableBoundExpressions.clear();
        throw;
      }
    }
  }
    
  _hasV8Expression = hasV8Expression();

  return res;
  LEAVE_BLOCK;
}

// init the ranges for reading, this should be called once per new incoming
// block!
//
// This is either called every time we get a new incoming block. 
// If all the bounds are constant, then in the case of hash, primary or edges
// indexes it does nothing. In the case of a skiplist index, it creates a
// skiplistIterator which is used by readIndex. If at least one bound is
// variable, then this this also evaluates the IndexOrCondition required to
// determine the values of the bounds. 
//
// It is guaranteed that
//   _buffer   is not empty, in particular _buffer.front() is defined
//   _pos      points to a position in _buffer.front()
// Therefore, we can use the register values in _buffer.front() in row
// _pos to evaluate the variable bounds.

bool IndexRangeBlock::initRanges () {
  ENTER_BLOCK
  _flag = true; 

  // Find out about the actual values for the bounds in the variable bound case:

  if (_anyBoundVariable) {
    if (_hasV8Expression) {
      bool const isRunningInCluster = triagens::arango::ServerState::instance()->isRunningInCluster();

      // must have a V8 context here to protect Expression::execute()
      auto engine = _engine;
      triagens::basics::ScopeGuard guard{
        [&engine]() -> void { 
          engine->getQuery()->enterContext(); 
        },
        [&]() -> void {
          if (isRunningInCluster) {
            // must invalidate the expression now as we might be called from
            // different threads
            if (triagens::arango::ServerState::instance()->isRunningInCluster()) {
              for (auto const& e : _allVariableBoundExpressions) {
                e->invalidate();
              }
            }
          
            engine->getQuery()->exitContext(); 
          }
        }
      };

      ISOLATE;
      v8::HandleScope scope(isolate); // do not delete this!
    
      buildExpressions();
    }
    else {
      // no V8 context required!
      buildExpressions();
    }
  }
  
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  TRI_ASSERT(en->_index != nullptr);
   
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return true; //no initialization here!
  }
  
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    if (_condition == nullptr || _condition->empty()) {
      return false;
    }

    _posInRanges = 0;
    getEdgeIndexIterator(_condition->at(_posInRanges));
    return (_edgeIndexIterator != nullptr);
  }
      
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX) {
    if (_condition == nullptr || _condition->empty()) {
      return false;
    }

    _posInRanges = 0;
    getHashIndexIterator(_condition->at(_posInRanges));
    return (_hashIndexSearchValue._values != nullptr); 
  }
  
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    if (_condition == nullptr || _condition->empty()) {
      return false;
    }

    sortConditions();
    _posInRanges = 0;

    getSkiplistIterator(_condition->at(_sortCoords[_posInRanges]));
    return (_skiplistIterator != nullptr);
  }
          
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected index type"); 
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
// @brief: sorts the index range conditions and resets _posInRanges to 0
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::sortConditions () {
  size_t const n = _condition->size(); 

  if (! _sortCoords.empty()) {
    _sortCoords.clear();
    _sortCoords.reserve(n);
  }
  
  if (n == 1) {
    // nothing to do
    _sortCoords.emplace_back(0);
    TRI_IF_FAILURE("IndexRangeBlock::sortConditions") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    return;
  }
  
  // first sort by the prefix of the index
  std::vector<std::vector<size_t>> prefix;
  prefix.reserve(n);

  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  size_t const numFields = en->_index->fields.size();

  for (size_t s = 0; s < n; s++) {
    _sortCoords.emplace_back(s);
        
    TRI_IF_FAILURE("IndexRangeBlock::sortConditions") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    {
      std::vector<size_t> next;
      next.reserve(numFields);
      prefix.emplace_back(std::move(next));
    }

    // prefix[s][t] = position in _condition[s] corresponding to the <t>th index
    // field
    for (size_t t = 0; t < numFields; t++) {
      for (size_t u = 0; u < _condition->at(s).size(); u++) {
        auto const& ri = _condition->at(s)[u];

        if (en->_index->fields[t].compare(ri._attr) == 0) {
    
          TRI_IF_FAILURE("IndexRangeBlock::sortConditionsInner") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          prefix.at(s).insert(prefix.at(s).begin() + t, u);
          break;
        }
      }
    }
  }

  SortFunc sortFunc(prefix, _condition, en->_reverse);

  // then sort by the values of the bounds
  std::sort(_sortCoords.begin(), _sortCoords.end(), sortFunc);

  _posInRanges = 0;
}

////////////////////////////////////////////////////////////////////////////////
// @brief: is _condition[i] < _condition[j]? these are IndexAndConditions. 
////////////////////////////////////////////////////////////////////////////////

bool IndexRangeBlock::SortFunc::operator() (size_t const& i, size_t const& j) const {
  size_t l, r;

  if (! _reverse) {
    l = i;
    r = j;
  } 
  else {
    l = j; 
    r = i;
  }

  size_t shortest = (std::min)(_prefix[i].size(), _prefix[j].size());

  for (size_t k = 0; k < shortest; k++) {
    RangeInfo const& lhs = _condition->at(l).at(_prefix[l][k]);
    RangeInfo const& rhs = _condition->at(r).at(_prefix[r][k]);
    int cmp;
      
    if (lhs.is1ValueRangeInfo() && rhs.is1ValueRangeInfo()) {
      cmp = TRI_CompareValuesJson(lhs._lowConst.bound().json(), 
                                  rhs._lowConst.bound().json());
      if (cmp != 0) {
        return (cmp < 0);
      }
    } 
    else {
      // assuming lhs and rhs are disjoint!!
      TRI_ASSERT_EXPENSIVE(areDisjointRangeInfos(lhs, rhs));
      if (lhs._highConst.isDefined() && rhs._lowConst.isDefined()) {
        cmp = (TRI_CompareValuesJson(lhs._highConst.bound().json(), 
                                     rhs._lowConst.bound().json()));
        return (cmp == 0 || cmp < 0);
      } 
      // lhs._lowConst.isDefined() && rhs._highConst.isDefined()
      return false;
    }
  }
  TRI_ASSERT(false); 
  // shouldn't get here since the IndexAndConditions in _condition should be 
  // disjoint!
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineRangeInfoVecs: combine the arguments into a single vector,
/// by intersecting every pair of range infos and inserting them in the returned
/// value if the intersection is valid. 
////////////////////////////////////////////////////////////////////////////////

std::vector<RangeInfo> IndexRangeBlock::andCombineRangeInfoVecs (std::vector<RangeInfo> const& riv1, 
                                                                 std::vector<RangeInfo> const& riv2) const {
  std::vector<RangeInfo> out;
  
  std::unordered_set<TRI_json_t*, triagens::basics::JsonHash, triagens::basics::JsonEqual> cache(
    16, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );
      
  triagens::basics::ScopeGuard guard{
    []() -> void { },
    [&cache]() -> void {
      // free the JSON values in the cache
      for (auto& it : cache) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it);
      }
    }
  };

  for (RangeInfo const& ri1: riv1) {
    for (RangeInfo const& ri2: riv2) {
      RangeInfo x(ri1.clone());
      x.fuse(ri2);

      if (x.isValid()) {
        TRI_IF_FAILURE("IndexRangeBlock::andCombineRangeInfoVecs") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        if (x.is1ValueRangeInfo()) {
          // de-duplicate
          auto lowBoundValue = x._lowConst.bound().json();

          if (cache.find(lowBoundValue) != cache.end()) {
            // already seen the same value
            continue;
          }

          std::unique_ptr<TRI_json_t> copy(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, lowBoundValue));

          if (copy == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }

          // every JSON in the cache is a copy
          cache.emplace(copy.get());
          copy.release();
        }

        out.emplace_back(std::move(x));
      }
    }
  }
  
  return out;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cartesian: form the cartesian product of the inner vectors. This is
/// required in case a dynamic bound evaluates to a list, then we have an 
/// "and" condition containing an "or" condition, which we must then distribute. 
////////////////////////////////////////////////////////////////////////////////

IndexOrCondition* IndexRangeBlock::cartesian (std::vector<std::vector<RangeInfo>> const& collector) const {
  size_t const n = collector.size();
  
  std::vector<size_t> indexes;
  indexes.reserve(n);

  for (size_t i = 0; i < n; i++) {
    indexes.emplace_back(0);
  }
  
  std::unique_ptr<IndexOrCondition> out(new IndexOrCondition());

  while (true) {
    IndexAndCondition next;
    for (size_t i = 0; i < n; i++) {
      next.emplace_back(collector[i][indexes[i]].clone());
    }

    TRI_IF_FAILURE("IndexRangeBlock::cartesian") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    out->emplace_back(next);
    size_t j = n - 1;

    while (true) {
      indexes[j]++;
      if (indexes[j] < collector[j].size()) {
        break;
      }

      indexes[j] = 0;
      if (j == 0) { 
        return out.release();
      }
      j--;
    }
  }
}

void IndexRangeBlock::freeCondition () {
  if (_condition != nullptr && _freeCondition) {
    delete _condition;
    _condition = nullptr;
    _freeCondition = false;
  }
}

// this is called every time everything in _documents has been passed on

bool IndexRangeBlock::readIndex (size_t atMost) {
  ENTER_BLOCK;
  // this is called every time we want more in _documents. 
  // For the primary key index, this only reads the index once, and never 
  // again (although there might be multiple calls to this function). 
  // For the edge, hash or skiplists indexes, initRanges creates an iterator
  // and read*Index just reads from the iterator until it is done. 
  // Then initRanges is read again and so on. This is to avoid reading the
  // entire index when we only want a small number of documents. 
  
  if (_documents.empty()) {
    TRI_IF_FAILURE("IndexRangeBlock::readIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    _documents.reserve(atMost);
  }
  else { 
    _documents.clear();
  }
  
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
    if (_flag && _condition != nullptr) {
      readPrimaryIndex(*_condition);
    }
  }
  else if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    readEdgeIndex(atMost);
  }
  else if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX) {
    readHashIndex(atMost);
  }
  else if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    readSkiplistIndex(atMost);
  }
  else {
    TRI_ASSERT(false);
  }
  _flag = false;
  return (! _documents.empty());
  LEAVE_BLOCK;
}

int IndexRangeBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK;
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  _pos = 0;
  _posInDocs = 0;
  
  return TRI_ERROR_NO_ERROR; 
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* IndexRangeBlock::getSome (size_t atLeast,
                                        size_t atMost) {
  ENTER_BLOCK;
  if (_done) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> res(nullptr);

  do {
    // repeatedly try to get more stuff from upstream
    // note that the value of the variable we have to loop over
    // can contain zero entries, in which case we have to
    // try again!

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! ExecutionBlock::getBlock(toFetch, toFetch) 
          || (! initRanges())) {
        _done = true;
        return nullptr;
      }
      _pos = 0;           // this is in the first block

      // This is a new item, so let's read the index (it is already
      // initialised).
      readIndex(atMost);
      _posInDocs = 0;     // position in _documents . . .
    } 
    else if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,

      _posInDocs = 0;
      AqlItemBlock* cur = _buffer.front();

      if (! readIndex(atMost)) { //no more output from this version of the index
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          delete cur;
          _pos = 0;
        }
        if (_buffer.empty()) {
          if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize) ) {
            _done = true;
            return nullptr;
          }
          _pos = 0;           // this is in the first block
        }
        
        if (! initRanges()) {
          _done = true;
          return nullptr;
        }
        readIndex(atMost);
      }
    }

    // If we get here, we do have _buffer.front() and _pos points into it
    AqlItemBlock* cur = _buffer.front();
    size_t const curRegs = cur->getNrRegs();

    size_t available = _documents.size() - _posInDocs;
    size_t toSend = (std::min)(atMost, available);

    if (toSend > 0) {

      res.reset(new AqlItemBlock(toSend,
            getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

      // automatically freed should we throw
      TRI_ASSERT(curRegs <= res->getNrRegs());

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(cur, res.get(), _pos);

      // set our collection for our output register
      res->setDocumentCollection(static_cast<triagens::aql::RegisterId>(curRegs),
          _trx->documentCollection(_collection->cid()));

      for (size_t j = 0; j < toSend; j++) {
        if (j > 0) {
          // re-use already copied aqlvalues
          for (RegisterId i = 0; i < curRegs; i++) {
            res->setValue(j, i, res->getValueReference(0, i));
            // Note: if this throws, then all values will be deleted
            // properly since the first one is.
          }
        }

        // The result is in the first variable of this depth,
        // we do not need to do a lookup in getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        res->setValue(j, static_cast<triagens::aql::RegisterId>(curRegs),
                      AqlValue(reinterpret_cast<TRI_df_marker_t
                               const*>(_documents[_posInDocs++].getDataPtr())));
        // No harm done, if the setValue throws!
      }
    }

  }
  while (res.get() == nullptr);

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

size_t IndexRangeBlock::skipSome (size_t atLeast,
                                  size_t atMost) {
  
  if (_done) {
    return 0;
  }

  size_t skipped = 0;

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! ExecutionBlock::getBlock(toFetch, toFetch) 
          || (! initRanges())) {
        _done = true;
        return skipped;
      }
      _pos = 0;           // this is in the first block
      
      // This is a new item, so let's read the index if bounds are variable:
      readIndex(atMost); 
      _posInDocs = 0;     // position in _documents . . .
    }

    // If we get here, we do have _buffer.front() and _pos points into it
    AqlItemBlock* cur = _buffer.front();

    size_t available = _documents.size() - _posInDocs;
    size_t toSkip = (std::min)(atMost - skipped, available);

    _posInDocs += toSkip;
    skipped += toSkip;
    
    // Advance read position:
    if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,

      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        delete cur;
        _pos = 0;
      }

      // let's read the index if bounds are variable:
      if (! _buffer.empty()) {
        if (! initRanges()) {
          _done = true;
          return skipped;
        }
        readIndex(atMost);
      }
      _posInDocs = 0;
      
      // If _buffer is empty, then we will fetch a new block in the next round
      // and then read the index.
    }
  }

  return skipped; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents using the primary index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readPrimaryIndex (IndexOrCondition const& ranges) {
  ENTER_BLOCK;
  auto primaryIndex = _collection->documentCollection()->primaryIndex();
 
  for (size_t i = 0; i < ranges.size(); i++) {
    std::string key;

    for (auto const& x : ranges[i]) {
      if (x._attr == std::string(TRI_VOC_ATTRIBUTE_ID)) {
        // lookup by _id

        // we can use lower bound because only equality is supported
        TRI_ASSERT(x.is1ValueRangeInfo());
        auto const json = x._lowConst.bound().json();

        if (TRI_IsStringJson(json)) {
          // _id must be a string
          TRI_voc_cid_t documentCid;
          std::string documentKey;

          // parse _id value
          int errorCode = resolve(json->_value._string.data, documentCid, documentKey);

          if (errorCode == TRI_ERROR_NO_ERROR) {
            bool const isCluster = triagens::arango::ServerState::instance()->isRunningInCluster();

            if (! isCluster && documentCid == _collection->documentCollection()->_info._cid) {
              // only continue lookup if the id value is syntactically correct and
              // refers to "our" collection, using local collection id
              key = documentKey;
            }
            else if (isCluster && documentCid == _collection->documentCollection()->_info._planId) {
            // only continue lookup if the id value is syntactically correct and
            // refers to "our" collection, using cluster collection id
              key = documentKey;
            }
          }
        }
        /*if (! x._lows.empty() || ! x._highs.empty() || x._lowConst.isDefined() || x._highConst.isDefined()) {
          break;
        }*/
      }
      else if (x._attr == std::string(TRI_VOC_ATTRIBUTE_KEY)) {
        // lookup by _key

        // we can use lower bound because only equality is supported
        TRI_ASSERT(x.is1ValueRangeInfo());
        auto const json = x._lowConst.bound().json();
        if (TRI_IsStringJson(json)) {
          key = std::string(json->_value._string.data, json->_value._string.length - 1);
        }

        /*if (! x._lows.empty() || ! x._highs.empty() || x._lowConst.isDefined() || x._highConst.isDefined()) {
          break;
        }*/
      }
    }

    if (! key.empty()) {
      ++_engine->_stats.scannedIndex;

      auto found = static_cast<TRI_doc_mptr_t const*>(primaryIndex->lookupKey(key.c_str()));

      if (found != nullptr) {
        _documents.emplace_back(*found);
      }
    }
  }

  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build search values for edge index lookup
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::getEdgeIndexIterator (IndexAndCondition const& ranges) {
  ENTER_BLOCK;
  
  _edgeNextElement = nullptr;

  if (_edgeIndexIterator != nullptr) {
    delete _edgeIndexIterator;
    _edgeIndexIterator = nullptr;
  }
 
  auto buildIterator = [this] (TRI_edge_direction_e direction, TRI_json_t const* key) -> void {
    TRI_ASSERT(_edgeIndexIterator == nullptr);

    TRI_voc_cid_t documentCid;
    std::string documentKey;

    int errorCode = resolve(key->_value._string.data, documentCid, documentKey);

    if (errorCode == TRI_ERROR_NO_ERROR) {
      _edgeIndexIterator = new TRI_edge_index_iterator_t(direction, documentCid, (TRI_voc_key_t) documentKey.c_str());
    }
  };

  for (auto const& x : ranges) {
    if (x._attr == std::string(TRI_VOC_ATTRIBUTE_FROM)) {
      // we can use lower bound because only equality is supported
      TRI_ASSERT(x.is1ValueRangeInfo());
      auto const json = x._lowConst.bound().json();
      if (TRI_IsStringJson(json)) {
        // no error will be thrown if _from is not a string
        buildIterator(TRI_EDGE_OUT, json);
      }
      break;
    }
    else if (x._attr == std::string(TRI_VOC_ATTRIBUTE_TO)) {
      // we can use lower bound because only equality is supported
      TRI_ASSERT(x.is1ValueRangeInfo());
      auto const json = x._lowConst.bound().json();
      if (TRI_IsStringJson(json)) {
        // no error will be thrown if _to is not a string
        buildIterator(TRI_EDGE_IN, json);
      }
      break;
    }
  }
  
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually read from the edge index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readEdgeIndex (size_t atMost) {
  ENTER_BLOCK;

  if (_edgeIndexIterator == nullptr) {
    return;
  }

  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);
 
  try { 
    size_t nrSent = 0;
    while (nrSent < atMost && _edgeIndexIterator != nullptr) { 
      size_t const n = _documents.size();

      TRI_IF_FAILURE("IndexRangeBlock::readEdgeIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      static_cast<triagens::arango::EdgeIndex*>(idx)->lookup(_edgeIndexIterator, _documents, _edgeNextElement, atMost);
    
      size_t const numRead = _documents.size() - n;

      _engine->_stats.scannedIndex += static_cast<int64_t>(numRead);
      nrSent += numRead;

      if (_edgeNextElement == nullptr) {
        delete _edgeIndexIterator;
        _edgeIndexIterator = nullptr;

        if (++_posInRanges < _condition->size()) {
          getEdgeIndexIterator(_condition->at(_posInRanges));
        }
      }
    }
  }
  catch (...) {
    if (_edgeIndexIterator != nullptr) {
      delete _edgeIndexIterator;
      _edgeIndexIterator = nullptr;
    }
    throw;
  }
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the search values for the hash index lookup
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::destroyHashIndexSearchValues () {
  if (_hashIndexSearchValue._values != nullptr) {
    TRI_shaper_t* shaper = _collection->documentCollection()->getShaper(); 

    for (size_t i = 0; i < _hashIndexSearchValue._length; ++i) {
      TRI_DestroyShapedJson(shaper->_memoryZone, &_hashIndexSearchValue._values[i]);
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _hashIndexSearchValue._values);
    _hashIndexSearchValue._values = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up search values for the hash index lookup
////////////////////////////////////////////////////////////////////////////////

bool IndexRangeBlock::setupHashIndexSearchValue (IndexAndCondition const& range) { 
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);

  auto hashIndex = static_cast<triagens::arango::HashIndex*>(idx);
  auto const& paths = hashIndex->paths();

  TRI_shaper_t* shaper = _collection->documentCollection()->getShaper(); 

  size_t const n = paths.size();

  TRI_ASSERT(_hashIndexSearchValue._values == nullptr); // to prevent leak
  _hashIndexSearchValue._length = 0;
    // initialize the whole range of shapes with zeros
  _hashIndexSearchValue._values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, 
      n * sizeof(TRI_shaped_json_t), true));

  if (_hashIndexSearchValue._values == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
    
  _hashIndexSearchValue._length = n;


  for (size_t i = 0; i < n; ++i) {
    TRI_shape_pid_t pid = paths[i];
    TRI_ASSERT(pid != 0);
   
    char const* name = TRI_AttributeNameShapePid(shaper, pid);
    std::string const lookFor(name);

    for (auto const& x : range) {
      if (x._attr == lookFor) {    //found attribute
        if (x._lowConst.bound().json() == nullptr) {
          // attribute is empty. this may be case if a function expression is used as a 
          // comparison value, and the function returns an empty list, e.g. x.a IN PASSTHRU([])
          return false;
        }

        auto shaped = TRI_ShapedJsonJson(shaper, x._lowConst.bound().json(), false); 
        // here x->_low->_bound == x->_high->_bound 
        if (shaped == nullptr) {
          return false;
        }

        _hashIndexSearchValue._values[i] = *shaped;
        // free only the pointer, but not the internals
        TRI_Free(shaper->_memoryZone, shaped);
        break; 
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build search values for hash index lookup
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::getHashIndexIterator (IndexAndCondition const& ranges) {
  ENTER_BLOCK;
  
  _hashNextElement = nullptr;
 
  destroyHashIndexSearchValues();
  if (! setupHashIndexSearchValue(ranges)) {
    destroyHashIndexSearchValues();
  }

  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually read from the hash index
////////////////////////////////////////////////////////////////////////////////
 
void IndexRangeBlock::readHashIndex (size_t atMost) {
  ENTER_BLOCK;

  if (_hashIndexSearchValue._values == nullptr) {
    return;
  }

  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);
  
  size_t nrSent = 0;
  while (nrSent < atMost) { 
    size_t const n = _documents.size();

    TRI_IF_FAILURE("IndexRangeBlock::readHashIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    static_cast<triagens::arango::HashIndex*>(idx)->lookup(&_hashIndexSearchValue, _documents, _hashNextElement, atMost);
    size_t const numRead = _documents.size() - n;

    _engine->_stats.scannedIndex += static_cast<int64_t>(numRead);
    nrSent += numRead;

    if (_hashNextElement == nullptr) {
      destroyHashIndexSearchValues();

      if (++_posInRanges < _condition->size()) {
        getHashIndexIterator(_condition->at(_posInRanges));
      }
      if (_hashIndexSearchValue._values == nullptr) {
        _hashNextElement = nullptr;
        break;
      }
    }
  }
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents using a skiplist index
////////////////////////////////////////////////////////////////////////////////

// it is only possible to query a skip list using more than one attribute if we
// only have equalities followed by a single arbitrary comparison (i.e x.a == 1
// && x.b == 2 && x.c > 3 && x.c <= 4). Then we do: 
//
//   TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, left, right, nullptr, shaper,
//     2);
//
// where 
//
//   left =  TRI_CreateIndexOperator(TRI_GT_INDEX_OPERATOR, nullptr, nullptr, [1,2,3],
//     shaper, 3)
//
//   right =  TRI_CreateIndexOperator(TRI_LE_INDEX_OPERATOR, nullptr, nullptr, [1,2,4],
//     shaper, 3)
//
// If the final comparison is an equality (x.a == 1 && x.b == 2 && x.c ==3), then 
// we just do:
//
//   TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr, nullptr, [1,2,3],
//     shaper, 3)
//
// It is necessary that values of the attributes are listed in the correct
// order (i.e. <a> must be the first attribute indexed, and <b> must be the
// second). If one of the attributes is not indexed, then it is ignored,
// provided we are querying all the previously indexed attributes (i.e. we
// cannot do (x.c == 1 && x.a == 2) if the index covers <a>, <b>, <c> in this
// order but we can do (x.a == 2)).
//
// If the comparison is not equality, then the values of the parameters 
// (i.e. the 1 in x.c >= 1) cannot be lists or arrays.
//

void IndexRangeBlock::getSkiplistIterator (IndexAndCondition const& ranges) {
  ENTER_BLOCK;
  TRI_ASSERT(_skiplistIterator == nullptr);
  
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);

  TRI_shaper_t* shaper = _collection->documentCollection()->getShaper(); 
  TRI_ASSERT(shaper != nullptr);

  TRI_index_operator_t* skiplistOperator = nullptr; 

  Json parameters(Json::Array); 
  size_t i = 0;
  for (; i < ranges.size(); i++) {
    auto const& range = ranges[i];
    // TRI_ASSERT(range.isConstant());
    if (range.is1ValueRangeInfo()) {   // it's an equality . . . 
      parameters(range._lowConst.bound().copy());
    } 
    else {                          // it's not an equality and so the final comparison 
      if (parameters.size() != 0) {
        skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr,
            nullptr, parameters.copy().steal(), shaper, i);
      }
      if (range._lowConst.isDefined()) {
        auto op = range._lowConst.toIndexOperator(false, parameters.copy(), shaper);

        if (skiplistOperator != nullptr) {
          skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
              skiplistOperator, op, nullptr, shaper, 2);
        } 
        else {
          skiplistOperator = op;
        }
      }
      if (range._highConst.isDefined()) {
        auto op = range._highConst.toIndexOperator(true, parameters.copy(), shaper);

        if (skiplistOperator != nullptr) {
          skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
              skiplistOperator, op, nullptr, shaper, 2);
        } 
        else {
          skiplistOperator = op;
        }
      }
    }
  }

  if (skiplistOperator == nullptr) {      // only have equalities . . .
    if (parameters.size() == 0) {
      // this creates the infinite range (i.e. >= null)
      Json hass(Json::Array);
      hass.add(Json(Json::Null));
      skiplistOperator = TRI_CreateIndexOperator(TRI_GE_INDEX_OPERATOR, nullptr,
          nullptr, hass.steal(), shaper, 1);
    }
    else {
      skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr,
          nullptr, parameters.steal(), shaper, i);
    }
  }
  
  if (_skiplistIterator != nullptr) {
    TRI_FreeSkiplistIterator(_skiplistIterator);
  }

  _skiplistIterator = static_cast<triagens::arango::SkiplistIndex2*>(idx)->lookup(skiplistOperator, en->_reverse);

  if (skiplistOperator != nullptr) {
    TRI_FreeIndexOperator(skiplistOperator);
  }

  if (_skiplistIterator == nullptr) {
    int res = TRI_errno();

    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      return;
    }

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually read from the skiplist index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readSkiplistIndex (size_t atMost) {
  ENTER_BLOCK;
  if (_skiplistIterator == nullptr) {
    return;
  }

  try {
    size_t nrSent = 0;
    while (nrSent < atMost && _skiplistIterator !=nullptr) { 
      TRI_skiplist_index_element_t* indexElement = _skiplistIterator->next(_skiplistIterator);

      if (indexElement == nullptr) {
        TRI_FreeSkiplistIterator(_skiplistIterator);
        _skiplistIterator = nullptr;
        if (++_posInRanges < _condition->size()) {
          getSkiplistIterator(_condition->at(_sortCoords[_posInRanges]));
        }
      } 
      else {
        TRI_IF_FAILURE("IndexRangeBlock::readSkiplistIndex") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        
        _documents.emplace_back(*(indexElement->_document));
        ++nrSent;
        ++_engine->_stats.scannedIndex;
      }
    }
  }
  catch (...) {
    if (_skiplistIterator != nullptr) {
      TRI_FreeSkiplistIterator(_skiplistIterator);
      _skiplistIterator = nullptr;
    }
    throw;
  }
  LEAVE_BLOCK;
}

// -----------------------------------------------------------------------------
// --SECTION--                                          class EnumerateListBlock
// -----------------------------------------------------------------------------
        
EnumerateListBlock::EnumerateListBlock (ExecutionEngine* engine,
                                        EnumerateListNode const* en)
  : ExecutionBlock(engine, en),
    _inVarRegId(ExecutionNode::MaxRegisterId) {

  auto it = en->getRegisterPlan()->varInfo.find(en->_inVariable->id);

  if (it == en->getRegisterPlan()->varInfo.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "variable not found");
  }

  _inVarRegId = (*it).second.registerId;
  TRI_ASSERT(_inVarRegId < ExecutionNode::MaxRegisterId);
}

EnumerateListBlock::~EnumerateListBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, here we get the inVariable
////////////////////////////////////////////////////////////////////////////////

int EnumerateListBlock::initialize () {
  return ExecutionBlock::initialize();
}

int EnumerateListBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // handle local data (if any)
  _index = 0;     // index in _inVariable for next run
  _thisblock = 0; // the current block in the _inVariable DOCVEC
  _seen = 0;      // the sum of the sizes of the blocks in the _inVariable
  // DOCVEC that preceed _thisblock

  return TRI_ERROR_NO_ERROR;
}

AqlItemBlock* EnumerateListBlock::getSome (size_t, size_t atMost) {
  if (_done) {
    return nullptr;
  }

  unique_ptr<AqlItemBlock> res(nullptr);

  do {
    // repeatedly try to get more stuff from upstream
    // note that the value of the variable we have to loop over
    // can contain zero entries, in which case we have to
    // try again!

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! ExecutionBlock::getBlock(toFetch, toFetch)) {
        _done = true;
        return nullptr;
      }
      _pos = 0;           // this is in the first block
    }

    // if we make it here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    // get the thing we are looping over
    AqlValue inVarReg = cur->getValueReference(_pos, _inVarRegId);
    size_t sizeInVar = 0; // to shut up compiler

    // get the size of the thing we are looping over
    _collection = nullptr;

    switch (inVarReg._type) {
      case AqlValue::JSON: {
        if (! inVarReg._json->isArray()) {
          throwArrayExpectedException();
        }
        sizeInVar = inVarReg._json->size();
        break;
      }

      case AqlValue::RANGE: {
        sizeInVar = inVarReg._range->size();
        break;
      }

      case AqlValue::DOCVEC: {
        if (_index == 0) { // this is a (maybe) new DOCVEC
          _DOCVECsize = 0;
          // we require the total number of items

          for (size_t i = 0; i < inVarReg._vector->size(); i++) {
            _DOCVECsize += inVarReg._vector->at(i)->size();
          }
        }
        sizeInVar = _DOCVECsize;
        if (sizeInVar > 0) {
          _collection = inVarReg._vector->at(0)->getDocumentCollection(0);
        }
        break;
      }

      case AqlValue::SHAPED: {
        throwArrayExpectedException();
      }

      case AqlValue::EMPTY: {
        throwArrayExpectedException();
      }
    }

    if (sizeInVar == 0) {
      res = nullptr;
    }
    else {
      size_t toSend = (std::min)(atMost, sizeInVar - _index);

      // create the result
      res.reset(new AqlItemBlock(toSend, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

      inheritRegisters(cur, res.get(), _pos);

      // we might have a collection:
      res->setDocumentCollection(cur->getNrRegs(), _collection);

      for (size_t j = 0; j < toSend; j++) {
        if (j > 0) {
          // re-use already copied aqlvalues
          for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
            res->setValue(j, i, res->getValueReference(0, i));
            // Note that if this throws, all values will be
            // deleted properly, since the first row is.
          }
        }
        // add the new register value . . .
        AqlValue a = getAqlValue(inVarReg);
        // deep copy of the inVariable.at(_pos) with correct memory
        // requirements
        // Note that _index has been increased by 1 by getAqlValue!
        try {
          TRI_IF_FAILURE("EnumerateListBlock::getSome") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          res->setValue(j, cur->getNrRegs(), a);
        }
        catch (...) {
          a.destroy();
          throw;
        }
      }
    }

    if (_index == sizeInVar) {
      _index = 0;
      _thisblock = 0;
      _seen = 0;
      // advance read position in the current block . . .
      if (++_pos == cur->size()) {
        delete cur;
        _buffer.pop_front();  // does not throw
        _pos = 0;
      }
    }
  }
  while (res.get() == nullptr);

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
}

size_t EnumerateListBlock::skipSome (size_t atLeast, size_t atMost) {

  if (_done) {
    return 0;
  }

  size_t skipped = 0;

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! ExecutionBlock::getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;           // this is in the first block
    }

    // if we make it here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    // get the thing we are looping over
    AqlValue inVarReg = cur->getValue(_pos, _inVarRegId);
    size_t sizeInVar = 0; // to shut up compiler

    // get the size of the thing we are looping over
    switch (inVarReg._type) {
      case AqlValue::JSON: {
        if (! inVarReg._json->isArray()) {
          throwArrayExpectedException();
        }
        sizeInVar = inVarReg._json->size();
        break;
      }

      case AqlValue::RANGE: {
        sizeInVar = inVarReg._range->size();
        break;
      }

      case AqlValue::DOCVEC: {
        if (_index == 0) { // this is a (maybe) new DOCVEC
          _DOCVECsize = 0;
          //we require the total number of items 
          for (size_t i = 0; i < inVarReg._vector->size(); i++) {
            _DOCVECsize += inVarReg._vector->at(i)->size();
          }
        }
        sizeInVar = _DOCVECsize;
        break;
      }

      case AqlValue::SHAPED: 
      case AqlValue::EMPTY: {
        throwArrayExpectedException();
      }
    }

    if (atMost < sizeInVar - _index) {
      // eat just enough of inVariable . . .
      _index += atMost;
      skipped = atMost;
    }
    else {
      // eat the whole of the current inVariable and proceed . . .
      skipped += (sizeInVar - _index);
      _index = 0;
      _thisblock = 0;
      _seen = 0;
      delete cur;
      _buffer.pop_front();
      _pos = 0;
    }
  }
  return skipped;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from the inVariable using the current _index
////////////////////////////////////////////////////////////////////////////////

AqlValue EnumerateListBlock::getAqlValue (AqlValue const& inVarReg) {
  TRI_IF_FAILURE("EnumerateListBlock::getAqlValue") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  switch (inVarReg._type) {
    case AqlValue::JSON: {
      // FIXME: is this correct? What if the copy works, but the
      // new throws? Is this then a leak? What if the new works
      // but the AqlValue temporary cannot be made?
      return AqlValue(new Json(inVarReg._json->at(static_cast<int>(_index++)).copy()));
    }
    case AqlValue::RANGE: {
      return AqlValue(new Json(static_cast<double>(inVarReg._range->at(_index++))));
    }
    case AqlValue::DOCVEC: { // incoming doc vec has a single column
      AqlValue out = inVarReg._vector->at(_thisblock)->getValue(_index -
                                                                _seen, 0).clone();
      if (++_index == (inVarReg._vector->at(_thisblock)->size() + _seen)) {
        _seen += inVarReg._vector->at(_thisblock)->size();
        _thisblock++;
      }
      return out;
    }

    case AqlValue::SHAPED:
    case AqlValue::EMPTY: {
      // error
      break;
    }
  }

  throwArrayExpectedException();
  TRI_ASSERT(false);

  // cannot be reached. function call above will always throw an exception
  return AqlValue();
}
          
void EnumerateListBlock::throwArrayExpectedException () {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_ARRAY_EXPECTED, 
                                 TRI_errno_string(TRI_ERROR_QUERY_ARRAY_EXPECTED) +
                                 std::string(" as operand to FOR loop"));
}

// -----------------------------------------------------------------------------
// --SECTION--                                            class CalculationBlock
// -----------------------------------------------------------------------------
        
CalculationBlock::CalculationBlock (ExecutionEngine* engine,
                                    CalculationNode const* en)
  : ExecutionBlock(engine, en),
    _expression(en->expression()),
    _inVars(),
    _inRegs(),
    _outReg(ExecutionNode::MaxRegisterId) {

  std::unordered_set<Variable*> const& inVars = _expression->variables();
  _inVars.reserve(inVars.size());
  _inRegs.reserve(inVars.size());

  for (auto it = inVars.begin(); it != inVars.end(); ++it) {
    _inVars.emplace_back(*it);
    auto it2 = en->getRegisterPlan()->varInfo.find((*it)->id);

    TRI_ASSERT(it2 != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT(it2->second.registerId < ExecutionNode::MaxRegisterId);
    _inRegs.emplace_back(it2->second.registerId);
  }

  // check if the expression is "only" a reference to another variable
  // this allows further optimizations inside doEvaluation
  _isReference = (_expression->node()->type == NODE_TYPE_REFERENCE);

  if (_isReference) {
    TRI_ASSERT(_inRegs.size() == 1);
  }

  auto it3 = en->getRegisterPlan()->varInfo.find(en->_outVariable->id);
  TRI_ASSERT(it3 != en->getRegisterPlan()->varInfo.end());
  _outReg = it3->second.registerId;
  TRI_ASSERT(_outReg < ExecutionNode::MaxRegisterId);
  
  if (en->_conditionVariable != nullptr) {
    auto it = en->getRegisterPlan()->varInfo.find(en->_conditionVariable->id);
    TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
    _conditionReg = it->second.registerId;
    TRI_ASSERT(_conditionReg < ExecutionNode::MaxRegisterId);
  }
}

CalculationBlock::~CalculationBlock () {
}

int CalculationBlock::initialize () {
  return ExecutionBlock::initialize();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fill the target register in the item block with a reference to 
/// another variable
////////////////////////////////////////////////////////////////////////////////

void CalculationBlock::fillBlockWithReference (AqlItemBlock* result) {
  result->setDocumentCollection(_outReg, result->getDocumentCollection(_inRegs[0]));

  size_t const n = result->size();
  for (size_t i = 0; i < n; i++) {
    // need not clone to avoid a copy, the AqlItemBlock's hash takes
    // care of correct freeing:
    auto a = result->getValueReference(i, _inRegs[0]);

    try {
      TRI_IF_FAILURE("CalculationBlock::fillBlockWithReference") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      result->setValue(i, _outReg, a);
    }
    catch (...) {
      a.destroy();
      throw;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shared code for executing a simple or a V8 expression
////////////////////////////////////////////////////////////////////////////////

void CalculationBlock::executeExpression (AqlItemBlock* result) {
  result->setDocumentCollection(_outReg, nullptr);

  bool const hasCondition = (static_cast<CalculationNode const*>(_exeNode)->_conditionVariable != nullptr);

  size_t const n = result->size();

  for (size_t i = 0; i < n; i++) {
    // check the condition variable (if any)
    if (hasCondition) {
      AqlValue conditionResult = result->getValueReference(i, _conditionReg);

      if (! conditionResult.isTrue()) {
        TRI_IF_FAILURE("CalculationBlock::executeExpressionWithCondition") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        result->setValue(i, _outReg, AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, &Expression::NullJson, Json::NOFREE)));
        continue;
      }
    }
    
    // execute the expression
    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue a = _expression->execute(_trx, result, i, _inVars, _inRegs, &myCollection);
    
    try {
      TRI_IF_FAILURE("CalculationBlock::executeExpression") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      result->setValue(i, _outReg, a);
    }
    catch (...) {
      a.destroy();
      throw;
    }
    throwIfKilled(); // check if we were aborted
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief doEvaluation, private helper to do the work
////////////////////////////////////////////////////////////////////////////////

void CalculationBlock::doEvaluation (AqlItemBlock* result) {
  TRI_ASSERT(result != nullptr);

  if (_isReference) {
    // the calculation is a reference to a variable only.
    // no need to execute the expression at all
    fillBlockWithReference(result);
    throwIfKilled(); // check if we were aborted
    return;
  }

  // non-reference expression

  TRI_ASSERT(_expression != nullptr);

  if (! _expression->isV8()) {
    // an expression that does not require V8
    executeExpression(result);
  }
  else {
    bool const isRunningInCluster = triagens::arango::ServerState::instance()->isRunningInCluster();

    // must have a V8 context here to protect Expression::execute()
    triagens::basics::ScopeGuard guard{
      [&]() -> void {
        _engine->getQuery()->enterContext(); 
      },
      [&]() -> void { 
        if (isRunningInCluster) {
          // must invalidate the expression now as we might be called from
          // different threads
          _expression->invalidate();
        
          _engine->getQuery()->exitContext(); 
        }
      }
    };

    ISOLATE;
    v8::HandleScope scope(isolate); // do not delete this!

    // do not merge the following function call with the same function call above!
    // the V8 expression execution must happen in the scope that contains
    // the V8 handle scope and the scope guard
    executeExpression(result);
  }
}

AqlItemBlock* CalculationBlock::getSome (size_t atLeast,
                                         size_t atMost) {

  std::unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

  if (res.get() == nullptr) {
    return nullptr;
  }

  doEvaluation(res.get());
  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                               class SubqueryBlock
// -----------------------------------------------------------------------------
        
SubqueryBlock::SubqueryBlock (ExecutionEngine* engine,
                              SubqueryNode const* en,
                              ExecutionBlock* subquery)
  : ExecutionBlock(engine, en), 
    _outReg(ExecutionNode::MaxRegisterId),
    _subquery(subquery) {
  
  auto it = en->getRegisterPlan()->varInfo.find(en->_outVariable->id);
  TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
  _outReg = it->second.registerId;
  TRI_ASSERT(_outReg < ExecutionNode::MaxRegisterId);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

SubqueryBlock::~SubqueryBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, tell dependency and the subquery
////////////////////////////////////////////////////////////////////////////////

int SubqueryBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return getSubquery()->initialize();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* SubqueryBlock::getSome (size_t atLeast,
                                      size_t atMost) {
  std::unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

  if (res.get() == nullptr) {
    return nullptr;
  }

  // TODO: constant and deterministic subqueries only need to be executed once
  bool const subqueryIsConst = false; // TODO 

  std::vector<AqlItemBlock*>* subqueryResults = nullptr;

  for (size_t i = 0; i < res->size(); i++) {
    int ret = _subquery->initializeCursor(res.get(), i);

    if (ret != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(ret);
    }

    if (i > 0 && subqueryIsConst) {
      // re-use already calculated subquery result
      TRI_ASSERT(subqueryResults != nullptr);
      res->setValue(i, _outReg, AqlValue(subqueryResults));
    }
    else {
      // initial subquery execution or subquery is not constant

      // execute the subquery
      subqueryResults = executeSubquery();
      TRI_ASSERT(subqueryResults != nullptr);

      try {
        TRI_IF_FAILURE("SubqueryBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        res->setValue(i, _outReg, AqlValue(subqueryResults));
      }
      catch (...) {
        destroySubqueryResults(subqueryResults);
        throw;
      }
    } 
      
    throwIfKilled(); // check if we were aborted
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, tell dependency and the subquery
////////////////////////////////////////////////////////////////////////////////

int SubqueryBlock::shutdown (int errorCode) {
  int res = ExecutionBlock::shutdown(errorCode);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return getSubquery()->shutdown(errorCode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the subquery and return its results
////////////////////////////////////////////////////////////////////////////////

std::vector<AqlItemBlock*>* SubqueryBlock::executeSubquery () {
  auto results = new std::vector<AqlItemBlock*>;

  try {
    do {
      std::unique_ptr<AqlItemBlock> tmp(_subquery->getSome(DefaultBatchSize, DefaultBatchSize));

      if (tmp.get() == nullptr) {
        break;
      }

      TRI_IF_FAILURE("SubqueryBlock::executeSubquery") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      results->emplace_back(tmp.get());
      tmp.release();
    }
    while (true);
    return results;
  }
  catch (...) {
    destroySubqueryResults(results);
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the results of a subquery
////////////////////////////////////////////////////////////////////////////////

void SubqueryBlock::destroySubqueryResults (std::vector<AqlItemBlock*>* results) {
  for (auto& x : *results) {
    delete x;
  }
  delete results;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class FilterBlock
// -----------------------------------------------------------------------------
        
FilterBlock::FilterBlock (ExecutionEngine* engine,
                          FilterNode const* en)
  : ExecutionBlock(engine, en),
    _inReg(ExecutionNode::MaxRegisterId) {
  
  auto it = en->getRegisterPlan()->varInfo.find(en->_inVariable->id);
  TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
  _inReg = it->second.registerId;
  TRI_ASSERT(_inReg < ExecutionNode::MaxRegisterId);
}

FilterBlock::~FilterBlock () {
}

int FilterBlock::initialize () {
  return ExecutionBlock::initialize();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to get another block
////////////////////////////////////////////////////////////////////////////////

bool FilterBlock::getBlock (size_t atLeast, size_t atMost) {
  while (true) {  // will be left by break or return
    if (! ExecutionBlock::getBlock(atLeast, atMost)) {
      return false;
    }

    if (_buffer.size() > 1) {
      break;  // Already have a current block
    }

    // Now decide about these docs:
    AqlItemBlock* cur = _buffer.front();

    _chosen.clear();
    _chosen.reserve(cur->size());
    for (size_t i = 0; i < cur->size(); ++i) {
      if (takeItem(cur, i)) {
        TRI_IF_FAILURE("FilterBlock::getBlock") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _chosen.emplace_back(i);
      }
    }

    _engine->_stats.filtered += (cur->size() - _chosen.size());

    if (! _chosen.empty()) {
      break;   // OK, there are some docs in the result
    }

    _buffer.pop_front();  // Block was useless, just try again
    delete cur;   // free this block
  }

  return true;
}

int FilterBlock::getOrSkipSome (size_t atLeast,
                                size_t atMost,
                                bool skipping,
                                AqlItemBlock*& result,
                                size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  // if _buffer.size() is > 0 then _pos is valid
  std::vector<AqlItemBlock*> collector;

  try {
    while (skipped < atLeast) {
      if (_buffer.empty()) {
        if (! getBlock(atLeast - skipped, atMost - skipped)) {
          _done = true;
          break;
        }
        _pos = 0;
      }
  
      // If we get here, then _buffer.size() > 0 and _pos points to a
      // valid place in it.
      AqlItemBlock* cur = _buffer.front();
      if (_chosen.size() - _pos + skipped > atMost) {
        // The current block of chosen ones is too large for atMost:
        if (! skipping) {
          std::unique_ptr<AqlItemBlock> more(cur->slice(_chosen, _pos, _pos + (atMost - skipped)));
        
          TRI_IF_FAILURE("FilterBlock::getOrSkipSome1") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          collector.emplace_back(more.get());
          more.release();
        }
        _pos += atMost - skipped;
        skipped = atMost;
      }
      else if (_pos > 0 || _chosen.size() < cur->size()) {
        // The current block fits into our result, but it is already
        // half-eaten or needs to be copied anyway:
        if (! skipping) {
          std::unique_ptr<AqlItemBlock> more(cur->steal(_chosen, _pos, _chosen.size()));
          
          TRI_IF_FAILURE("FilterBlock::getOrSkipSome2") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          collector.emplace_back(more.get());
          more.release();
        }
        skipped += _chosen.size() - _pos;
        delete cur;
        _buffer.pop_front();
        _chosen.clear();
        _pos = 0;
      }
      else {
        // The current block fits into our result and is fresh and
        // takes them all, so we can just hand it on:
        skipped += cur->size();
        if (! skipping) {
          // if any of the following statements throw, then cur is not lost,
          // as it is still contained in _buffer
          TRI_IF_FAILURE("FilterBlock::getOrSkipSome3") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          collector.emplace_back(cur);
        }
        else {
          delete cur;
        }
        _buffer.pop_front();
        _chosen.clear();
        _pos = 0;
      }
    }
  }
  catch (...) {
    for (auto& c : collector) {
      delete c;
    }
    throw;
  }

  if (! skipping) {
    if (collector.size() == 1) {
      result = collector[0];
    }
    else if (collector.size() > 1) {
      try {
        TRI_IF_FAILURE("FilterBlock::getOrSkipSomeConcatenate") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        result = AqlItemBlock::concatenate(collector);
      }
      catch (...) {
        for (auto& x : collector) {
          delete x;
        }
        throw;
      }
      for (auto& x : collector) {
        delete x;
      }
    }
  }
  return TRI_ERROR_NO_ERROR;
}

bool FilterBlock::hasMore () {
  if (_done) {
    return false;
  }

  if (_buffer.empty()) {
    // QUESTION: Is this sensible? Asking whether there is more might
    // trigger an expensive fetching operation, even if later on only
    // a single document is needed due to a LIMIT...
    // However, how should we know this here?
    if (! getBlock(DefaultBatchSize, DefaultBatchSize)) {
      _done = true;
      return false;
    }
    _pos = 0;
  }

  TRI_ASSERT(! _buffer.empty());

  // Here, _buffer.size() is > 0 and _pos points to a valid place
  // in it.
  
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        class SortedAggregateBlock
// -----------------------------------------------------------------------------
        
SortedAggregateBlock::SortedAggregateBlock (ExecutionEngine* engine,
                                            AggregateNode const* en)
  : ExecutionBlock(engine, en),
    _aggregateRegisters(),
    _currentGroup(en->_count),
    _expressionRegister(ExecutionNode::MaxRegisterId),
    _groupRegister(ExecutionNode::MaxRegisterId),
    _variableNames() {
 
  for (auto const& p : en->_aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != en->getRegisterPlan()->varInfo.end());

    auto itIn = en->getRegisterPlan()->varInfo.find(p.second->id);
    TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    _aggregateRegisters.emplace_back(make_pair((*itOut).second.registerId, (*itIn).second.registerId));
  }

  if (en->_outVariable != nullptr) {
    auto const& registerPlan = en->getRegisterPlan()->varInfo;
    auto it = registerPlan.find(en->_outVariable->id);
    TRI_ASSERT(it != registerPlan.end());
    _groupRegister = (*it).second.registerId;
    TRI_ASSERT(_groupRegister > 0 && _groupRegister < ExecutionNode::MaxRegisterId);

    if (en->_expressionVariable != nullptr) {
      auto it = registerPlan.find(en->_expressionVariable->id);
      TRI_ASSERT(it != registerPlan.end());
      _expressionRegister = (*it).second.registerId;
    }

    // construct a mapping of all register ids to variable names
    // we need this mapping to generate the grouped output

    for (size_t i = 0; i < registerPlan.size(); ++i) {
      _variableNames.emplace_back(""); // initialize with some default value
    }
            
    // iterate over all our variables
    if (en->_keepVariables.empty()) {
      auto&& usedVariableIds = en->getVariableIdsUsedHere();

      for (auto const& vi : registerPlan) {
        if (vi.second.depth > 0 || en->getDepth() == 1) {
          // Do not keep variables from depth 0, unless we are depth 1 ourselves
          // (which means no FOR in which we are contained)

          if (usedVariableIds.find(vi.first) == usedVariableIds.end()) {
            // variable is not visible to the AggregateBlock
            continue;
          }

          // find variable in the global variable map
          auto itVar = en->_variableMap.find(vi.first);

          if (itVar != en->_variableMap.end()) {
            _variableNames[vi.second.registerId] = (*itVar).second;
          }
        }
      }
    }
    else {
      for (auto const& x : en->_keepVariables) {
        auto it = registerPlan.find(x->id);

        if (it != registerPlan.end()) {
          _variableNames[(*it).second.registerId] = x->name;
        }
      }
    }
  }
}

SortedAggregateBlock::~SortedAggregateBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int SortedAggregateBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // reserve space for the current row
  _currentGroup.initialize(_aggregateRegisters.size());

  return TRI_ERROR_NO_ERROR;
}

int SortedAggregateBlock::getOrSkipSome (size_t atLeast,
                                         size_t atMost,
                                         bool skipping,
                                         AqlItemBlock*& result,
                                         size_t& skipped) {
  TRI_ASSERT(result == nullptr && skipped == 0);
  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  bool const isTotalAggregation = _aggregateRegisters.empty();
  std::unique_ptr<AqlItemBlock> res;

  if (_buffer.empty()) {
    if (! ExecutionBlock::getBlock(atLeast, atMost)) {
      // done
      _done = true;

      if (isTotalAggregation && _currentGroup.groupLength == 0) {
        // total aggregation, but have not yet emitted a group
        res.reset(new AqlItemBlock(1, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));
        emitGroup(nullptr, res.get(), skipped);
        result = res.release();
      }

      return TRI_ERROR_NO_ERROR;
    }
    _pos = 0;           // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();

  if (! skipping) {
    res.reset(new AqlItemBlock(atMost, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

    TRI_ASSERT(cur->getNrRegs() <= res->getNrRegs());
    inheritRegisters(cur, res.get(), _pos);
  }


  while (skipped < atMost) {
    // read the next input row

    bool newGroup = false;
    if (! isTotalAggregation) {
      if (_currentGroup.groupValues[0].isEmpty()) {
        // we never had any previous group
        newGroup = true;
      }
      else {
        // we already had a group, check if the group has changed
        size_t i = 0;

        for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
          int cmp = AqlValue::Compare(
            _trx,
            _currentGroup.groupValues[i],
            _currentGroup.collections[i],
            cur->getValue(_pos, (*it).second),
            cur->getDocumentCollection((*it).second),
            false
          );

          if (cmp != 0) {
            // group change
            newGroup = true;
            break;
          }
          ++i;
        }
      }
    }

    if (newGroup) {
      if (! _currentGroup.groupValues[0].isEmpty()) {
        if (! skipping) {
          // need to emit the current group first
          emitGroup(cur, res.get(), skipped);
        }

        // increase output row count
        ++skipped;

        if (skipped == atMost) {
          // output is full
          // do NOT advance input pointer
          result = res.release();
          return TRI_ERROR_NO_ERROR;
        }
      }

      // still space left in the output to create a new group

      // construct the new group
      size_t i = 0;
      for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
        _currentGroup.groupValues[i] = cur->getValue(_pos, (*it).second).clone();
        _currentGroup.collections[i] = cur->getDocumentCollection((*it).second);
        ++i;
      }
      if (! skipping) {
        _currentGroup.setFirstRow(_pos);
      }
    }

    if (! skipping) {
      _currentGroup.setLastRow(_pos);
    }

    if (++_pos >= cur->size()) {
      _buffer.pop_front();
      _pos = 0;

      bool hasMore = ! _buffer.empty();

      if (! hasMore) {
        try {
          TRI_IF_FAILURE("SortedAggregateBlock::hasMore") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          hasMore = ExecutionBlock::getBlock(atLeast, atMost);
        }
        catch (...) {
          // prevent leak
          delete cur;
          throw;
        }
      }

      if (! hasMore) {
        // no more input. we're done
        try {
          // emit last buffered group
          if (! skipping) {
            TRI_IF_FAILURE("SortedAggregateBlock::getOrSkipSome") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }

            emitGroup(cur, res.get(), skipped);
            ++skipped;
            res->shrink(skipped);
          }
          else {
            ++skipped;
          }
          delete cur;
          _done = true;
          result = res.release();
          return TRI_ERROR_NO_ERROR;
        }
        catch (...) {
          delete cur;
          throw;
        }
      }

      // hasMore

      // move over the last group details into the group before we delete the block
      _currentGroup.addValues(cur, _groupRegister);

      delete cur;
      cur = _buffer.front();
    }
  }

  if (! skipping) {
    TRI_ASSERT(skipped > 0);
    res->shrink(skipped);
  }

  result = res.release();
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the current group data into the result
////////////////////////////////////////////////////////////////////////////////

void SortedAggregateBlock::emitGroup (AqlItemBlock const* cur,
                                      AqlItemBlock* res,
                                      size_t row) {

  if (row > 0) {
    // re-use already copied aqlvalues
    for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
      res->setValue(row, i, res->getValue(0, i));
      // Note: if this throws, then all values will be deleted
      // properly since the first one is.
    }
  }

  size_t i = 0;
  for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {

    if (_currentGroup.groupValues[i].type() == AqlValue::SHAPED) {
      // if a value in the group is a document, it must be converted into its JSON equivalent. the reason is
      // that a group might theoretically consist of multiple documents, from different collections. but there
      // is only one collection pointer per output register
      auto document = cur->getDocumentCollection((*it).second);
      res->setValue(row, (*it).first, AqlValue(new Json(_currentGroup.groupValues[i].toJson(_trx, document, true))));
    }
    else {
      res->setValue(row, (*it).first, _currentGroup.groupValues[i]);
    }
    // ownership of value is transferred into res
    _currentGroup.groupValues[i].erase();
    ++i;
  }

  if (_groupRegister != ExecutionNode::MaxRegisterId) {
    // set the group values
    _currentGroup.addValues(cur, _groupRegister);

    if (static_cast<AggregateNode const*>(_exeNode)->_count) {
      // only set group count in result register
      res->setValue(row, _groupRegister, AqlValue(new Json(static_cast<double>(_currentGroup.groupLength))));
    }
    else if (static_cast<AggregateNode const*>(_exeNode)->_expressionVariable != nullptr) {
      // copy expression result into result register
      res->setValue(row, _groupRegister,
                    AqlValue::CreateFromBlocks(_trx,
                                               _currentGroup.groupBlocks,
                                               _expressionRegister));
    }
    else {
      // copy variables / keep variables into result register
      res->setValue(row, _groupRegister,
                    AqlValue::CreateFromBlocks(_trx,
                                               _currentGroup.groupBlocks,
                                               _variableNames));
    }
  }

  // reset the group so a new one can start
  _currentGroup.reset();
}

// -----------------------------------------------------------------------------
// --SECTION--                                        class HashedAggregateBlock
// -----------------------------------------------------------------------------
        
HashedAggregateBlock::HashedAggregateBlock (ExecutionEngine* engine,
                                            AggregateNode const* en)
  : ExecutionBlock(engine, en),
    _aggregateRegisters(),
    _groupRegister(ExecutionNode::MaxRegisterId) {
 
  for (auto const& p : en->_aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != en->getRegisterPlan()->varInfo.end());

    auto itIn = en->getRegisterPlan()->varInfo.find(p.second->id);
    TRI_ASSERT(itIn != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    _aggregateRegisters.emplace_back(make_pair((*itOut).second.registerId, (*itIn).second.registerId));
  }

  if (en->_outVariable != nullptr) {
    TRI_ASSERT(static_cast<AggregateNode const*>(_exeNode)->_count);

    auto const& registerPlan = en->getRegisterPlan()->varInfo;
    auto it = registerPlan.find(en->_outVariable->id);
    TRI_ASSERT(it != registerPlan.end());
    _groupRegister = (*it).second.registerId;
    TRI_ASSERT(_groupRegister > 0 && _groupRegister < ExecutionNode::MaxRegisterId);
  }
  else {
    TRI_ASSERT(! static_cast<AggregateNode const*>(_exeNode)->_count);
  }

  TRI_ASSERT(! _aggregateRegisters.empty());
}

HashedAggregateBlock::~HashedAggregateBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int HashedAggregateBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
        
  return TRI_ERROR_NO_ERROR;
}

int HashedAggregateBlock::getOrSkipSome (size_t atLeast,
                                         size_t atMost,
                                         bool skipping,
                                         AqlItemBlock*& result,
                                         size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  if (_buffer.empty()) {
    if (! ExecutionBlock::getBlock(atLeast, atMost)) {
      // done
      _done = true;

      return TRI_ERROR_NO_ERROR;
    }
    _pos = 0;           // this is in the first block
  }
  
  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();

  std::vector<TRI_document_collection_t const*> colls;
  for (auto const& it : _aggregateRegisters) {
    colls.emplace_back(cur->getDocumentCollection(it.second));
  }

  std::unordered_map<std::vector<AqlValue>, size_t, GroupKeyHash, GroupKeyEqual> allGroups(
    1024, 
    GroupKeyHash(_trx, colls), 
    GroupKeyEqual(_trx, colls)
  );

  auto buildResult = [&] (AqlItemBlock const* src) {
    auto planNode = static_cast<AggregateNode const*>(getPlanNode());
    auto nrRegs = planNode->getRegisterPlan()->nrRegs[planNode->getDepth()];

    std::unique_ptr<AqlItemBlock> result(new AqlItemBlock(allGroups.size(), nrRegs));
    
    if (src != nullptr) {
      inheritRegisters(src, result.get(), 0);
    }

    size_t const n = _aggregateRegisters.size();
    TRI_ASSERT(colls.size() == n);

    for (size_t i = 0; i < n; ++i) {
      result->setDocumentCollection(_aggregateRegisters[i].first, colls[i]);
    }
    
    TRI_ASSERT(! planNode->_count || _groupRegister != ExecutionNode::MaxRegisterId);

    size_t row = 0;
    for (auto const& it : allGroups) {
      auto& keys = it.first;

      TRI_ASSERT_EXPENSIVE(keys.size() == n);
      size_t i = 0;
      for (auto& key : keys) {
        result->setValue(row, _aggregateRegisters[i++].first, key);
        const_cast<AqlValue*>(&key)->erase(); // to prevent double-freeing later
      }
    
      if (planNode->_count) {
        // set group count in result register
        result->setValue(row, _groupRegister, AqlValue(new Json(static_cast<double>(it.second))));
      }

      ++row;
    }

    return result.release();
  };



  std::vector<AqlValue> groupValues;
  size_t const n = _aggregateRegisters.size();
  groupValues.reserve(n);
      
  std::vector<AqlValue> group;
  group.reserve(n);

  try {
    while (skipped < atMost) {
      groupValues.clear();

      // for hashing simply re-use the aggregate registers, without cloning their contents
      for (size_t i = 0; i < n; ++i) {
        groupValues.emplace_back(cur->getValueReference(_pos, _aggregateRegisters[i].second));
      }

      // now check if we already know this group
      auto it = allGroups.find(groupValues);

      if (it == allGroups.end()) {
        // new group
        group.clear();

        // copy the group values before they get invalidated
        for (size_t i = 0; i < n; ++i) {
          group.emplace_back(cur->getValueReference(_pos, _aggregateRegisters[i].second).clone());
        }

        allGroups.emplace(group, 1);
      }
      else {
        // existing group. simply increase the counter
        (*it).second++;
      }

      if (++_pos >= cur->size()) {
        _buffer.pop_front();
        _pos = 0;

        bool hasMore = ! _buffer.empty();

        if (! hasMore) {
          hasMore = ExecutionBlock::getBlock(atLeast, atMost);
        }

        if (! hasMore) {
          // no more input. we're done
          try {
            // emit last buffered group
            if (! skipping) {
              TRI_IF_FAILURE("HashedAggregateBlock::getOrSkipSome") {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
              }
            }

            ++skipped;
            result = buildResult(cur);
   
            returnBlock(cur);         
            _done = true;
    
            allGroups.clear();  
            groupValues.clear();

            return TRI_ERROR_NO_ERROR;
          }
          catch (...) {
            returnBlock(cur);         
            throw;
          }
        }

        // hasMore

        returnBlock(cur);
        cur = _buffer.front();
      }
    }
  }
  catch (...) {
    // clean up
    for (auto& it : allGroups) {
      for (auto& it2 : it.first) {
        const_cast<AqlValue*>(&it2)->destroy();
      }
    }
    allGroups.clear();
    throw;
  }
  
  allGroups.clear();  
  groupValues.clear();

  if (! skipping) {
    TRI_ASSERT(skipped > 0);
  }

  result = buildResult(nullptr);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hasher for groups
////////////////////////////////////////////////////////////////////////////////

size_t HashedAggregateBlock::GroupKeyHash::operator() (std::vector<AqlValue> const& value) const {
  uint64_t hash = 0x12345678;

  for (size_t i = 0; i < _num; ++i) {
    hash ^= value[i].hash(_trx, _colls[i]);
  }

  return static_cast<size_t>(hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator for groups
////////////////////////////////////////////////////////////////////////////////
    
bool HashedAggregateBlock::GroupKeyEqual::operator() (std::vector<AqlValue> const& lhs,
                                                      std::vector<AqlValue> const& rhs) const {
  size_t const n = lhs.size();

  for (size_t i = 0; i < n; ++i) {
    int res = AqlValue::Compare(_trx, lhs[i], _colls[i], rhs[i], _colls[i], false);

    if (res != 0) {
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   class SortBlock
// -----------------------------------------------------------------------------
        
SortBlock::SortBlock (ExecutionEngine* engine,
                      SortNode const* en)
  : ExecutionBlock(engine, en),
    _sortRegisters(),
    _stable(en->_stable) {
  
  for (auto const& p : en->_elements) {
    auto it = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _sortRegisters.emplace_back(make_pair(it->second.registerId, p.second));
  }
}

SortBlock::~SortBlock () {
}

int SortBlock::initialize () {
  return ExecutionBlock::initialize();
}

int SortBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  // suck all blocks into _buffer
  while (getBlock(DefaultBatchSize, DefaultBatchSize)) {
  }

  if (_buffer.empty()) {
    _done = true;
    return TRI_ERROR_NO_ERROR;
  }

  doSorting();

  _done = false;
  _pos = 0;

  return TRI_ERROR_NO_ERROR;
}

void SortBlock::doSorting () {
  // coords[i][j] is the <j>th row of the <i>th block
  std::vector<std::pair<size_t, size_t>> coords;

  size_t sum = 0;
  for (auto const& block : _buffer) {
    sum += block->size();
  }

  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  coords.reserve(sum);

  // install the coords
  size_t count = 0;

  for (auto const& block : _buffer) {
    for (size_t i = 0; i < block->size(); i++) {
      coords.emplace_back(std::make_pair(count, i));
    }
    count++;
  }

  std::vector<TRI_document_collection_t const*> colls;
  for (RegisterId i = 0; i < _sortRegisters.size(); i++) {
    colls.emplace_back(_buffer.front()->getDocumentCollection(_sortRegisters[i].first));
  }

  // comparison function
  OurLessThan ourLessThan(_trx, _buffer, _sortRegisters, colls);

  // sort coords
  if (_stable) {
    std::stable_sort(coords.begin(), coords.end(), ourLessThan);
  }
  else {
    std::sort(coords.begin(), coords.end(), ourLessThan);
  }

  // here we collect the new blocks (later swapped into _buffer):
  std::deque<AqlItemBlock*> newbuffer;

  try {  // If we throw from here, the catch will delete the new
    // blocks in newbuffer

    count = 0;
    RegisterId const nrregs = _buffer.front()->getNrRegs();

    // install the rearranged values from _buffer into newbuffer

    while (count < sum) {
      size_t sizeNext = (std::min)(sum - count, DefaultBatchSize);
      AqlItemBlock* next = new AqlItemBlock(sizeNext, nrregs);

      try {
        TRI_IF_FAILURE("SortBlock::doSortingInner") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        newbuffer.emplace_back(next);
      }
      catch (...) {
        delete next;
        throw;
      }
      
      std::unordered_map<AqlValue, AqlValue> cache;
      // only copy as much as needed!
      for (size_t i = 0; i < sizeNext; i++) {
        for (RegisterId j = 0; j < nrregs; j++) {
          auto a = _buffer[coords[count].first]->getValue(coords[count].second, j);
          // If we have already dealt with this value for the next
          // block, then we just put the same value again:
          if (! a.isEmpty()) {
            auto it = cache.find(a);

            if (it != cache.end()) {
              AqlValue const& b = it->second;
              // If one of the following throws, all is well, because
              // the new block already has either a copy or stolen
              // the AqlValue:
              _buffer[coords[count].first]->eraseValue(coords[count].second, j);
              next->setValue(i, j, b);
            }
            else {
              // We need to copy a, if it has already been stolen from
              // its original buffer, which we know by looking at the
              // valueCount there.
              auto vCount = _buffer[coords[count].first]->valueCount(a);

              if (vCount == 0) {
                // Was already stolen for another block
                AqlValue b = a.clone();
                try {
                  TRI_IF_FAILURE("SortBlock::doSortingCache") {
                    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                  }
                  cache.emplace(make_pair(a, b));
                }
                catch (...) {
                  b.destroy();
                  throw;
                }

                try {
                  TRI_IF_FAILURE("SortBlock::doSortingNext1") {
                    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                  }
                  next->setValue(i, j, b);
                }
                catch (...) {
                  b.destroy();
                  cache.erase(a);
                  throw;
                }
                // It does not matter whether the following works or not,
                // since the original block keeps its responsibility
                // for a:
                _buffer[coords[count].first]->eraseValue(coords[count].second, j);
              }
              else {
                // Here we are the first to want to inherit a, so we
                // steal it:
                _buffer[coords[count].first]->steal(a);
                // If this has worked, responsibility is now with the
                // new block or indeed with us!
                try {
                  TRI_IF_FAILURE("SortBlock::doSortingNext2") {
                    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                  }
                  next->setValue(i, j, a);
                }
                catch (...) {
                  a.destroy();
                  throw;
                }
                _buffer[coords[count].first]->eraseValue(coords[count].second, j);
                // This might throw as well, however, the responsibility
                // is already with the new block.

                // If the following does not work, we will create a
                // few unnecessary copies, but this does not matter:
                cache.emplace(make_pair(a, a));
              }
            }
          }
        }
        count++;
      }
      cache.clear();
      for (RegisterId j = 0; j < nrregs; j++) {
        next->setDocumentCollection(j, _buffer.front()->getDocumentCollection(j));
      }
    }
  }
  catch (...) {
    for (auto& x : newbuffer) {
      delete x;
    }
    throw;
  }
  _buffer.swap(newbuffer);  // does not throw since allocators
  // are the same
  for (auto& x : newbuffer) {
    delete x;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                      class SortBlock::OurLessThan
// -----------------------------------------------------------------------------

bool SortBlock::OurLessThan::operator() (std::pair<size_t, size_t> const& a,
                                         std::pair<size_t, size_t> const& b) {

  size_t i = 0;
  for (auto const& reg : _sortRegisters) {

    int cmp = AqlValue::Compare(
      _trx,
      _buffer[a.first]->getValueReference(a.second, reg.first),
      _colls[i],
      _buffer[b.first]->getValueReference(b.second, reg.first),
      _colls[i],
      true
    );
    
    if (cmp < 0) {
      return reg.second;
    } 
    else if (cmp > 0) {
      return ! reg.second;
    }
    i++;
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  class LimitBlock
// -----------------------------------------------------------------------------

int LimitBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  return TRI_ERROR_NO_ERROR;
}

int LimitBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  _state = 0;
  _count = 0;
  return TRI_ERROR_NO_ERROR;
}

int LimitBlock::getOrSkipSome (size_t atLeast,
                               size_t atMost,
                               bool skipping,
                               AqlItemBlock*& result,
                               size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_state == 2) {
    return TRI_ERROR_NO_ERROR;
  }

  if (_state == 0) {
    if (_fullCount) {
      // properly initialize fullcount value, which has a default of -1
      if (_engine->_stats.fullCount == -1) {
        _engine->_stats.fullCount = 0;
      }
      _engine->_stats.fullCount += static_cast<int64_t>(_offset);
    }

    if (_offset > 0) {
      ExecutionBlock::_dependencies[0]->skip(_offset);
    }
    _state = 1;
    _count = 0;
    if (_limit == 0 && ! _fullCount) {
      // quick exit for limit == 0
      _state = 2;
      return TRI_ERROR_NO_ERROR;
    }
  }

  // If we get to here, _state == 1 and _count < _limit
  if (_limit > 0) {
    if (atMost > _limit - _count) {
      atMost = _limit - _count;
      if (atLeast > atMost) {
        atLeast = atMost;
      }
    }

    ExecutionBlock::getOrSkipSome(atLeast, atMost, skipping, result, skipped);

    if (skipped == 0) {
      return TRI_ERROR_NO_ERROR;
    }

    _count += skipped; 
    if (_fullCount) {
      _engine->_stats.fullCount += static_cast<int64_t>(skipped);
    }
  }

  if (_count >= _limit) {
    _state = 2;
  
    if (_fullCount) {
      // if fullCount is set, we must fetch all elements from the
      // dependency. we'll use the default batch size for this
      atLeast = DefaultBatchSize; 
      atMost = DefaultBatchSize; 
  
      // suck out all data from the dependencies
      while (true) {
        skipped = 0;
        AqlItemBlock* ignore = nullptr;
        ExecutionBlock::getOrSkipSome(atLeast, atMost, skipping, ignore, skipped);

        if (ignore != nullptr) {
          _engine->_stats.fullCount += static_cast<int64_t>(ignore->size());
          delete ignore;
        }

        if (skipped == 0) {
          break;
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class ReturnBlock
// -----------------------------------------------------------------------------

AqlItemBlock* ReturnBlock::getSome (size_t atLeast,
                                    size_t atMost) {

  std::unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

  if (res.get() == nullptr) {
    return nullptr;
  }
  
  if (_returnInheritedResults) {
    return res.release();
  }

  size_t const n = res->size();

  // Let's steal the actual result and throw away the vars:
  auto ep = static_cast<ReturnNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  std::unique_ptr<AqlItemBlock> stripped(new AqlItemBlock(n, 1));

  for (size_t i = 0; i < n; i++) {
    auto a = res->getValueReference(i, registerId);

    if (! a.isEmpty()) {
      if (a.requiresDestruction()) {
        res->steal(a);

        try {
          TRI_IF_FAILURE("ReturnBlock::getSome") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          stripped->setValue(i, 0, a);
        }
        catch (...) {
          a.destroy();
          throw;
        }
        // If the following does not go well, we do not care, since
        // the value is already stolen and installed in stripped
        res->eraseValue(i, registerId);
      }
      else {
        stripped->setValue(i, 0, a);
      }
    }
  }

  stripped->setDocumentCollection(0, res->getDocumentCollection(registerId));
  delete res.get();
  res.release();

  return stripped.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief make the return block return the results inherited from above, 
/// without creating new blocks
/// returns the id of the register the final result can be found in
////////////////////////////////////////////////////////////////////////////////

RegisterId ReturnBlock::returnInheritedResults () {
  _returnInheritedResults = true;

  auto ep = static_cast<ReturnNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());

  return it->second.registerId;
}

// -----------------------------------------------------------------------------
// --SECTION--                                           class ModificationBlock
// -----------------------------------------------------------------------------

ModificationBlock::ModificationBlock (ExecutionEngine* engine,
                                      ModificationNode const* ep)
  : ExecutionBlock(engine, ep),
    _outRegOld(ExecutionNode::MaxRegisterId),
    _outRegNew(ExecutionNode::MaxRegisterId),
    _collection(ep->_collection),
    _isDBServer(false),
    _usesDefaultSharding(true),
    _buffer(TRI_UNKNOWN_MEM_ZONE) {
  
  auto trxCollection = _trx->trxCollection(_collection->cid());
  if (trxCollection != nullptr) {
    _trx->orderDitch(trxCollection);
  }

  auto const& registerPlan = ep->getRegisterPlan()->varInfo;

  if (ep->_outVariableOld != nullptr) {
    auto it = registerPlan.find(ep->_outVariableOld->id);
    TRI_ASSERT(it != registerPlan.end());
    _outRegOld = (*it).second.registerId;
  }
  if (ep->_outVariableNew != nullptr) {
    auto it = registerPlan.find(ep->_outVariableNew->id);
    TRI_ASSERT(it != registerPlan.end());
    _outRegNew = (*it).second.registerId;
  }
            
  // check if we're a DB server in a cluster
  _isDBServer = triagens::arango::ServerState::instance()->isDBServer();

  if (_isDBServer) {
    _usesDefaultSharding = _collection->usesDefaultSharding();
  }
}

ModificationBlock::~ModificationBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get some - this accumulates all input and calls the work() method
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ModificationBlock::getSome (size_t atLeast,
                                          size_t atMost) {
  std::vector<AqlItemBlock*> blocks;
  std::unique_ptr<AqlItemBlock>replyBlocks;

  auto freeBlocks = [](std::vector<AqlItemBlock*>& blocks) {
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      if ((*it) != nullptr) {
        delete (*it);
      }
    }
    blocks.clear();
  };

  // loop over input until it is exhausted
  try {
    if (static_cast<ModificationNode const*>(_exeNode)->_options.readCompleteInput) {
      // read all input into a buffer first
      while (true) { 
        std::unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

        if (res.get() == nullptr) {
          break;
        }
          
        TRI_IF_FAILURE("ModificationBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
       
        blocks.emplace_back(res.get());
        res.release();
      }

      // now apply the modifications for the complete input
      replyBlocks.reset(work(blocks));
    }
    else {
      // read input in chunks, and process it in chunks
      // this reduces the amount of memory used for storing the input
      while (true) {
        freeBlocks(blocks);
        std::unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

        if (res.get() == nullptr) {
          break;
        }
        
        TRI_IF_FAILURE("ModificationBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
       
        blocks.emplace_back(res.get());
        res.release();

        replyBlocks.reset(work(blocks));

        if (replyBlocks.get() != nullptr) {
          break;
        }
      }
    }
  }
  catch (...) {
    freeBlocks(blocks);
    throw;
  }

  freeBlocks(blocks);
        
  return replyBlocks.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from the AqlValue passed
////////////////////////////////////////////////////////////////////////////////
          
int ModificationBlock::extractKey (AqlValue const& value,
                                   TRI_document_collection_t const* document,
                                   std::string& key) {
  if (value.isShaped()) {
    key = TRI_EXTRACT_MARKER_KEY(value.getMarker());
    return TRI_ERROR_NO_ERROR;
  }

  if (value.isObject()) {
    Json member(value.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_KEY, false, _buffer));

    TRI_json_t const* json = member.json();

    if (TRI_IsStringJson(json)) {
      key.assign(json->_value._string.data, json->_value._string.length - 1);
      return TRI_ERROR_NO_ERROR;
    }
  }
  else if (value.isString()) {
    key = value.toString();
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a master pointer from the marker passed
////////////////////////////////////////////////////////////////////////////////

void ModificationBlock::constructMptr (TRI_doc_mptr_copy_t* dst,
                                       TRI_df_marker_t const* marker) const { 
  dst->_rid = TRI_EXTRACT_MARKER_RID(marker);
  dst->_fid = 0;
  dst->_hash = 0;
  dst->_prev = nullptr;
  dst->_next = nullptr;
  dst->setDataPtr(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a shard key value has changed
////////////////////////////////////////////////////////////////////////////////
                
bool ModificationBlock::isShardKeyChange (TRI_json_t const* oldJson,
                                          TRI_json_t const* newJson,
                                          bool isPatch) const {
  TRI_ASSERT(_isDBServer);

  auto planId = _collection->documentCollection()->_info._planId;
  auto vocbase = static_cast<ModificationNode const*>(_exeNode)->_vocbase;
  return triagens::arango::shardKeysChanged(vocbase->_name, std::to_string(planId), oldJson, newJson, isPatch);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the _key attribute is specified when it must not be
/// specified
////////////////////////////////////////////////////////////////////////////////
              
bool ModificationBlock::isShardKeyError (TRI_json_t const* json) const {
  TRI_ASSERT(_isDBServer);
              
  if (_usesDefaultSharding) {
    return false;
  }

  return (TRI_LookupObjectJson(json, TRI_VOC_ATTRIBUTE_KEY) != nullptr);
}                 

////////////////////////////////////////////////////////////////////////////////
/// @brief process the result of a data-modification operation
////////////////////////////////////////////////////////////////////////////////

void ModificationBlock::handleResult (int code,
                                      bool ignoreErrors,
                                      std::string const* errorMessage) {
  if (code == TRI_ERROR_NO_ERROR) {
    // update the success counter
    ++_engine->_stats.writesExecuted;
    return;
  }
    
  if (ignoreErrors) {
    // update the ignored counter
    ++_engine->_stats.writesIgnored;
    return;
  }
      
  // bubble up the error
  if (errorMessage != nullptr && ! errorMessage->empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(code, *errorMessage);
  }
        
  THROW_ARANGO_EXCEPTION(code);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class RemoveBlock
// -----------------------------------------------------------------------------

RemoveBlock::RemoveBlock (ExecutionEngine* engine,
                          RemoveNode const* ep)
  : ModificationBlock(engine, ep) {
}

RemoveBlock::~RemoveBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for removing data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* RemoveBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);
  
  if (count == 0) {
    return nullptr;
  }
  
  std::unique_ptr<AqlItemBlock> result;

  auto ep = static_cast<RemoveNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  TRI_doc_mptr_copy_t nptr;
  auto trxCollection = _trx->trxCollection(_collection->cid());
  
  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const producesOutput = (ep->_outVariableOld != nullptr);

  result.reset(new AqlItemBlock(count,
                                getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (producesOutput) {
    result->setDocumentCollection(_outRegOld, trxCollection->_collection->_collection);
  }

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);
    auto document = res->getDocumentCollection(registerId);
    
    throwIfKilled(); // check if we were aborted
      
    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, registerId);
    
      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      std::string key;
      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an array. now extract the _key attribute
        errorCode = extractKey(a, document, key);
      }
      else if (a.isString()) {
        // value is a string
        key = a.toChar();
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
        // read "old" version
        if (a.isShaped()) {
          // already have a ShapedJson, no need to fetch the old document again
          constructMptr(&nptr, a.getMarker());
        }
        else {
          // need to fetch the old document
          errorCode = _trx->readSingle(trxCollection, &nptr, key);
        }
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        // no error. we expect to have a key
                  
        // all exceptions are caught in _trx->remove()
        errorCode = _trx->remove(trxCollection, 
                                key,
                                0,
                                TRI_DOC_UPDATE_LAST_WRITE,
                                0, 
                                nullptr,
                                ep->_options.waitForSync);

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && 
            _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          errorCode = TRI_ERROR_NO_ERROR;
        }

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          result->setValue(dstRow,
                           _outRegOld,
                           AqlValue(reinterpret_cast<TRI_df_marker_t const*>(nptr.getDataPtr())));

        }
      }
        
      handleResult(errorCode, ep->_options.ignoreErrors); 
      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class InsertBlock
// -----------------------------------------------------------------------------

InsertBlock::InsertBlock (ExecutionEngine* engine,
                          InsertNode const* ep)
  : ModificationBlock(engine, ep) {
}

InsertBlock::~InsertBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* InsertBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }
  
  std::unique_ptr<AqlItemBlock> result;

  auto ep = static_cast<InsertNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  auto trxCollection = _trx->trxCollection(_collection->cid());

  TRI_doc_mptr_copy_t nptr;
  bool const isEdgeCollection = _collection->isEdgeCollection();
  bool const producesOutput = (ep->_outVariableNew != nullptr);
         
  // initialize an empty edge container
  TRI_document_edge_t edge = { 0, nullptr, 0, nullptr };

  std::string from;
  std::string to;

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (producesOutput) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);
    auto document = res->getDocumentCollection(registerId);
    size_t const n = res->size();
    
    throwIfKilled(); // check if we were aborted
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, registerId);
      
      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an array
        
        if (isEdgeCollection) {
          // array must have _from and _to attributes
          TRI_json_t const* json;

          Json member(a.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_FROM, false, _buffer));
          json = member.json();

          if (TRI_IsStringJson(json)) {
            errorCode = resolve(json->_value._string.data, edge._fromCid, from);
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }
         
          if (errorCode == TRI_ERROR_NO_ERROR) { 
            Json member(a.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_TO, false, _buffer));
            json = member.json();
            if (TRI_IsStringJson(json)) {
              errorCode = resolve(json->_value._string.data, edge._toCid, to);
            }
            else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
            }
          }
        }
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        TRI_doc_mptr_copy_t mptr;
        auto json = a.toJson(_trx, document, false);

        if (isEdgeCollection) {
          // edge
          edge._fromKey = (TRI_voc_key_t) from.c_str();
          edge._toKey = (TRI_voc_key_t) to.c_str();
          errorCode = _trx->create(trxCollection, &mptr, json.json(), &edge, ep->_options.waitForSync);
        }
        else {
          // document
          errorCode = _trx->create(trxCollection, &mptr, json.json(), nullptr, ep->_options.waitForSync);
        }

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          result->setValue(dstRow,
                           _outRegNew,
                           AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors);

      ++dstRow; 
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class UpdateBlock
// -----------------------------------------------------------------------------

UpdateBlock::UpdateBlock (ExecutionEngine* engine,
                          UpdateNode const* ep)
  : ModificationBlock(engine, ep) {
}

UpdateBlock::~UpdateBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* UpdateBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<UpdateNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization
  
  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const producesOutput = (ep->_outVariableOld != nullptr || ep->_outVariableNew != nullptr);

  TRI_doc_mptr_copy_t nptr;
  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  std::string errorMessage;

  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }
  
  auto trxCollection = _trx->trxCollection(_collection->cid());

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (ep->_outVariableOld != nullptr) {
    result->setDocumentCollection(_outRegOld, trxCollection->_collection->_collection);
  }
  if (ep->_outVariableNew != nullptr) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }
         
  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);   // This is intentionally a copy!
    auto document = res->getDocumentCollection(docRegisterId);
    decltype(document) keyDocument = nullptr;

    throwIfKilled(); // check if we were aborted

    if (hasKeyVariable) {
      keyDocument = res->getDocumentCollection(keyRegisterId);
    }

    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, docRegisterId);

      int errorCode = TRI_ERROR_NO_ERROR;
      std::string key;

      if (a.isObject()) {
        // value is an object
        if (hasKeyVariable) {
          // seperate key specification
          AqlValue k = res->getValue(i, keyRegisterId);
          errorCode = extractKey(k, keyDocument, key);
        }
        else {
          errorCode = extractKey(a, document, key);
        }
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        errorMessage += std::string("expecting 'object', got: ") +
          a.getTypeString() + 
          std::string(" while handling: ") + 
          _exeNode->getTypeString();
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        TRI_doc_mptr_copy_t mptr;
        auto json = a.toJson(_trx, document, true);

        // read old document
        TRI_doc_mptr_copy_t oldDocument;
        if (! hasKeyVariable && a.isShaped()) {
          // "old" is already ShapedJson. no need to fetch the old document first
          constructMptr(&oldDocument, a.getMarker());
        }
        else {
          // "old" is no ShapedJson. now fetch old version from database
          errorCode = _trx->readSingle(trxCollection, &oldDocument, key);
        }

        if (! json.isObject()) {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }

        if (errorCode == TRI_ERROR_NO_ERROR) {
          if (oldDocument.getDataPtr() != nullptr) {

            if (json.members() > 0) {
              // only update the document if the update value is not empty
              TRI_shaped_json_t shapedJson;
              TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, oldDocument.getDataPtr()); // PROTECTED by trx here
              std::unique_ptr<TRI_json_t> old(TRI_JsonShapedJson(_collection->documentCollection()->getShaper(), &shapedJson));

              // the default
              errorCode = TRI_ERROR_OUT_OF_MEMORY;

              if (old.get() != nullptr) {
                std::unique_ptr<TRI_json_t> patchedJson(TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old.get(), json.json(), ep->_options.nullMeansRemove, ep->_options.mergeObjects));

                if (patchedJson.get() != nullptr) {
                  // all exceptions are caught in _trx->update()
                  errorCode = _trx->update(trxCollection, key, 0, &mptr, patchedJson.get(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
                }
              }
            }
            else {
              // copy the existing master pointer for OLD into NEW
              mptr = oldDocument;
            }
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
          }
        }

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          if (ep->_outVariableOld != nullptr) {
            // store $OLD
            result->setValue(dstRow,
                             _outRegOld,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(oldDocument.getDataPtr())));
          }

          if (ep->_outVariableNew != nullptr) {
            // store $NEW
            result->setValue(dstRow,
                             _outRegNew,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
          }
        }

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && 
            _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          errorCode = TRI_ERROR_NO_ERROR;
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      
      ++dstRow; 
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class UpsertBlock
// -----------------------------------------------------------------------------

UpsertBlock::UpsertBlock (ExecutionEngine* engine,
                          UpsertNode const* ep)
  : ModificationBlock(engine, ep) {
}

UpsertBlock::~UpsertBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* UpsertBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<UpsertNode const*>(getPlanNode());
  
  auto const& registerPlan = ep->getRegisterPlan()->varInfo;
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  
  it= registerPlan.find(ep->_insertVariable->id);
  TRI_ASSERT(it != registerPlan.end());
  RegisterId const insertRegisterId = it->second.registerId;

  it = registerPlan.find(ep->_updateVariable->id);
  TRI_ASSERT(it != registerPlan.end());
  RegisterId const updateRegisterId = it->second.registerId;
  
  bool const producesOutput = (ep->_outVariableNew != nullptr);
  
  // initialize an empty edge container
  TRI_document_edge_t edge = { 0, nullptr, 0, nullptr };

  std::string from;
  std::string to;

  TRI_doc_mptr_copy_t nptr;
  std::string errorMessage;

  auto trxCollection = _trx->trxCollection(_collection->cid());
  bool const isEdgeCollection = _collection->isEdgeCollection();

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (ep->_outVariableNew != nullptr) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }
         
  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);   // This is intentionally a copy!
    auto document = res->getDocumentCollection(docRegisterId);

    throwIfKilled(); // check if we were aborted

    decltype(document) keyDocument = res->getDocumentCollection(docRegisterId);
    decltype(document) updateDocument = res->getDocumentCollection(updateRegisterId);
    decltype(document) insertDocument = res->getDocumentCollection(insertRegisterId);

    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, docRegisterId);

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      std::string key;
        
      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {

        // old document present => update case
        errorCode = extractKey(a, keyDocument, key);

        if (errorCode == TRI_ERROR_NO_ERROR) {
          AqlValue updateDoc = res->getValue(i, updateRegisterId);

          if (updateDoc.isObject()) {
            auto const updateJson = updateDoc.toJson(_trx, updateDocument, false);
            auto searchJson = a.toJson(_trx, keyDocument, true);

            if (! searchJson.isObject()) {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
            }

            if (errorCode == TRI_ERROR_NO_ERROR && 
                updateJson.isObject()) {
              TRI_doc_mptr_copy_t mptr;
              
              // use default value 
              errorCode = TRI_ERROR_OUT_OF_MEMORY;
              bool wasEmpty = false;

              // check for shard key change
              if (_isDBServer && 
                  isShardKeyChange(searchJson.json(), updateJson.json(), ! ep->_isReplace)) {
                // a shard key value has changed. this is not allowed!
                errorCode = TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
              }
              else if (ep->_isReplace) {
                // replace

                errorCode = _trx->update(trxCollection, key, 0, &mptr, updateJson.json(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
              }
              else {
                // update
                if (updateJson.members() == 0) {
                  // empty object. nothing to do
                  errorCode = TRI_ERROR_NO_ERROR;

                  if (producesOutput) {
                    // copy OLD into NEW
                    result->setValue(dstRow,
                                      _outRegNew,
                                      AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, searchJson.steal())));
                    wasEmpty = true;
                  }
                }
                else {
                  std::unique_ptr<TRI_json_t> mergedJson(TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, searchJson.json(), updateJson.json(), ep->_options.nullMeansRemove, ep->_options.mergeObjects));
              
                  if (mergedJson.get() != nullptr) {
                    // all exceptions are caught in _trx->update()
                    errorCode = _trx->update(trxCollection, key, 0, &mptr, mergedJson.get(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
                  }
                }
              }

              if (producesOutput && 
                  errorCode == TRI_ERROR_NO_ERROR && 
                  ! wasEmpty) {
                // store $NEW
                result->setValue(dstRow,
                                  _outRegNew,
                                  AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
              }
            }
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
          }
        }

      }
      else {
        // no document found => insert case
        AqlValue insertDoc = res->getValue(i, insertRegisterId);

        if (insertDoc.isObject()) {
          if (isEdgeCollection) {
            // array must have _from and _to attributes
            Json member(insertDoc.extractObjectMember(_trx, insertDocument, TRI_VOC_ATTRIBUTE_FROM, false, _buffer));
            TRI_json_t const* json = member.json();

            if (TRI_IsStringJson(json)) {
              errorCode = resolve(json->_value._string.data, edge._fromCid, from);
            }
            else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
            }
          
            if (errorCode == TRI_ERROR_NO_ERROR) { 
              Json member(insertDoc.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_TO, false, _buffer));
              json = member.json();
              if (TRI_IsStringJson(json)) {
                errorCode = resolve(json->_value._string.data, edge._toCid, to);
              }
              else {
                errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
              }
            }
          }

          if (errorCode == TRI_ERROR_NO_ERROR) {
            auto const insertJson = insertDoc.toJson(_trx, insertDocument, true);

            // use default value
            errorCode = TRI_ERROR_OUT_OF_MEMORY;

            if (insertJson.isObject()) {
              // now insert
              if (_isDBServer && isShardKeyError(insertJson.json())) {
                errorCode = TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
              }
              else {
                TRI_doc_mptr_copy_t mptr;

                if (isEdgeCollection) {
                  // edge
                  edge._fromKey = (TRI_voc_key_t) from.c_str();
                  edge._toKey = (TRI_voc_key_t) to.c_str();
                  errorCode = _trx->create(trxCollection, &mptr, insertJson.json(), &edge, ep->_options.waitForSync);
                }
                else {
                  // document
                  errorCode = _trx->create(trxCollection, &mptr, insertJson.json(), nullptr, ep->_options.waitForSync);
                }
          
                if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
                  result->setValue(dstRow,
                                    _outRegNew,
                                    AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
                }
              }
            }
          }

        }
        else {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }

      }

      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      
      ++dstRow; 
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class ReplaceBlock
// -----------------------------------------------------------------------------

ReplaceBlock::ReplaceBlock (ExecutionEngine* engine,
                            ReplaceNode const* ep)
  : ModificationBlock(engine, ep) {
}

ReplaceBlock::~ReplaceBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for replacing data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ReplaceBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }
 
  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<ReplaceNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization
  
  TRI_doc_mptr_copy_t nptr;
          
  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  
  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  auto trxCollection = _trx->trxCollection(_collection->cid());

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (ep->_outVariableOld != nullptr) {
    result->setDocumentCollection(_outRegOld, trxCollection->_collection->_collection);
  }
  if (ep->_outVariableNew != nullptr) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);  // This is intentionally a copy
    auto document = res->getDocumentCollection(registerId);
    decltype(document) keyDocument = nullptr;

    if (hasKeyVariable) {
      keyDocument = res->getDocumentCollection(keyRegisterId);
    }
    
    throwIfKilled(); // check if we were aborted
      
    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, registerId);

      int errorCode = TRI_ERROR_NO_ERROR;
      int readErrorCode = TRI_ERROR_NO_ERROR;
      std::string key;

      if (a.isObject()) {
        // value is an object
        if (hasKeyVariable) {
          // seperate key specification
          AqlValue k = res->getValue(i, keyRegisterId);
          errorCode = extractKey(k, keyDocument, key);
        }
        else {
          errorCode = extractKey(a, document, key);
        }
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (errorCode == TRI_ERROR_NO_ERROR && ep->_outVariableOld != nullptr) {
        if (! hasKeyVariable && a.isShaped()) {
          // "old" is already ShapedJson. no need to fetch the old document first
          constructMptr(&nptr, a.getMarker());
        }
        else {
          // "old" is no ShapedJson. now fetch old version from database
          readErrorCode = _trx->readSingle(trxCollection, &nptr, key);
        }
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        TRI_doc_mptr_copy_t mptr;
        auto const json = a.toJson(_trx, document, true);
          
        // all exceptions are caught in _trx->update()
        errorCode = _trx->update(trxCollection, key, 0, &mptr, json.json(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer) { 
          if (ignoreDocumentNotFound) {
            // Note that this is coded here for the sake of completeness, 
            // but it will intentionally never happen, since this flag is
            // not set in the REPLACE case, because we will always use
            // a DistributeNode rather than a ScatterNode:
            errorCode = TRI_ERROR_NO_ERROR;
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND_OR_SHARDING_ATTRIBUTES_CHANGED;
          }
        }

        if (errorCode == TRI_ERROR_NO_ERROR &&
            readErrorCode == TRI_ERROR_NO_ERROR) {

          if (ep->_outVariableOld != nullptr) {
            result->setValue(dstRow,
                             _outRegOld,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(nptr.getDataPtr())));
          }

          if (ep->_outVariableNew != nullptr) {
            result->setValue(dstRow,
                             _outRegNew,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
          }
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors); 

      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class NoResultsBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor, only call base
////////////////////////////////////////////////////////////////////////////////

int NoResultsBlock::initializeCursor (AqlItemBlock*, size_t) {
  _done = true;
  return TRI_ERROR_NO_ERROR;
}

int NoResultsBlock::getOrSkipSome (size_t,   // atLeast
                                   size_t,   // atMost
                                   bool,     // skipping
                                   AqlItemBlock*& result,
                                   size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class GatherBlock
// -----------------------------------------------------------------------------
        
GatherBlock::GatherBlock (ExecutionEngine* engine,
                          GatherNode const* en)
  : ExecutionBlock(engine, en),
    _sortRegisters(),
    _isSimple(en->getElements().empty()) {

  if (! _isSimple) {
    for (auto const& p : en->getElements()) {
      // We know that planRegisters has been run, so
      // getPlanNode()->_registerPlan is set up
      auto it = en->getRegisterPlan()->varInfo.find(p.first->id);
      TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
      TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
      _sortRegisters.emplace_back(make_pair(it->second.registerId, p.second));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

GatherBlock::~GatherBlock () {
  ENTER_BLOCK
  for (std::deque<AqlItemBlock*>& x : _gatherBlockBuffer) {
    for (AqlItemBlock* y: x) {
      delete y;
    }
    x.clear();
  }
  _gatherBlockBuffer.clear();
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int GatherBlock::initialize () {
  ENTER_BLOCK
  _atDep = 0;
  auto res = ExecutionBlock::initialize();
  
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown: need our own method since our _buffer is different
////////////////////////////////////////////////////////////////////////////////

int GatherBlock::shutdown (int errorCode) {
  ENTER_BLOCK
  // don't call default shutdown method since it does the wrong thing to
  // _gatherBlockBuffer
  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    int res = (*it)->shutdown(errorCode);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  
  if (! _isSimple) {
    for (std::deque<AqlItemBlock*>& x : _gatherBlockBuffer) {
      for (AqlItemBlock* y: x) {
        delete y;
      }
      x.clear();
    }
    _gatherBlockBuffer.clear();
    _gatherBlockPos.clear();
  }
    
  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor
////////////////////////////////////////////////////////////////////////////////

int GatherBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  _atDep = 0;
  
  if (! _isSimple) {
    for (std::deque<AqlItemBlock*>& x : _gatherBlockBuffer) {
      for (AqlItemBlock* y: x) {
        delete y;
      }
      x.clear();
    }
    _gatherBlockBuffer.clear();
    _gatherBlockPos.clear();
    
    _gatherBlockBuffer.reserve(_dependencies.size());
    _gatherBlockPos.reserve(_dependencies.size());
    for(size_t i = 0; i < _dependencies.size(); i++) {
      _gatherBlockBuffer.emplace_back(); 
      _gatherBlockPos.emplace_back(make_pair(i, 0)); 
    }
  }

  _done = false;
  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief count: the sum of the count() of the dependencies or -1 (if any
/// dependency has count -1
////////////////////////////////////////////////////////////////////////////////

int64_t GatherBlock::count () const {
  ENTER_BLOCK
  int64_t sum = 0;
  for (auto const& x: _dependencies) {
    if (x->count() == -1) {
      return -1;
    }
    sum += x->count();
  }
  return sum;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remaining: the sum of the remaining() of the dependencies or -1 (if
/// any dependency has remaining -1
////////////////////////////////////////////////////////////////////////////////

int64_t GatherBlock::remaining () {
  ENTER_BLOCK
  int64_t sum = 0;
  for (auto const& x : _dependencies) {
    if (x->remaining() == -1) {
      return -1;
    }
    sum += x->remaining();
  }
  return sum;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMore: true if any position of _buffer hasMore and false
/// otherwise.
////////////////////////////////////////////////////////////////////////////////

bool GatherBlock::hasMore () {
  ENTER_BLOCK
  if (_done) {
    return false;
  }

  if (_isSimple) {
    for (size_t i = 0; i < _dependencies.size(); i++) {
      if (_dependencies.at(i)->hasMore()) {
        return true;
      }
    }
  }
  else {
    for (size_t i = 0; i < _gatherBlockBuffer.size(); i++) { 
      if (! _gatherBlockBuffer.at(i).empty()) {
        return true;
      } 
      else if (getBlock(i, DefaultBatchSize, DefaultBatchSize)) {
        _gatherBlockPos.at(i) = make_pair(i, 0);
        return true;
      }
    }
  }
  _done = true;
  return false;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* GatherBlock::getSome (size_t atLeast, size_t atMost) {
  ENTER_BLOCK
  if (_done) {
    return nullptr;
  }

  // the simple case . . .  
  if (_isSimple) {
    auto res = _dependencies.at(_atDep)->getSome(atLeast, atMost);
    while (res == nullptr && _atDep < _dependencies.size() - 1) {
      _atDep++;
      res = _dependencies.at(_atDep)->getSome(atLeast, atMost);
    }
    if (res == nullptr) {
      _done = true;
    }
    return res;
  }
 
  // the non-simple case . . .
  size_t available = 0; // nr of available rows
  size_t index = 0;     // an index of a non-empty buffer
  
  // pull more blocks from dependencies . . .
  for (size_t i = 0; i < _dependencies.size(); i++) {
    
    if (_gatherBlockBuffer.at(i).empty()) {
      if (getBlock(i, atLeast, atMost)) {
        index = i;
        _gatherBlockPos.at(i) = make_pair(i, 0);           
      }
    } 
    else {
      index = i;
    }
    
    auto cur = _gatherBlockBuffer.at(i);
    if (! cur.empty()) {
      available += cur.at(0)->size() - _gatherBlockPos.at(i).second;
      for (size_t j = 1; j < cur.size(); j++) {
        available += cur.at(j)->size();
      }
    }
  }
  
  if (available == 0) {
    _done = true;
    return nullptr;
  }
  
  size_t toSend = (std::min)(available, atMost); // nr rows in outgoing block
  
  // get collections for ourLessThan . . .
  std::vector<TRI_document_collection_t const*> colls;
  for (RegisterId i = 0; i < _sortRegisters.size(); i++) {
    colls.emplace_back(_gatherBlockBuffer.at(index).front()->
        getDocumentCollection(_sortRegisters[i].first));
  }
  
  // the following is similar to AqlItemBlock's slice method . . .
  std::unordered_map<AqlValue, AqlValue> cache;
  
  // comparison function 
  OurLessThan ourLessThan(_trx, _gatherBlockBuffer, _sortRegisters, colls);
  AqlItemBlock* example =_gatherBlockBuffer.at(index).front();
  size_t nrRegs = example->getNrRegs();

  std::unique_ptr<AqlItemBlock> res(new AqlItemBlock(toSend,
        static_cast<triagens::aql::RegisterId>(nrRegs)));  
  // automatically deleted if things go wrong
    
  for (RegisterId i = 0; i < nrRegs; i++) {
    res->setDocumentCollection(i, example->getDocumentCollection(i));
  }

  for (size_t i = 0; i < toSend; i++) {
    // get the next smallest row from the buffer . . .
    std::pair<size_t, size_t> val = *(std::min_element(_gatherBlockPos.begin(),
          _gatherBlockPos.end(), ourLessThan));
    
    // copy the row in to the outgoing block . . .
    for (RegisterId col = 0; col < nrRegs; col++) {
      AqlValue const&
        x(_gatherBlockBuffer.at(val.first).front()->getValue(val.second, col));
      if (! x.isEmpty()) {
        auto it = cache.find(x);
        if (it == cache.end()) {
          AqlValue y = x.clone();
          try {
            res->setValue(i, col, y);
          }
          catch (...) {
            y.destroy();
            throw;
          }
          cache.emplace(x, y);
        }
        else {
          res->setValue(i, col, it->second);
        }
      }
    }

    // renew the _gatherBlockPos and clean up the buffer if necessary
    _gatherBlockPos.at(val.first).second++;
    if (_gatherBlockPos.at(val.first).second ==
        _gatherBlockBuffer.at(val.first).front()->size()) {
      AqlItemBlock* cur = _gatherBlockBuffer.at(val.first).front();
      delete cur;
      _gatherBlockBuffer.at(val.first).pop_front();
      _gatherBlockPos.at(val.first) = make_pair(val.first, 0);
    }
  }

  return res.release();
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

size_t GatherBlock::skipSome (size_t atLeast, size_t atMost) {
  ENTER_BLOCK
  if (_done) {
    return 0;
  }

  // the simple case . . .  
  if (_isSimple) {
    auto skipped = _dependencies.at(_atDep)->skipSome(atLeast, atMost);
    while (skipped == 0 && _atDep < _dependencies.size() - 1) {
      _atDep++;
      skipped = _dependencies.at(_atDep)->skipSome(atLeast, atMost);
    }
    if (skipped == 0) {
      _done = true;
    }
    return skipped;
  }

  // the non-simple case . . .
  size_t available = 0; // nr of available rows
  size_t index = 0;     // an index of a non-empty buffer
  TRI_ASSERT(_dependencies.size() != 0); 

  // pull more blocks from dependencies . . .
  for (size_t i = 0; i < _dependencies.size(); i++) {
    if (_gatherBlockBuffer.at(i).empty()) {
      if (getBlock(i, atLeast, atMost)) {
        index = i;
        _gatherBlockPos.at(i) = make_pair(i, 0);           
      }
    } 
    else {
      index = i;
    }

    auto cur = _gatherBlockBuffer.at(i);
    if (! cur.empty()) {
      available += cur.at(0)->size() - _gatherBlockPos.at(i).second;
      for (size_t j = 1; j < cur.size(); j++) {
        available += cur.at(j)->size();
      }
    }
  }
  
  if (available == 0) {
    _done = true;
    return 0;
  }
  
  size_t skipped = (std::min)(available, atMost); //nr rows in outgoing block
  
  // get collections for ourLessThan . . .
  std::vector<TRI_document_collection_t const*> colls;
  for (RegisterId i = 0; i < _sortRegisters.size(); i++) {
    colls.emplace_back(_gatherBlockBuffer.at(index).front()->
        getDocumentCollection(_sortRegisters[i].first));
  }
  
  // comparison function 
  OurLessThan ourLessThan(_trx, _gatherBlockBuffer, _sortRegisters, colls);

  for (size_t i = 0; i < skipped; i++) {
    // get the next smallest row from the buffer . . .
    std::pair<size_t, size_t> val = *(std::min_element(_gatherBlockPos.begin(),
          _gatherBlockPos.end(), ourLessThan));
    
    // renew the _gatherBlockPos and clean up the buffer if necessary
    _gatherBlockPos.at(val.first).second++;
    if (_gatherBlockPos.at(val.first).second ==
        _gatherBlockBuffer.at(val.first).front()->size()) {
      AqlItemBlock* cur = _gatherBlockBuffer.at(val.first).front();
      delete cur;
      _gatherBlockBuffer.at(val.first).pop_front();
      _gatherBlockPos.at(val.first) = make_pair(val.first, 0);
    }
  }

  return skipped;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
/// non-simple case only 
////////////////////////////////////////////////////////////////////////////////

bool GatherBlock::getBlock (size_t i, size_t atLeast, size_t atMost) {
  ENTER_BLOCK
  TRI_ASSERT(i < _dependencies.size());
  TRI_ASSERT(! _isSimple);
  AqlItemBlock* docs = _dependencies.at(i)->getSome(atLeast, atMost);
  if (docs != nullptr) {
    try {
      _gatherBlockBuffer.at(i).emplace_back(docs);
    }
    catch (...) {
      delete docs;
      throw;
    }
    return true;
  }

  return false;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief OurLessThan: comparison method for elements of _gatherBlockPos
////////////////////////////////////////////////////////////////////////////////

bool GatherBlock::OurLessThan::operator() (std::pair<size_t, size_t> const& a,
                                           std::pair<size_t, size_t> const& b) {
  // nothing in the buffer is maximum!
  if (_gatherBlockBuffer.at(a.first).empty()) {
    return false;
  }
  if (_gatherBlockBuffer.at(b.first).empty()) {
    return true;
  }

  size_t i = 0;
  for (auto const& reg : _sortRegisters) {
    int cmp = AqlValue::Compare(
      _trx,
      _gatherBlockBuffer.at(a.first).front()->getValue(a.second, reg.first),
      _colls[i],
      _gatherBlockBuffer.at(b.first).front()->getValue(b.second, reg.first),
      _colls[i],
      true
    );

    if (cmp == -1) {
      return reg.second;
    } 
    else if (cmp == 1) {
      return ! reg.second;
    }
    i++;
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            class BlockWithClients
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

BlockWithClients::BlockWithClients (ExecutionEngine* engine,
                                    ExecutionNode const* ep, 
                                    std::vector<std::string> const& shardIds) 
  : ExecutionBlock(engine, ep), 
    _nrClients(shardIds.size()),
    _ignoreInitCursor(false),
    _ignoreShutdown(false) {

  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
}                                  

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor: reset _doneForClient
////////////////////////////////////////////////////////////////////////////////

int BlockWithClients::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK
  TRI_ASSERT(! _ignoreInitCursor);
  _ignoreInitCursor = true;
  
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

  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown
////////////////////////////////////////////////////////////////////////////////

int BlockWithClients::shutdown (int errorCode) {
  ENTER_BLOCK

  TRI_ASSERT(! _ignoreShutdown);
  _ignoreShutdown = true;

  _doneForClient.clear();

  return ExecutionBlock::shutdown(errorCode);
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSomeForShard
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* BlockWithClients::getSomeForShard (size_t atLeast, 
                                                 size_t atMost, 
                                                 std::string const& shardId) {
  ENTER_BLOCK
  _ignoreInitCursor = false;
  _ignoreShutdown = false;
  size_t skipped = 0;
  AqlItemBlock* result = nullptr;

  int out = getOrSkipSomeForShard(atLeast, atMost, false, result, skipped, shardId);

  if (out != TRI_ERROR_NO_ERROR) {
    if (result != nullptr) {
      delete result;
    }

    THROW_ARANGO_EXCEPTION(out);
  }

  return result;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSomeForShard
////////////////////////////////////////////////////////////////////////////////

size_t BlockWithClients::skipSomeForShard (size_t atLeast, 
                                           size_t atMost, 
                                           std::string const& shardId) {
  ENTER_BLOCK
  _ignoreInitCursor = false;
  _ignoreShutdown = false;
  size_t skipped = 0;
  AqlItemBlock* result = nullptr;
  int out = getOrSkipSomeForShard(atLeast, atMost, true, result, skipped, shardId);
  TRI_ASSERT(result == nullptr);
  if (out != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(out);
  }
  return skipped;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipForShard
////////////////////////////////////////////////////////////////////////////////

bool BlockWithClients::skipForShard (size_t number, 
                                     std::string const& shardId) {
  ENTER_BLOCK
  size_t skipped = skipSomeForShard(number, number, shardId);
  size_t nr = skipped;
  while (nr != 0 && skipped < number) {
    nr = skipSomeForShard(number - skipped, number - skipped, shardId);
    skipped += nr;
  }
  if (nr == 0) {
    return true;
  }
  return ! hasMoreForShard(shardId);
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getClientId: get the number <clientId> (used internally)
/// corresponding to <shardId>
////////////////////////////////////////////////////////////////////////////////

size_t BlockWithClients::getClientId (std::string const& shardId) {
  ENTER_BLOCK
  if (shardId.empty()) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "got empty shard id");
  }

  auto it = _shardIdMap.find(shardId);
  if (it == _shardIdMap.end()) {
    std::string message("AQL: unknown shard id ");
    message.append(shardId);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }
  return ((*it).second);
  LEAVE_BLOCK
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class ScatterBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor
////////////////////////////////////////////////////////////////////////////////

int ScatterBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK
  if (_ignoreInitCursor) {
    return TRI_ERROR_NO_ERROR;
  }
  
  int res = BlockWithClients::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // local clean up
  _posForClient.clear();
  
  for (size_t i = 0; i < _nrClients; i++) {
    _posForClient.emplace_back(std::make_pair(0, 0));
  }
  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor
////////////////////////////////////////////////////////////////////////////////

int ScatterBlock::shutdown (int errorCode) {
  ENTER_BLOCK
  if (_ignoreShutdown) {
    return TRI_ERROR_NO_ERROR;
  }
  
  int res = BlockWithClients::shutdown(errorCode);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // local clean up
  _posForClient.clear();
  
  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMoreForShard: any more for shard <shardId>?
////////////////////////////////////////////////////////////////////////////////

bool ScatterBlock::hasMoreForShard (std::string const& shardId) {
  ENTER_BLOCK
  size_t clientId = getClientId(shardId);

  if (_doneForClient.at(clientId)) {
    return false;
  }

  // TODO is this correct? 
  _ignoreInitCursor = false;
  _ignoreShutdown = false;

  std::pair<size_t,size_t> pos = _posForClient.at(clientId); 
  // (i, j) where i is the position in _buffer, and j is the position in
  // _buffer.at(i) we are sending to <clientId>

  if (pos.first > _buffer.size()) {
    if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
      _doneForClient.at(clientId) = true;
      return false;
    }
  }
  return true;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remainingForShard: remaining for shard, sum of the number of row left
/// in the buffer and _dependencies[0]->remaining()
////////////////////////////////////////////////////////////////////////////////

int64_t ScatterBlock::remainingForShard (std::string const& shardId) {
  ENTER_BLOCK
  size_t clientId = getClientId(shardId);
  if (_doneForClient.at(clientId)) {
    return 0;
  }
  
  int64_t sum = _dependencies[0]->remaining();
  if (sum == -1) {
    return -1;
  }

  std::pair<size_t,size_t> pos = _posForClient.at(clientId);

  if (pos.first <= _buffer.size()) {
    sum += _buffer.at(pos.first)->size() - pos.second;
    for (auto i = pos.first + 1; i < _buffer.size(); i++) {
      sum += _buffer.at(i)->size();
    }
  }

  return sum;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getOrSkipSomeForShard
////////////////////////////////////////////////////////////////////////////////

int ScatterBlock::getOrSkipSomeForShard (size_t atLeast, 
    size_t atMost, bool skipping, AqlItemBlock*& result, 
    size_t& skipped, std::string const& shardId) {
  ENTER_BLOCK
  TRI_ASSERT(0 < atLeast && atLeast <= atMost);
  TRI_ASSERT(result == nullptr && skipped == 0);

  size_t clientId = getClientId(shardId);
  
  if (_doneForClient.at(clientId)) {
    return TRI_ERROR_NO_ERROR;
  }

  std::pair<size_t, size_t> pos = _posForClient.at(clientId); 

  // pull more blocks from dependency if necessary . . . 
  if (pos.first >= _buffer.size()) {
    if (! getBlock(atLeast, atMost)) {
      _doneForClient.at(clientId) = true;
      return TRI_ERROR_NO_ERROR;
    }
  }
  
  size_t available = _buffer.at(pos.first)->size() - pos.second;
  // available should be non-zero  
  
  skipped = (std::min)(available, atMost); //nr rows in outgoing block
  
  if (! skipping) { 
    result = _buffer.at(pos.first)->slice(pos.second, pos.second + skipped);
  }

  // increment the position . . .
  _posForClient.at(clientId).second += skipped;

  // check if we're done at current block in buffer . . .  
  if (_posForClient.at(clientId).second 
      == _buffer.at(_posForClient.at(clientId).first)->size()) {
    _posForClient.at(clientId).first++;
    _posForClient.at(clientId).second = 0;

    // check if we can pop the front of the buffer . . . 
    bool popit = true;
    for (size_t i = 0; i < _nrClients; i++) {
      if (_posForClient.at(i).first == 0) {
        popit = false;
        break;
      }
    }
    if (popit) {
      delete _buffer.front();
      _buffer.pop_front();
      // update the values in first coord of _posForClient
      for (size_t i = 0; i < _nrClients; i++) {
        _posForClient.at(i).first--;
      }

    }
  }

  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

// -----------------------------------------------------------------------------
// --SECTION--                                             class DistributeBlock
// -----------------------------------------------------------------------------

DistributeBlock::DistributeBlock (ExecutionEngine* engine,
                                  DistributeNode const* ep, 
                                  std::vector<std::string> const& shardIds, 
                                  Collection const* collection)
  : BlockWithClients(engine, ep, shardIds), 
    _collection(collection),
    _regId(ExecutionNode::MaxRegisterId),
    _alternativeRegId(ExecutionNode::MaxRegisterId) {
    
  // get the variable to inspect . . .
  VariableId varId = ep->_varId;
  
  // get the register id of the variable to inspect . . .
  auto it = ep->getRegisterPlan()->varInfo.find(varId);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  _regId = (*it).second.registerId;
  
  TRI_ASSERT(_regId < ExecutionNode::MaxRegisterId);

  if (ep->_alternativeVarId != ep->_varId) {
    // use second variable
    auto it = ep->getRegisterPlan()->varInfo.find(ep->_alternativeVarId);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    _alternativeRegId = (*it).second.registerId;
    
    TRI_ASSERT(_alternativeRegId < ExecutionNode::MaxRegisterId);
  }

  _usesDefaultSharding = collection->usesDefaultSharding();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor
////////////////////////////////////////////////////////////////////////////////

int DistributeBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK
  if (_ignoreInitCursor) {
    return TRI_ERROR_NO_ERROR;
  }
  
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
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown
////////////////////////////////////////////////////////////////////////////////

int DistributeBlock::shutdown (int errorCode) {
  ENTER_BLOCK
  if (_ignoreShutdown) {
    return TRI_ERROR_NO_ERROR;
  }
  
  int res = BlockWithClients::shutdown(errorCode);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // local clean up
  _distBuffer.clear();

  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMore: any more for any shard?
////////////////////////////////////////////////////////////////////////////////

bool DistributeBlock::hasMoreForShard (std::string const& shardId) {
  ENTER_BLOCK
 
  size_t clientId = getClientId(shardId);
  if (_doneForClient.at(clientId)) {
    return false;
  }

  // TODO is this correct? 
  _ignoreInitCursor = false;
  _ignoreShutdown = false;
        
  if (! _distBuffer.at(clientId).empty()) {
    return true;
  }

  if (! getBlockForClient(DefaultBatchSize, DefaultBatchSize, clientId)) {
    _doneForClient.at(clientId) = true;
    return false;
  }
  return true;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getOrSkipSomeForShard
////////////////////////////////////////////////////////////////////////////////

int DistributeBlock::getOrSkipSomeForShard (size_t atLeast,
                                            size_t atMost,
                                            bool skipping,
                                            AqlItemBlock*& result,
                                            size_t& skipped,
                                            std::string const& shardId) {
  ENTER_BLOCK
  TRI_ASSERT(0 < atLeast && atLeast <= atMost);
  TRI_ASSERT(result == nullptr && skipped == 0);
  
  size_t clientId = getClientId(shardId);

  if (_doneForClient.at(clientId)) {
    return TRI_ERROR_NO_ERROR;
  }

  std::deque<pair<size_t, size_t>>& buf = _distBuffer.at(clientId);

  vector<AqlItemBlock*> collector;

  auto freeCollector = [&collector]() {
    for (auto& x : collector) {
      delete x;
    }
    collector.clear();
  };

  try {
    if (buf.empty()) {
      if (! getBlockForClient(atLeast, atMost, clientId)) {
        _doneForClient.at(clientId) = true;
        return TRI_ERROR_NO_ERROR;
      }
    }

    skipped = (std::min)(buf.size(), atMost);

    if (skipping) {
      for (size_t i = 0; i < skipped; i++) {
        buf.pop_front();
      }
      freeCollector();
      return TRI_ERROR_NO_ERROR; 
    } 
   
    size_t i = 0;
    while (i < skipped) {
      std::vector<size_t> chosen;
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

      std::unique_ptr<AqlItemBlock> more(_buffer.at(n)->slice(chosen, 0, chosen.size()));
      collector.emplace_back(more.get());
      more.release(); // do not delete it!
    }
  }
  catch (...) {
    freeCollector();
    throw;
  }

  if (! skipping) {
    if (collector.size() == 1) {
      result = collector[0];
      collector.clear();
    }
    else if (! collector.empty()) {
      try {
        result = AqlItemBlock::concatenate(collector);
      }
      catch (...) {
        freeCollector();
        throw;
      }
    }
  }

  freeCollector();
  
  // _buffer is left intact, deleted and cleared at shutdown

  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getBlockForClient: try to get atLeast pairs into
/// _distBuffer.at(clientId), this means we have to look at every row in the
/// incoming blocks until they run out or we find enough rows for clientId. We 
/// also keep track of blocks which should be sent to other clients than the
/// current one. 
////////////////////////////////////////////////////////////////////////////////

bool DistributeBlock::getBlockForClient (size_t atLeast, 
                                         size_t atMost,
                                         size_t clientId) {
  ENTER_BLOCK
  if (_buffer.empty()) {
    _index = 0;         // position in _buffer
    _pos = 0;           // position in _buffer.at(_index)
  }

  std::vector<std::deque<pair<size_t, size_t>>>& buf = _distBuffer;
  // it should be the case that buf.at(clientId) is empty 
  
  while (buf.at(clientId).size() < atLeast) {
    if (_index == _buffer.size()) {
      if (! ExecutionBlock::getBlock(atLeast, atMost)) {
        if (buf.at(clientId).size() == 0) {
          _doneForClient.at(clientId) = true;
          return false;
        }
        break; 
      }
    }

    AqlItemBlock* cur = _buffer.at(_index);
      
    while (_pos < cur->size() && buf.at(clientId).size() < atLeast) {
      // this may modify the input item buffer in place
      size_t id = sendToClient(cur);

      buf.at(id).emplace_back(make_pair(_index, _pos++));
    }

    if (_pos == cur->size()) {
      _pos = 0;
      _index++;
    } 
    else {
      break;
    }
  }
  
  return true;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the JSON that is used to determine the initial shard
////////////////////////////////////////////////////////////////////////////////
  
TRI_json_t const* DistributeBlock::getInputJson (AqlItemBlock const* cur) const {
  auto const& val = cur->getValueReference(_pos, _regId);

  if (val._type != AqlValue::JSON) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "DistributeBlock: can only send JSON");
  }

  TRI_json_t const* json = val._json->json();
  
  if (json != nullptr &&
      TRI_IsNullJson(json) && 
      _alternativeRegId != ExecutionNode::MaxRegisterId) {
    // json is set, but null
    // check if there is a second input register available (UPSERT makes use of two input registers,
    // one for the search document, the other for the insert document)
    auto const& val = cur->getValueReference(_pos, _alternativeRegId);

    if (val._type != AqlValue::JSON) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "DistributeBlock: can only send JSON");
    } 

    json = val._json->json();
  }
    
  if (json == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "json is a nullptr");
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sendToClient: for each row of the incoming AqlItemBlock use the
/// attributes <shardKeys> of the Aql value <val> to determine to which shard
/// the row should be sent and return its clientId
////////////////////////////////////////////////////////////////////////////////

size_t DistributeBlock::sendToClient (AqlItemBlock* cur) {
  ENTER_BLOCK
      
  // inspect cur in row _pos and check to which shard it should be sent . .
  auto json = getInputJson(cur);

  TRI_ASSERT(json != nullptr);

  bool hasCreatedKeyAttribute = false;

  if (TRI_IsStringJson(json)) {
    TRI_json_t* obj = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, 1);

    if (obj == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    TRI_InsertObjectJson(TRI_UNKNOWN_MEM_ZONE, obj, TRI_VOC_ATTRIBUTE_KEY, json);
    // clear the previous value
    cur->destroyValue(_pos, _regId);

    // overwrite with new value
    cur->setValue(_pos, _regId, AqlValue(new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, obj)));

    json = obj;
    hasCreatedKeyAttribute = true;
  }
  else if (! TRI_IsObjectJson(json)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_ASSERT(TRI_IsObjectJson(json));

  if (static_cast<DistributeNode const*>(_exeNode)->_createKeys) {
    // we are responsible for creating keys if none present

    if (_usesDefaultSharding) {
      // the collection is sharded by _key...

      if (! hasCreatedKeyAttribute && 
          TRI_LookupObjectJson(json, TRI_VOC_ATTRIBUTE_KEY) == nullptr) {
        // there is no _key attribute present, so we are responsible for creating one
        std::string const&& keyString(createKey());

        TRI_json_t* obj = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, json);
      
        if (obj == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }

        TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, obj, TRI_VOC_ATTRIBUTE_KEY, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, keyString.c_str(), keyString.size()));
    
        // clear the previous value
        cur->destroyValue(_pos, _regId);

        // overwrite with new value
        cur->setValue(_pos, _regId, AqlValue(new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, obj)));
        json = obj;
      } 
    }
    else {
      // the collection is not sharded by _key

      if (hasCreatedKeyAttribute || 
          TRI_LookupObjectJson(json, TRI_VOC_ATTRIBUTE_KEY) != nullptr) {
        // a _key was given, but user is not allowed to specify _key
        THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY);
      }

      // no _key given. now create one
      std::string const&& keyString(createKey());

      TRI_json_t* obj = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, json);

      if (obj == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, obj, TRI_VOC_ATTRIBUTE_KEY, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, keyString.c_str(), keyString.size()));
        
      // clear the previous value
      cur->destroyValue(_pos, _regId);

      // overwrite with new value
      cur->setValue(_pos, _regId, AqlValue(new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, obj)));
      json = obj;
    }
  }

  // std::cout << "JSON: " << triagens::basics::JsonHelper::toString(json) << "\n";

  std::string shardId;
  bool usesDefaultShardingAttributes;  
  auto clusterInfo = triagens::arango::ClusterInfo::instance();
  auto const planId = triagens::basics::StringUtils::itoa(_collection->getPlanId());

  int res = clusterInfo->getResponsibleShard(planId,
                                             json,
                                             true,
                                             shardId,
                                             usesDefaultShardingAttributes);
  
  // std::cout << "SHARDID: " << shardId << "\n";
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(! shardId.empty());

  return getClientId(shardId); 
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new document key
////////////////////////////////////////////////////////////////////////////////

std::string DistributeBlock::createKey () const {
  ClusterInfo* ci = ClusterInfo::instance();
  uint64_t uid = ci->uniqid();
  return std::to_string(uid);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class RemoteBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief local helper to throw an exception if a HTTP request went wrong
////////////////////////////////////////////////////////////////////////////////

static bool throwExceptionAfterBadSyncRequest (ClusterCommResult* res,
                                               bool isShutdown) {
  ENTER_BLOCK
  if (res->status == CL_COMM_TIMEOUT) {
    std::string errorMessage = std::string("Timeout in communication with shard '") + 
      std::string(res->shardID) + 
      std::string("' on cluster node '") +
      std::string(res->serverID) +
      std::string("' failed.");
    
    // No reply, we give up:
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_TIMEOUT,
                                   errorMessage);
  }

  if (res->status == CL_COMM_ERROR) {
    std::string errorMessage;
    // This could be a broken connection or an Http error:
    if (res->result == nullptr || ! res->result->isComplete()) {
      // there is no result
      errorMessage += std::string("Empty result in communication with shard '") + 
        std::string(res->shardID) + 
        std::string("' on cluster node '") +
        std::string(res->serverID) +
        std::string("'");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_CONNECTION_LOST,
                                     errorMessage);
    }
      
    StringBuffer const& responseBodyBuf(res->result->getBody());
 
    // extract error number and message from response
    int errorNum = TRI_ERROR_NO_ERROR;
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, responseBodyBuf.c_str());

    if (JsonHelper::getBooleanValue(json, "error", true)) {
      errorNum = TRI_ERROR_INTERNAL;
      errorMessage = std::string("Error message received from shard '") + 
        std::string(res->shardID) + 
        std::string("' on cluster node '") +
        std::string(res->serverID) +
        std::string("': ");
    }

    if (TRI_IsObjectJson(json)) {
      TRI_json_t const* v = TRI_LookupObjectJson(json, "errorNum");

      if (TRI_IsNumberJson(v)) {
        if (static_cast<int>(v->_value._number) != TRI_ERROR_NO_ERROR) {
          /* if we've got an error num, error has to be true. */
          TRI_ASSERT(errorNum == TRI_ERROR_INTERNAL);
          errorNum = static_cast<int>(v->_value._number);
        }
      }

      v = TRI_LookupObjectJson(json, "errorMessage");
      if (TRI_IsStringJson(v)) {
        errorMessage += std::string(v->_value._string.data, v->_value._string.length - 1);
      }
      else {
        errorMessage += std::string("(no valid error in response)");
      }
    }
    else {
      errorMessage += std::string("(no valid response)");
    }

    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }

    if (isShutdown && 
        errorNum == TRI_ERROR_QUERY_NOT_FOUND) {
      // this error may happen on shutdown and is thus tolerated
      // pass the info to the caller who can opt to ignore this error
      return true;
    }

    // In this case a proper HTTP error was reported by the DBserver,
    if (errorNum > 0 && ! errorMessage.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(errorNum, errorMessage);
    }

    // default error
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
  }

  return false;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief timeout
////////////////////////////////////////////////////////////////////////////////

double const RemoteBlock::defaultTimeOut = 3600.0;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a remote block
////////////////////////////////////////////////////////////////////////////////
        
RemoteBlock::RemoteBlock (ExecutionEngine* engine,
                          RemoteNode const* en,
                          std::string const& server,
                          std::string const& ownName,
                          std::string const& queryId)
  : ExecutionBlock(engine, en),
    _server(server),
    _ownName(ownName),
    _queryId(queryId) {

  TRI_ASSERT(! queryId.empty());
  TRI_ASSERT_EXPENSIVE((triagens::arango::ServerState::instance()->isCoordinator() && ownName.empty()) ||
                       (! triagens::arango::ServerState::instance()->isCoordinator() && ! ownName.empty()));
}

RemoteBlock::~RemoteBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief local helper to send a request
////////////////////////////////////////////////////////////////////////////////

ClusterCommResult* RemoteBlock::sendRequest (
          triagens::rest::HttpRequest::HttpRequestType type,
          std::string const& urlPart,
          std::string const& body) const {
  ENTER_BLOCK
  ClusterComm* cc = ClusterComm::instance();

  // Later, we probably want to set these sensibly:
  ClientTransactionID const clientTransactionId = "AQL";
  CoordTransactionID const coordTransactionId = TRI_NewTickServer(); //1;
  std::map<std::string, std::string> headers;
  if (! _ownName.empty()) {
    headers.emplace(make_pair("Shard-Id", _ownName));
  }

  auto currentThread = triagens::rest::DispatcherThread::currentDispatcherThread;

  if (currentThread != nullptr) {
    triagens::rest::DispatcherThread::currentDispatcherThread->blockThread();
  }

  auto result = cc->syncRequest(clientTransactionId,
                                coordTransactionId,
                                _server,
                                type,
                                std::string("/_db/") 
                                + triagens::basics::StringUtils::urlEncode(_engine->getQuery()->trx()->vocbase()->_name)
                                + urlPart + _queryId,
                                body,
                                headers,
                                defaultTimeOut);

  if (currentThread != nullptr) {
    triagens::rest::DispatcherThread::currentDispatcherThread->unblockThread();
  }

  return result;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int RemoteBlock::initialize () {
  ENTER_BLOCK
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return TRI_ERROR_NO_ERROR;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor, could be called multiple times
////////////////////////////////////////////////////////////////////////////////

int RemoteBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK
  // For every call we simply forward via HTTP

  Json body(Json::Object, 4);
  if (items == nullptr) {
    // first call, items is still a nullptr
    body("exhausted", Json(true))
        ("error", Json(false));
  }
  else {
    body("pos", Json(static_cast<double>(pos)))
        ("items", items->toJson(_engine->getQuery()->trx()))
        ("exhausted", Json(false))
        ("error", Json(false));
  }

  std::string bodyString(body.toString());

  std::unique_ptr<ClusterCommResult> res;
  res.reset(sendRequest(rest::HttpRequest::HTTP_REQUEST_PUT,
                        "/_api/aql/initializeCursor/",
                        bodyString));
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  Json responseBodyJson(TRI_UNKNOWN_MEM_ZONE,
                        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, 
                                       responseBodyBuf.begin()));
  return JsonHelper::getNumericValue<int>
              (responseBodyJson.json(), "code", TRI_ERROR_INTERNAL);
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, will be called exactly once for the whole query
////////////////////////////////////////////////////////////////////////////////

int RemoteBlock::shutdown (int errorCode) {
  ENTER_BLOCK
  // For every call we simply forward via HTTP

  std::unique_ptr<ClusterCommResult> res;
  res.reset(sendRequest(rest::HttpRequest::HTTP_REQUEST_PUT,
                        "/_api/aql/shutdown/",
                        string("{\"code\":" + std::to_string(errorCode) + "}")));
  if (throwExceptionAfterBadSyncRequest(res.get(), true)) {
    // artificially ignore error in case query was not found during shutdown
    return TRI_ERROR_NO_ERROR;
  }

  StringBuffer const& responseBodyBuf(res->result->getBody());
  Json responseBodyJson(TRI_UNKNOWN_MEM_ZONE,
                        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, 
                                       responseBodyBuf.begin()));

  // read "warnings" attribute if present and add it our query
  if (responseBodyJson.isObject()) {
    auto warnings = responseBodyJson.get("warnings");
    if (warnings.isArray()) {
      auto query = _engine->getQuery();
      for (size_t i = 0; i < warnings.size(); ++i) {
        auto warning = warnings.at(i);
        if (warning.isObject()) {
          auto code = warning.get("code");
          auto message = warning.get("message");
          if (code.isNumber() && message.isString()) {
            query->registerWarning(static_cast<int>(code.json()->_value._number),
                                   message.json()->_value._string.data);
          }
        }
      }
    }
  }

  return JsonHelper::getNumericValue<int>
              (responseBodyJson.json(), "code", TRI_ERROR_INTERNAL);
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* RemoteBlock::getSome (size_t atLeast,
                                    size_t atMost) {
  ENTER_BLOCK
  // For every call we simply forward via HTTP

  Json body(Json::Object, 2);
  body("atLeast", Json(static_cast<double>(atLeast)))
      ("atMost", Json(static_cast<double>(atMost)));
  std::string bodyString(body.toString());

  std::unique_ptr<ClusterCommResult> res;
  res.reset(sendRequest(rest::HttpRequest::HTTP_REQUEST_PUT,
                        "/_api/aql/getSome/",
                        bodyString));
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  Json responseBodyJson(TRI_UNKNOWN_MEM_ZONE,
                        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, 
                                       responseBodyBuf.begin()));

  ExecutionStats newStats(responseBodyJson.get("stats"));
  
  _engine->_stats.addDelta(_deltaStats, newStats);
  _deltaStats = newStats;
  
  if (JsonHelper::getBooleanValue(responseBodyJson.json(), "exhausted", true)) {
    return nullptr;
  }
    
  return new triagens::aql::AqlItemBlock(responseBodyJson);
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

size_t RemoteBlock::skipSome (size_t atLeast, size_t atMost) {
  ENTER_BLOCK
  // For every call we simply forward via HTTP

  Json body(Json::Object, 2);
  body("atLeast", Json(static_cast<double>(atLeast)))
      ("atMost", Json(static_cast<double>(atMost)));
  std::string bodyString(body.toString());

  std::unique_ptr<ClusterCommResult> res;
  res.reset(sendRequest(rest::HttpRequest::HTTP_REQUEST_PUT,
                        "/_api/aql/skipSome/",
                        bodyString));
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  Json responseBodyJson(TRI_UNKNOWN_MEM_ZONE,
                        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, 
                                       responseBodyBuf.begin()));
  if (JsonHelper::getBooleanValue(responseBodyJson.json(), "error", true)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
  }
  size_t skipped = JsonHelper::getNumericValue<size_t>(responseBodyJson.json(),
                                                       "skipped", 0);
  return skipped;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMore
////////////////////////////////////////////////////////////////////////////////

bool RemoteBlock::hasMore () {
  ENTER_BLOCK
  // For every call we simply forward via HTTP
  std::unique_ptr<ClusterCommResult> res;
  res.reset(sendRequest(rest::HttpRequest::HTTP_REQUEST_GET,
                        "/_api/aql/hasMore/",
                        string()));
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  Json responseBodyJson(TRI_UNKNOWN_MEM_ZONE,
                        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, 
                                       responseBodyBuf.begin()));
  if (JsonHelper::getBooleanValue(responseBodyJson.json(), "error", true)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
  }
  return JsonHelper::getBooleanValue(responseBodyJson.json(), "hasMore", true);
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief count
////////////////////////////////////////////////////////////////////////////////

int64_t RemoteBlock::count () const {
  ENTER_BLOCK
  // For every call we simply forward via HTTP
  std::unique_ptr<ClusterCommResult> res;
  res.reset(sendRequest(rest::HttpRequest::HTTP_REQUEST_GET,
                        "/_api/aql/count/",
                        string()));
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  Json responseBodyJson(TRI_UNKNOWN_MEM_ZONE,
                        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, 
                                       responseBodyBuf.begin()));
  if (JsonHelper::getBooleanValue(responseBodyJson.json(), "error", true)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
  }
  return JsonHelper::getNumericValue<int64_t>
               (responseBodyJson.json(), "count", 0);
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remaining
////////////////////////////////////////////////////////////////////////////////

int64_t RemoteBlock::remaining () {
  ENTER_BLOCK
  // For every call we simply forward via HTTP
  std::unique_ptr<ClusterCommResult> res;
  res.reset(sendRequest(rest::HttpRequest::HTTP_REQUEST_GET,
                        "/_api/aql/remaining/",
                        string()));
  throwExceptionAfterBadSyncRequest(res.get(), false);

  // If we get here, then res->result is the response which will be
  // a serialized AqlItemBlock:
  StringBuffer const& responseBodyBuf(res->result->getBody());
  Json responseBodyJson(TRI_UNKNOWN_MEM_ZONE,
                        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, 
                                       responseBodyBuf.begin()));
  if (JsonHelper::getBooleanValue(responseBodyJson.json(), "error", true)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_AQL_COMMUNICATION);
  }
  return JsonHelper::getNumericValue<int64_t>
               (responseBodyJson.json(), "remaining", 0);
  LEAVE_BLOCK
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
