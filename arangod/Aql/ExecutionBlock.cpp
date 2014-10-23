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
#include "Aql/ExecutionEngine.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/json-utilities.h"
#include "HashIndex/hash-index.h"
#include "Utils/Exception.h"
#include "VocBase/edge-collection.h"
#include "VocBase/index.h"
#include "VocBase/vocbase.h"

using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;
using StringBuffer = triagens::basics::StringBuffer;

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define ENTER_BLOCK try { (void) 0;
#define LEAVE_BLOCK } catch (...) { std::cout << "caught an exception in " << __FUNCTION__ << ", " << __FILE__ << ":" << __LINE__ << "!\n"; throw; }
#else
#define ENTER_BLOCK
#define LEAVE_BLOCK
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                            struct AggregatorGroup
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void AggregatorGroup::initialize (size_t capacity) {
  TRI_ASSERT(capacity > 0);

  groupValues.reserve(capacity);
  collections.reserve(capacity);

  for (size_t i = 0; i < capacity; ++i) {
    groupValues[i] = AqlValue();
    collections[i] = nullptr;
  }
}

void AggregatorGroup::reset () {
  for (auto it = groupBlocks.begin(); it != groupBlocks.end(); ++it) {
    delete (*it);
  }
  groupBlocks.clear();
  groupValues[0].erase();
}

void AggregatorGroup::addValues (AqlItemBlock const* src,
                                 RegisterId groupRegister) {
  if (groupRegister == 0) {
    // nothing to do
    return;
  }

  if (rowsAreValid) {
    // emit group details
    TRI_ASSERT(firstRow <= lastRow);

    auto block = src->slice(firstRow, lastRow + 1);
    try {
      groupBlocks.push_back(block);
    }
    catch (...) {
      delete block;
      throw;
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
  for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
    delete *it;
  }
  _buffer.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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
  for (auto d : _dependencies) {
    int res = d->initializeCursor(items, pos);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  for (auto x : _buffer) {
    delete x;
  }
  _buffer.clear();
  _done = false;
  return TRI_ERROR_NO_ERROR;
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

  // Now handle a subquery:
  if ((_exeNode->getType() == ExecutionNode::SUBQUERY) &&
      worker->EnterSubQueryFirst()) {
    auto p = static_cast<SubqueryBlock*>(this);
    if (worker->enterSubquery(this, p->getSubquery())) {
      bool abort = p->getSubquery()->walk(worker);
      worker->leaveSubquery(this, p->getSubquery());
      if (abort) {
        return true;
      }
    }
  }

  // Now the children in their natural order:
  for (auto c : _dependencies) {
    if (c->walk(worker)) {
      return true;
    }
  }
  // Now handle a subquery:
  if ((_exeNode->getType() == ExecutionNode::SUBQUERY) &&
      ! worker->EnterSubQueryFirst()) {
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

  for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
    delete *it;
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
};

////////////////////////////////////////////////////////////////////////////////
/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::inheritRegisters (AqlItemBlock const* src,
                                       AqlItemBlock* dst,
                                       size_t row) {
  RegisterId const n = src->getNrRegs();

  for (RegisterId i = 0; i < n; i++) {
    if (getPlanNode()->_regsToClear.find(i) == 
        getPlanNode()->_regsToClear.end()) {
      if (! src->getValue(row, i).isEmpty()) {
        AqlValue a = src->getValue(row, i).clone();
        try {
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
  AqlItemBlock* docs = _dependencies[0]->getSome(atLeast, atMost);
  if (docs == nullptr) {
    return false;
  }
  try {
    _buffer.push_back(docs);
  }
  catch (...) {
    delete docs;
    throw;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSomeWithoutRegisterClearout, same as above, however, this
/// is the actual worker which does not clear out registers at the end
/// the idea is that somebody who wants to call the generic functionality
/// in a derived class but wants to modify the results before the register
/// cleanup can use this method, internal use only
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ExecutionBlock::getSomeWithoutRegisterClearout (
                                  size_t atLeast, size_t atMost) {
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
  while ( nr != 0 && skipped < number ){
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
  for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
    sum += (*it)->size();
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
  vector<AqlItemBlock*> collector;

  auto freeCollector = [&collector]() {
    for (auto x : collector) {
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
          if (! getBlock(atLeast - skipped,
                         (std::max)(atMost - skipped, DefaultBatchSize))) {
            _done = true;
            break; // must still put things in the result from the collector . . .
          }
          _pos = 0;
        }
      }

      AqlItemBlock* cur = _buffer.front();

      if (cur->size() - _pos > atMost - skipped) {
        // The current block is too large for atMost:
        if (! skipping){
          unique_ptr<AqlItemBlock> more(cur->slice(_pos, _pos + (atMost - skipped)));
          collector.push_back(more.get());
          more.release(); // do not delete it!
        }
        _pos += atMost - skipped;
        skipped = atMost;
      }
      else if (_pos > 0) {
        // The current block fits into our result, but it is already
        // half-eaten:
        if(! skipping){
          unique_ptr<AqlItemBlock> more(cur->slice(_pos, cur->size()));
          collector.push_back(more.get());
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
        if(! skipping){
          collector.push_back(cur);
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
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class SingletonBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor, store a copy of the register values coming from above
////////////////////////////////////////////////////////////////////////////////

int SingletonBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  // Create a deep copy of the register values given to us:
  if (_inputRegisterValues != nullptr) {
    delete _inputRegisterValues;
  }
  if (items != nullptr) {
    _inputRegisterValues = items->slice(pos, pos+1);
  }
  _done = false;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the singleton block
////////////////////////////////////////////////////////////////////////////////

int SingletonBlock::shutdown (int errorCode) {
  int res = ExecutionBlock::shutdown(errorCode);

  if (_inputRegisterValues != nullptr) {
    delete _inputRegisterValues;
    _inputRegisterValues = nullptr;
  }

  return res;
}

int SingletonBlock::getOrSkipSome (size_t,   // atLeast,
                                   size_t,   // atMost,
                                   bool skipping,
                                   AqlItemBlock*& result,
                                   size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  if(! skipping){
    result = new AqlItemBlock(1, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]);
    try {
      if (_inputRegisterValues != nullptr) {
        skipped++;
        for (RegisterId reg = 0; reg < _inputRegisterValues->getNrRegs(); ++reg) {

          AqlValue a = _inputRegisterValues->getValue(0, reg);
          _inputRegisterValues->steal(a);

          try {
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
    _totalCount(0),
    _posInAllDocs(0) {
}

EnumerateCollectionBlock::~EnumerateCollectionBlock () {
}

bool EnumerateCollectionBlock::moreDocuments () {
  if (_documents.empty()) {
    _documents.reserve(DefaultBatchSize);
  }

  _documents.clear();

  int res = _trx->readIncremental(_trx->trxCollection(_collection->cid()),
                                  _documents,
                                  _internalSkip,
                                  static_cast<TRI_voc_size_t>(DefaultBatchSize),
                                  0,
                                  TRI_QRY_NO_LIMIT,
                                  &_totalCount);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  _engine->_stats.scannedFull += static_cast<int64_t>(_documents.size());

  return (! _documents.empty());
}

int EnumerateCollectionBlock::initialize () {
  return ExecutionBlock::initialize();
}

int EnumerateCollectionBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  initializeDocuments();

  if (_totalCount == 0) {
    _done = true;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* EnumerateCollectionBlock::getSome (size_t, // atLeast,
                                                 size_t atMost) {
  if (_done) {
    return nullptr;
  }

  if (_buffer.empty()) {
    if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
      _done = true;
      return nullptr;
    }
    _pos = 0;           // this is in the first block
    _posInAllDocs = 0;  // Note that we know _allDocs.size() > 0,
    // otherwise _done would be true already
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  size_t const curRegs = cur->getNrRegs();

  size_t available = _documents.size() - _posInAllDocs;
  size_t toSend = (std::min)(atMost, available);

  unique_ptr<AqlItemBlock> res(new AqlItemBlock(toSend, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());

  // only copy 1st row of registers inherited from previous frame(s)
  inheritRegisters(cur, res.get(), _pos);

  // set our collection for our output register
  res->setDocumentCollection(static_cast<triagens::aql::RegisterId>(curRegs), _trx->documentCollection(_collection->cid()));

  for (size_t j = 0; j < toSend; j++) {
    if (j > 0) {
      // re-use already copied aqlvalues
      for (RegisterId i = 0; i < curRegs; i++) {
        res->setValue(j, i, res->getValue(0, i));
        // Note: if this throws, then all values will be deleted
        // properly since the first one is.
      }
    }

    // The result is in the first variable of this depth,
    // we do not need to do a lookup in getPlanNode()->_registerPlan->varInfo,
    // but can just take cur->getNrRegs() as registerId:
    res->setValue(j, static_cast<triagens::aql::RegisterId>(curRegs),
                  AqlValue(reinterpret_cast<TRI_df_marker_t
                           const*>(_documents[_posInAllDocs++].getDataPtr())));
    // No harm done, if the setValue throws!
  }

  // Advance read position:
  if (_posInAllDocs >= _documents.size()) {
    // we have exhausted our local documents buffer
    _posInAllDocs = 0;

    // fetch more documents into our buffer
    if (! moreDocuments()) {
      // nothing more to read, re-initialize fetching of documents
      initializeDocuments();
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        delete cur;
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
      if (! getBlock(DefaultBatchSize, DefaultBatchSize)) {
        _done = true;
        return skipped;
      }
      _pos = 0;           // this is in the first block
      _posInAllDocs = 0;  // Note that we know _allDocs.size() > 0,
      // otherwise _done would be true already
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    if (atMost >= skipped + _documents.size() - _posInAllDocs) {
      skipped += _documents.size() - _posInAllDocs;
      _posInAllDocs = 0;

      // fetch more documents into our buffer
      if (! moreDocuments()) {
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
      _posInAllDocs += atMost - skipped;
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
    _allBoundsConstant(true) {
   
  std::vector<std::vector<RangeInfo>> const& orRanges = en->_ranges;
  TRI_ASSERT(en->_index != nullptr);

  TRI_ASSERT(orRanges.size() == 1);  // OR expressions not yet implemented

  // Detect, whether all ranges are constant:
  std::vector<RangeInfo> const& attrRanges = orRanges[0];
  for (auto r : attrRanges) {
    _allBoundsConstant &= r.isConstant();
  }
}

IndexRangeBlock::~IndexRangeBlock () {
  for (auto e : _allVariableBoundExpressions) {
    delete e;
  }
  _allVariableBoundExpressions.clear();
}

int IndexRangeBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res == TRI_ERROR_NO_ERROR) {
    if (_trx->orderBarrier(_trx->trxCollection(_collection->cid())) == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
    }
  }
  
  // Get the ranges from the node:
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  std::vector<std::vector<RangeInfo>> const& orRanges = en->_ranges;
  std::vector<RangeInfo> const& attrRanges = orRanges[0];

  // instanciate expressions:
  auto instanciateExpression = [&] (RangeInfoBound& b) -> void {
    AstNode const* a = b.getExpressionAst(_engine->getQuery()->ast());
    // all new AstNodes are registered with the Ast in the Query
    auto e = new Expression(_engine->getQuery()->ast(), a);
    try {
      _allVariableBoundExpressions.push_back(e);
    }
    catch (...) {
      delete e;
      throw;
    }
    // Prepare _inVars and _inRegs:
    _inVars.emplace_back();
    std::vector<Variable*>& inVarsCur = _inVars.back();
    _inRegs.emplace_back();
    std::vector<RegisterId>& inRegsCur = _inRegs.back();

    std::unordered_set<Variable*> inVars = e->variables();
    for (auto v : inVars) {
      inVarsCur.push_back(v);
      auto it = getPlanNode()->getRegisterPlan()->varInfo.find(v->id);
      TRI_ASSERT(it != getPlanNode()->getRegisterPlan()->varInfo.end());
      TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
      inRegsCur.push_back(it->second.registerId);
    }

  };

  if (! _allBoundsConstant) {
    try {
      for (auto r : attrRanges) {
        for (auto l : r._lows) {
          instanciateExpression(l);
        }
        for (auto h : r._highs) {
          instanciateExpression(h);
        }
      }
    }
    catch (...) {
      for (auto e : _allVariableBoundExpressions) {
        delete e;
      }
      throw;
    }
  }
  else {   // _allBoundsConstant
    readIndex();
  }
  return res;
}

bool IndexRangeBlock::readIndex () {
  // This is either called from initialize if all bounds are constant,
  // in this case it is never called again. If there is at least one
  // variable bound, then readIndex is called once for every item coming
  // in from our dependency. In that case, it is guaranteed that
  //   _buffer   is not empty, in particular _buffer.front() is defined
  //   _pos      points to a position in _buffer.front()
  // Therefore, we can use the register values in _buffer.front() in row
  // _pos to evaluate the variable bounds.
  
  if (_documents.empty()) {
    _documents.reserve(DefaultBatchSize);
  }
  else {
    _documents.clear();
  }

  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  IndexOrCondition const* condition = &en->_ranges;
  
  TRI_ASSERT(en->_index != nullptr);
   
  std::unique_ptr<IndexOrCondition> newCondition;

  // Find out about the actual values for the bounds in the variable bound case:
  if (! _allBoundsConstant) {
    // The following are needed to evaluate expressions with local data from
    // the current incoming item:
    AqlItemBlock* cur = _buffer.front();
    vector<AqlValue>& data(cur->getData());
    vector<TRI_document_collection_t const*>& docColls(cur->getDocumentCollections());
    RegisterId nrRegs = cur->getNrRegs();

    newCondition = std::unique_ptr<IndexOrCondition>(new IndexOrCondition());
    newCondition.get()->push_back(std::vector<RangeInfo>());

    // must have a V8 context here to protect Expression::execute()
    auto engine = _engine;
    triagens::basics::ScopeGuard guard{
      [&engine]() -> void { 
        engine->getQuery()->enterContext(); 
      },
      [&]() -> void {
        
        // must invalidate the expression now as we might be called from
        // different threads
        if (ExecutionEngine::isDBServer()) {
          for (auto e : _allVariableBoundExpressions) {
            e->invalidate();
          }
        }
         
        engine->getQuery()->exitContext(); 
      }
    };

    v8::HandleScope scope; // do not delete this!

    size_t posInExpressions = 0;
    for (auto r : en->_ranges.at(0)) {
      // First create a new RangeInfo containing only the constant 
      // low and high bound of r:
      RangeInfo actualRange(r._var, r._attr, r._lowConst, r._highConst,
                            r.is1ValueRangeInfo());
      // Now work the actual values of the variable lows and highs into 
      // this constant range:
      for (auto l : r._lows) {
        Expression* e = _allVariableBoundExpressions[posInExpressions];
        AqlValue a = e->execute(_trx, docColls, data, nrRegs * _pos,
                                _inVars[posInExpressions],
                                _inRegs[posInExpressions]);
        posInExpressions++;
        if (a._type == AqlValue::JSON) {
          Json json(Json::Array, 3);
          json("include", Json(l.inclusive()))
              ("isConstant", Json(true))
              ("bound", *(a._json));
          a.destroy();  // the TRI_json_t* of a._json has been stolen
          RangeInfoBound b(json);   // Construct from JSON
          actualRange._lowConst.andCombineLowerBounds(b);
        }
        else {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, 
              "AQL: computed a variable bound and got non-JSON");
        }
      }

      for (auto h : r._highs) {
        Expression* e = _allVariableBoundExpressions[posInExpressions];

        TRI_ASSERT(e != nullptr);

        AqlValue a = e->execute(_trx, docColls, data, nrRegs * _pos,
                                _inVars[posInExpressions],
                                _inRegs[posInExpressions]);
        posInExpressions++;
        if (a._type == AqlValue::JSON) {
          Json json(Json::Array, 3);
          json("include", Json(h.inclusive()))
              ("isConstant", Json(true))
              ("bound", *(a._json));
          a.destroy();  // the TRI_json_t* of a._json has been stolen
          RangeInfoBound b(json);   // Construct from JSON
          actualRange._highConst.andCombineUpperBounds(b);
        }
        else {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, 
              "AQL: computed a variable bound and got non-JSON");
        }
      }

      newCondition.get()->at(0).push_back(actualRange);
    }
    
    condition = newCondition.get();
  }
  
  if (en->_index->type == TRI_IDX_TYPE_PRIMARY_INDEX) {
    readPrimaryIndex(*condition);
  }
  else if (en->_index->type == TRI_IDX_TYPE_HASH_INDEX) {
    readHashIndex(*condition);
  }
  else if (en->_index->type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
    readSkiplistIndex(*condition);
  }
  else if (en->_index->type == TRI_IDX_TYPE_EDGE_INDEX) {
    readEdgeIndex(*condition);
  }
  else {
    TRI_ASSERT(false);
  }

  return (!_documents.empty());
}

int IndexRangeBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  _pos = 0;
  _posInDocs = 0;
  
  if (_allBoundsConstant && _documents.size() == 0) {
    _done = true;
  }

  return TRI_ERROR_NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* IndexRangeBlock::getSome (size_t, // atLeast
                                        size_t atMost) {
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
      if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
        _done = true;
        return nullptr;
      }
      _pos = 0;           // this is in the first block

      // This is a new item, so let's read the index if bounds are variable:
      if (! _allBoundsConstant) {
        readIndex();
      }

      _posInDocs = 0;     // position in _documents . . .
    }

    // If we get here, we do have _buffer.front() and _pos points into it
    AqlItemBlock* cur = _buffer.front();
    size_t const curRegs = cur->getNrRegs();

    size_t available = _documents.size() - _posInDocs;
    size_t toSend = (std::min)(atMost, available);

    if (toSend > 0) {

      res.reset(new AqlItemBlock(toSend, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

      // automatically freed should we throw
      TRI_ASSERT(curRegs <= res->getNrRegs());

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(cur, res.get(), _pos);

      // set our collection for our output register
      res->setDocumentCollection(static_cast<triagens::aql::RegisterId>(curRegs), _trx->documentCollection(_collection->cid()));

      for (size_t j = 0; j < toSend; j++) {
        if (j > 0) {
          // re-use already copied aqlvalues
          for (RegisterId i = 0; i < curRegs; i++) {
            res->setValue(j, i, res->getValue(0, i));
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

    // Advance read position:
    if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,

      _posInDocs = 0;

      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        delete cur;
        _pos = 0;
      }

      // let's read the index if bounds are variable:
      if (! _buffer.empty() && ! _allBoundsConstant) {
        readIndex();
      }
      // If _buffer is empty, then we will fetch a new block in the next call
      // and then read the index.
      
    }
  }
  while (res.get() == nullptr);

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
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

  while (skipped < atLeast ){
    if (_buffer.empty()) {
      if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
        _done = true;
        return skipped;
      }
      _pos = 0;           // this is in the first block
      
      // This is a new item, so let's read the index if bounds are variable:
      if (! _allBoundsConstant) {
        readIndex();
      }

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
      if (! _buffer.empty() && ! _allBoundsConstant) {
        readIndex();
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
  TRI_primary_index_t* primaryIndex = &(_collection->documentCollection()->_primaryIndex);
     
  std::string key;
  for (auto x : ranges.at(0)) {
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
          bool const isCluster = (ExecutionEngine::isCoordinator() || ExecutionEngine::isDBServer());
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
      break;
    }
    else if (x._attr == std::string(TRI_VOC_ATTRIBUTE_KEY)) {
      // lookup by _key

      // we can use lower bound because only equality is supported
      TRI_ASSERT(x.is1ValueRangeInfo());
      auto const json = x._lowConst.bound().json();
      if (TRI_IsStringJson(json)) {
        key = std::string(json->_value._string.data, json->_value._string.length - 1);
      }
      break;
    }
  }

  if (! key.empty()) {
    ++_engine->_stats.scannedIndex;

    auto found = static_cast<TRI_doc_mptr_t const*>(TRI_LookupByKeyPrimaryIndex(primaryIndex, key.c_str()));
    if (found != nullptr) {
      _documents.push_back(*found);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents using a hash index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readHashIndex (IndexOrCondition const& ranges) {
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  TRI_index_t* idx = en->_index->data;
  TRI_ASSERT(idx != nullptr);
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;
             
  TRI_shaper_t* shaper = _collection->documentCollection()->getShaper(); 
  TRI_ASSERT(shaper != nullptr);
  
  TRI_index_search_value_t searchValue;
  
  auto destroySearchValue = [&]() {
    if (searchValue._values != nullptr) {
      for (size_t i = 0; i < searchValue._length; ++i) {
        TRI_DestroyShapedJson(shaper->_memoryZone, &searchValue._values[i]);
      }
      TRI_Free(TRI_CORE_MEM_ZONE, searchValue._values);
    }
    searchValue._values = nullptr;
  };

  auto setupSearchValue = [&]() {
    size_t const n = hashIndex->_paths._length;
    searchValue._length = 0;
    searchValue._values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, 
          n * sizeof(TRI_shaped_json_t), true));

    if (searchValue._values == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    
    searchValue._length = n;

    for (size_t i = 0;  i < n;  ++i) {
      TRI_shape_pid_t pid = *(static_cast<TRI_shape_pid_t*>(TRI_AtVector(&hashIndex->_paths, i)));
      TRI_ASSERT(pid != 0);
   
      char const* name = TRI_AttributeNameShapePid(shaper, pid);

      for (auto x : ranges.at(0)) {
        if (x._attr == std::string(name)) {    //found attribute
          auto shaped = TRI_ShapedJsonJson(shaper, x._lowConst.bound().json(), false); 
          // here x->_low->_bound = x->_high->_bound 
          searchValue._values[i] = *shaped;
          TRI_Free(shaper->_memoryZone, shaped);
        }
      }

    }
  };
 
  setupSearchValue();  
  TRI_index_result_t list = TRI_LookupHashIndex(idx, &searchValue);

  destroySearchValue();
  
  size_t const n = list._length;
  try {
    for (size_t i = 0; i < n; ++i) {
      _documents.push_back(*(list._documents[i]));
    }
  
    _engine->_stats.scannedIndex += static_cast<int64_t>(n);
    TRI_DestroyIndexResult(&list);
  }
  catch (...) {
    TRI_DestroyIndexResult(&list);
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents using the edges index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readEdgeIndex (IndexOrCondition const& ranges) {
  TRI_document_collection_t* document = _collection->documentCollection();
     
  std::string key;
  TRI_edge_direction_e direction = TRI_EDGE_IN; // must set a default to satisfy compiler
   
  for (auto x : ranges.at(0)) {
    if (x._attr == std::string(TRI_VOC_ATTRIBUTE_FROM)) {
      // we can use lower bound because only equality is supported
      TRI_ASSERT(x.is1ValueRangeInfo());
      auto const json = x._lowConst.bound().json();
      if (TRI_IsStringJson(json)) {
        // no error will be thrown if _from is not a string
        key = std::string(json->_value._string.data, json->_value._string.length - 1);
        direction = TRI_EDGE_OUT;
      }
      break;
    }
    else if (x._attr == std::string(TRI_VOC_ATTRIBUTE_TO)) {
      // we can use lower bound because only equality is supported
      TRI_ASSERT(x.is1ValueRangeInfo());
      auto const json = x._lowConst.bound().json();
      if (TRI_IsStringJson(json)) {
        // no error will be thrown if _to is not a string
        key = std::string(json->_value._string.data, json->_value._string.length - 1);
        direction = TRI_EDGE_IN;
      }
      break;
    }
  }

  if (! key.empty()) {
    TRI_voc_cid_t documentCid;
    std::string documentKey;

    int errorCode = resolve(key.c_str(), documentCid, documentKey);

    if (errorCode == TRI_ERROR_NO_ERROR) {
      // silently ignore all errors due to wrong _from / _to specifications
      auto&& result = TRI_LookupEdgesDocumentCollection(document, direction, documentCid, (TRI_voc_key_t) documentKey.c_str());
      for (auto it : result) {
        _documents.push_back((it));
      }
  
      _engine->_stats.scannedIndex += static_cast<int64_t>(result.size());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents using a skiplist index
////////////////////////////////////////////////////////////////////////////////

// it is only possible to query a skip list using more than one attribute if we
// only have equalities followed by a single arbitrary comparison (i.e x.a == 1
// && x.b == 2 && x.c > 3 && x.c <= 4). Then we do: 
//
//   TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, left, right, NULL, shaper,
//     NULL, 2, NULL);
//
// where 
//
//   left =  TRI_CreateIndexOperator(TRI_GT_INDEX_OPERATOR, NULL, NULL, [1,2,3],
//     shaper, NULL, 3, NULL)
//
//   right =  TRI_CreateIndexOperator(TRI_LE_INDEX_OPERATOR, NULL, NULL, [1,2,4],
//     shaper, NULL, 3, NULL)
//
// If the final comparison is an equality (x.a == 1 && x.b == 2 && x.c ==3), then 
// we just do:
//
//   TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, NULL, NULL, [1,2,3],
//     shaper, NULL, 3, NULL)
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

void IndexRangeBlock::readSkiplistIndex (IndexOrCondition const& ranges) {
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  TRI_index_t* idx = en->_index->data;
  TRI_ASSERT(idx != nullptr);
  
  TRI_shaper_t* shaper = _collection->documentCollection()->getShaper(); 
  TRI_ASSERT(shaper != nullptr);
  
  TRI_index_operator_t* skiplistOperator = nullptr; 

  Json parameters(Json::List); 
  size_t i = 0;
  for (; i < ranges.at(0).size(); i++) {
    // ranges.at(0) corresponds to a prefix of idx->_fields . . .
    // TODO only doing case with a single OR (i.e. ranges.size()==1 at the
    // moment ...
    auto range = ranges.at(0).at(i);
    TRI_ASSERT(range.isConstant());
    if (range.is1ValueRangeInfo()) {   // it's an equality . . . 
      parameters(range._lowConst.bound().copy());
    } 
    else {                          // it's not an equality and so the final comparison 
      if (parameters.size() != 0) {
        skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr,
            nullptr, parameters.copy().steal(), shaper, nullptr, i, nullptr);
      }
      if (range._lowConst.isDefined()) {
        auto op = range._lowConst.toIndexOperator(false, parameters.copy(), shaper);
        if (skiplistOperator != nullptr) {
          skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
              skiplistOperator, op, nullptr, shaper, nullptr, 2, nullptr);
        } 
        else {
          skiplistOperator = op;
        }
      }
      if (range._highConst.isDefined()) {
        auto op = range._highConst.toIndexOperator(true, parameters.copy(), shaper);
        if (skiplistOperator != nullptr) {
          skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
              skiplistOperator, op, nullptr, shaper, nullptr, 2, nullptr);
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
      Json hass(Json::List);
      hass.add(Json(Json::Null));
      skiplistOperator = TRI_CreateIndexOperator(TRI_GE_INDEX_OPERATOR, nullptr,
          nullptr, hass.steal(), shaper, nullptr, 1, nullptr);
    }
    else {
      skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr,
          nullptr, parameters.steal(), shaper, nullptr, i, nullptr);
    }
  }

  TRI_skiplist_iterator_t* skiplistIterator = TRI_LookupSkiplistIndex(idx, skiplistOperator, en->_reverse);
  //skiplistOperator is deleted by the prev line 
  
  if (skiplistIterator == nullptr) {
    int res = TRI_errno();
    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      return;
    }

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  try {
    while (true) { 
      TRI_skiplist_index_element_t* indexElement = skiplistIterator->next(skiplistIterator);

      if (indexElement == nullptr) {
        break;
      }
      _documents.push_back(*(indexElement->_document));
      ++_engine->_stats.scannedIndex;
    }
    TRI_FreeSkiplistIterator(skiplistIterator);
  }
  catch (...) {
    TRI_FreeSkiplistIterator(skiplistIterator);
    throw;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                          class EnumerateListBlock
// -----------------------------------------------------------------------------
        
EnumerateListBlock::EnumerateListBlock (ExecutionEngine* engine,
                                        EnumerateListNode const* en)
  : ExecutionBlock(engine, en),
    _inVarRegId(ExecutionNode::MaxRegisterId) {

  auto it = en->getRegisterPlan()->varInfo.find(en->_inVariable->id);
  if (it == en->getRegisterPlan()->varInfo.end()){
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
      if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
        _done = true;
        return nullptr;
      }
      _pos = 0;           // this is in the first block
    }

    // if we make it here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    // get the thing we are looping over
    AqlValue inVarReg = cur->getValue(_pos, _inVarRegId);
    size_t sizeInVar = 0; // to shut up compiler

    // get the size of the thing we are looping over
    _collection = nullptr;
    switch (inVarReg._type) {
    case AqlValue::JSON: {
      if(! inVarReg._json->isList()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "EnumerateListBlock: JSON is not a list");
      }
      sizeInVar = inVarReg._json->size();
      break;
    }

    case AqlValue::RANGE: {
      sizeInVar = inVarReg._range->size();
      break;
    }

    case AqlValue::DOCVEC: {
      if( _index == 0) { // this is a (maybe) new DOCVEC
        _DOCVECsize = 0;
        //we require the total number of items

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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "EnumerateListBlock: cannot iterate over shaped value");
    }

    case AqlValue::EMPTY: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "EnumerateListBlock: cannot iterate over empty value");
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
            res->setValue(j, i, res->getValue(0, i));
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
      if (++_pos == cur->size() ) {
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

  while ( skipped < atLeast ) {
    if (_buffer.empty()) {
      if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
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
        if(! inVarReg._json->isList()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "EnumerateListBlock: JSON is not a list");
        }
        sizeInVar = inVarReg._json->size();
        break;
      }
      case AqlValue::RANGE: {
        sizeInVar = inVarReg._range->size();
        break;
      }
      case AqlValue::DOCVEC: {
        if( _index == 0) { // this is a (maybe) new DOCVEC
          _DOCVECsize = 0;
          //we require the total number of items 
          for (size_t i = 0; i < inVarReg._vector->size(); i++) {
            _DOCVECsize += inVarReg._vector->at(i)->size();
          }
        }
        sizeInVar = _DOCVECsize;
        break;
      }
      default: {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "EnumerateListBlock: unexpected type in register");
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

AqlValue EnumerateListBlock::getAqlValue (AqlValue inVarReg) {
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
      if(++_index == (inVarReg._vector->at(_thisblock)->size() + _seen)){
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

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected value in variable to iterate over");
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

  std::unordered_set<Variable*> inVars = _expression->variables();

  for (auto it = inVars.begin(); it != inVars.end(); ++it) {
    _inVars.push_back(*it);
    auto it2 = en->getRegisterPlan()->varInfo.find((*it)->id);

    TRI_ASSERT(it2 != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT(it2->second.registerId < ExecutionNode::MaxRegisterId);
    _inRegs.push_back(it2->second.registerId);
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
}

CalculationBlock::~CalculationBlock () {
}

int CalculationBlock::initialize () {
  return ExecutionBlock::initialize();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief doEvaluation, private helper to do the work
////////////////////////////////////////////////////////////////////////////////

void CalculationBlock::doEvaluation (AqlItemBlock* result) {
  TRI_ASSERT(result != nullptr);

  size_t const n = result->size();
  if (_isReference) {
    // the expression is a reference to a variable only.
    // no need to execute the expression at all
    result->setDocumentCollection(_outReg, result->getDocumentCollection(_inRegs[0]));

    for (size_t i = 0; i < n; i++) {
      // need not clone to avoid a copy, the AqlItemBlock's hash takes
      // care of correct freeing:
      AqlValue a = result->getValue(i, _inRegs[0]);
      try {
        result->setValue(i, _outReg, a);
      }
      catch (...) {
        a.destroy();
        throw;
      }
    }
  }
  else {
    vector<AqlValue>& data(result->getData());
    vector<TRI_document_collection_t const*> docColls(result->getDocumentCollections());

    RegisterId nrRegs = result->getNrRegs();
    result->setDocumentCollection(_outReg, nullptr);

    TRI_ASSERT(_expression != nullptr);

    // must have a V8 context here to protect Expression::execute()
    auto engine = _engine;
    triagens::basics::ScopeGuard guard{
      [&engine]() -> void { 
        engine->getQuery()->enterContext(); 
      },
      [&]() -> void { 
        // must invalidate the expression now as we might be called from
        // different threads
        if (ExecutionEngine::isDBServer()) {
          _expression->invalidate();
        }
        engine->getQuery()->exitContext(); 
      }
    };
    
    v8::HandleScope scope; // do not delete this!

    for (size_t i = 0; i < n; i++) {
      // need to execute the expression
      AqlValue a = _expression->execute(_trx, docColls, data, nrRegs * i, _inVars, _inRegs);
      try {
        result->setValue(i, _outReg, a);
      }
      catch (...) {
        a.destroy();
        throw;
      }
    }
  }
}

AqlItemBlock* CalculationBlock::getSome (size_t atLeast,
                                         size_t atMost) {

  unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(
                                                     atLeast, atMost));

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

SubqueryBlock::~SubqueryBlock () {
}

int SubqueryBlock::initialize () {
  int res = ExecutionBlock::initialize();
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return getSubquery()->initialize();
}

AqlItemBlock* SubqueryBlock::getSome (size_t atLeast,
                                      size_t atMost) {
  unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(
                                                    atLeast, atMost));

  if (res.get() == nullptr) {
    return nullptr;
  }

  for (size_t i = 0; i < res->size(); i++) {
    int ret = _subquery->initializeCursor(res.get(), i);
    if (ret != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(ret);
    }

    auto results = new std::vector<AqlItemBlock*>;
    try {
      do {
        unique_ptr<AqlItemBlock> tmp(_subquery->getSome(DefaultBatchSize, DefaultBatchSize));
        if (tmp.get() == nullptr) {
          break;
        }
        results->push_back(tmp.get());
        tmp.release();
      }
      while(true);
      res->setValue(i, _outReg, AqlValue(results));
    }
    catch (...) {
      for (auto x : *results) {
        delete x;
      }
      delete results;
      throw;
    }
  }
  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
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
        _chosen.push_back(i);
      }
    }

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
  vector<AqlItemBlock*> collector;
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
          unique_ptr<AqlItemBlock> more(cur->slice(_chosen,
                                                   _pos, _pos + (atMost - skipped)));
          collector.push_back(more.get());
          more.release();
        }
        _pos += atMost - skipped;
        skipped = atMost;
      }
      else if (_pos > 0 || _chosen.size() < cur->size()) {
        // The current block fits into our result, but it is already
        // half-eaten or needs to be copied anyway:
        if (! skipping) {
          unique_ptr<AqlItemBlock> more(cur->steal(_chosen, _pos, _chosen.size()));
          collector.push_back(more.get());
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
          collector.push_back(cur);
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
    for (auto c : collector) {
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
        result = AqlItemBlock::concatenate(collector);
      }
      catch (...){
        for (auto x : collector) {
          delete x;
        }
        throw;
      }
      for (auto x : collector) {
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
// --SECTION--                                              class AggregateBlock
// -----------------------------------------------------------------------------
        
AggregateBlock::AggregateBlock (ExecutionEngine* engine,
                                AggregateNode const* en)
  : ExecutionBlock(engine, en),
    _aggregateRegisters(),
    _currentGroup(),
    _groupRegister(ExecutionNode::MaxRegisterId),
    _variableNames() {
  
  for (auto p : en->_aggregateVariables) {
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
    auto it = en->getRegisterPlan()->varInfo.find(en->_outVariable->id);
    TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
    _groupRegister = (*it).second.registerId;
    TRI_ASSERT(_groupRegister > 0 && _groupRegister < ExecutionNode::MaxRegisterId);

    // construct a mapping of all register ids to variable names
    // we need this mapping to generate the grouped output

    for (size_t i = 0; i < en->getRegisterPlan()->varInfo.size(); ++i) {
      _variableNames.push_back(""); // initialize with some default value
    }

    // iterate over all our variables
    for (auto it = en->getRegisterPlan()->varInfo.begin(); 
         it != en->getRegisterPlan()->varInfo.end(); ++it) {
      // find variable in the global variable map
      auto itVar = en->_variableMap.find((*it).first);

      if (itVar != en->_variableMap.end()) {
        _variableNames[(*it).second.registerId] = (*itVar).second;
      }
    }
  }
  else {
    // set groupRegister to 0 if we don't have an out register
    _groupRegister = 0;
  }
}

AggregateBlock::~AggregateBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int AggregateBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // reserve space for the current row
  _currentGroup.initialize(_aggregateRegisters.size());

  return TRI_ERROR_NO_ERROR;
}

int AggregateBlock::getOrSkipSome (size_t atLeast,
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
      _done = true;
      return TRI_ERROR_NO_ERROR;
    }
    _pos = 0;           // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  unique_ptr<AqlItemBlock> res;

  if (! skipping) {
    res.reset(new AqlItemBlock(atMost, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

    TRI_ASSERT(cur->getNrRegs() <= res->getNrRegs());
    inheritRegisters(cur, res.get(), _pos);
  }

  while (skipped < atMost) {
    // read the next input tow

    bool newGroup = false;
    if (_currentGroup.groupValues[0].isEmpty()) {
      // we never had any previous group
      newGroup = true;
    }
    else {
      // we already had a group, check if the group has changed
      size_t i = 0;

      for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
        int cmp = AqlValue::Compare(_trx,
                                    _currentGroup.groupValues[i],
                                    _currentGroup.collections[i],
                                    cur->getValue(_pos, (*it).second),
                                    cur->getDocumentCollection((*it).second));
        if (cmp != 0) {
          // group change
          newGroup = true;
          break;
        }
        ++i;
      }
    }

    if (newGroup) {
      if (! _currentGroup.groupValues[0].isEmpty()) {
        if(! skipping){
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
      if(! skipping){
        _currentGroup.setFirstRow(_pos);
      }
    }
    if(! skipping){
      _currentGroup.setLastRow(_pos);
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
          if(! skipping){
            emitGroup(cur, res.get(), skipped);
            ++skipped;
            TRI_ASSERT(skipped > 0);
            res->shrink(skipped);
          }
          else {
            ++skipped;
          }
          delete cur;
          cur = nullptr;
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

  if(! skipping){
    TRI_ASSERT(skipped > 0);
    res->shrink(skipped);
  }

  result = res.release();
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the current group data into the result
////////////////////////////////////////////////////////////////////////////////

void AggregateBlock::emitGroup (AqlItemBlock const* cur,
                                AqlItemBlock* res,
                                size_t row) {

  size_t i = 0;
  for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
    // FIXME: can throw:
    res->setValue(row, (*it).first, _currentGroup.groupValues[i]);
    ++i;
  }

  if (_groupRegister > 0) {
    // set the group values
    _currentGroup.addValues(cur, _groupRegister);

    res->setValue(row, _groupRegister,
                  AqlValue::CreateFromBlocks(_trx,
                                             _currentGroup.groupBlocks,
                                             _variableNames));
    // FIXME: can throw:
  }

  // reset the group so a new one can start
  _currentGroup.reset();
}


// -----------------------------------------------------------------------------
// --SECTION--                                                   class SortBlock
// -----------------------------------------------------------------------------
        
SortBlock::SortBlock (ExecutionEngine* engine,
                      SortNode const* en)
  : ExecutionBlock(engine, en),
    _sortRegisters(),
    _stable(en->_stable) {
  
  for (auto p : en->_elements) {
    auto it = en->getRegisterPlan()->varInfo.find(p.first->id);
    TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _sortRegisters.push_back(make_pair(it->second.registerId, p.second));
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
  for (auto block : _buffer) {
    sum += block->size();
  }

  coords.reserve(sum);

  // install the coords
  size_t count = 0;

  for (auto block : _buffer) {
    for (size_t i = 0; i < block->size(); i++) {
      coords.push_back(std::make_pair(count, i));
    }
    count++;
  }

  std::vector<TRI_document_collection_t const*> colls;
  for (RegisterId i = 0; i < _sortRegisters.size(); i++) {
    colls.push_back(_buffer.front()->getDocumentCollection(_sortRegisters[i].first));
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
        newbuffer.push_back(next);
      }
      catch (...) {
        delete next;
        throw;
      }
      std::unordered_map<AqlValue, AqlValue> cache;
      // only copy as much as needed!
      for (size_t i = 0; i < sizeNext; i++) {
        for (RegisterId j = 0; j < nrregs; j++) {
          AqlValue a = _buffer[coords[count].first]->getValue(coords[count].second, j);
          // If we have already dealt with this value for the next
          // block, then we just put the same value again:
          if (! a.isEmpty()) {
            auto it = cache.find(a);
            if (it != cache.end()) {
              AqlValue b = it->second;
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
                  cache.insert(make_pair(a,b));
                }
                catch (...) {
                  b.destroy();
                  throw;
                }
                try {
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
                cache.insert(make_pair(a,a));
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
    for (auto x : newbuffer) {
      delete x;
    }
    throw;
  }
  _buffer.swap(newbuffer);  // does not throw since allocators
  // are the same
  for (auto x : newbuffer) {
    delete x;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                      class SortBlock::OurLessThan
// -----------------------------------------------------------------------------

bool SortBlock::OurLessThan::operator() (std::pair<size_t, size_t> const& a,
                                         std::pair<size_t, size_t> const& b) {

  size_t i = 0;
  for (auto reg : _sortRegisters) {

    int cmp = AqlValue::Compare(_trx,
                                _buffer[a.first]->getValue(a.second, reg.first),
                                _colls[i],
                                _buffer[b.first]->getValue(b.second, reg.first),
                                _colls[i]);
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
    if (_offset > 0) {
      ExecutionBlock::_dependencies[0]->skip(_offset);
    }
    _state = 1;
    _count = 0;
    if (_limit == 0) {
      _state = 2;
      return TRI_ERROR_NO_ERROR;
    }
  }

  // If we get to here, _state == 1 and _count < _limit

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
  if (_count >= _limit) {
    _state = 2;
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class ReturnBlock
// -----------------------------------------------------------------------------

AqlItemBlock* ReturnBlock::getSome (size_t atLeast,
                                    size_t atMost) {

  auto res = ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost);

  if (res == nullptr) {
    return res;
  }

  size_t const n = res->size();

  // Let's steal the actual result and throw away the vars:
  auto ep = static_cast<ReturnNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;
  AqlItemBlock* stripped = new AqlItemBlock(n, 1);

  try {
    for (size_t i = 0; i < n; i++) {
      AqlValue a = res->getValue(i, registerId);
      if (! a.isEmpty()) {
        res->steal(a);
        try {
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
    }
  }
  catch (...) {
    delete stripped;
    delete res;
    throw;
  }

  stripped->setDocumentCollection(0, res->getDocumentCollection(registerId));
  delete res;

  return stripped;
}

// -----------------------------------------------------------------------------
// --SECTION--                                           class ModificationBlock
// -----------------------------------------------------------------------------

ModificationBlock::ModificationBlock (ExecutionEngine* engine,
                                      ModificationNode const* ep)
  : ExecutionBlock(engine, ep),
    _collection(ep->_collection) {
}

ModificationBlock::~ModificationBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get some - this accumulates all input and calls the work() method
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ModificationBlock::getSome (size_t atLeast,
                                          size_t atMost) {
  std::vector<AqlItemBlock*> blocks;

  auto freeBlocks = [](std::vector<AqlItemBlock*>& blocks) {
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      if ((*it) != nullptr) {
        delete (*it);
      }
    }
  };

  // loop over input until it is exhausted
  try {
    while (true) { 
      auto res = ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost);

      if (res == nullptr) {
        break;
      }
       
      blocks.push_back(res);
    }

    work(blocks);
    freeBlocks(blocks);

    return nullptr;
  }
  catch (...) {
    freeBlocks(blocks);
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from the AqlValue passed
////////////////////////////////////////////////////////////////////////////////
          
int ModificationBlock::extractKey (AqlValue const& value,
                                   TRI_document_collection_t const* document,
                                   std::string& key) const {
  if (value.isArray()) {
    Json member(value.extractArrayMember(_trx, document, TRI_VOC_ATTRIBUTE_KEY));

    // TODO: allow _id, too
          
    TRI_json_t const* json = member.json();
    if (TRI_IsStringJson(json)) {
      key = std::string(json->_value._string.data, json->_value._string.length - 1);
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
/// @brief process the result of a data-modification operation
////////////////////////////////////////////////////////////////////////////////

void ModificationBlock::handleResult (int code,
                                      bool ignoreErrors,
                                      std::string const *errorMessage) {
  if (code == TRI_ERROR_NO_ERROR) {
    // update the success counter
    ++_engine->_stats.writesExecuted;
  }
  else {
    if (ignoreErrors) {
      // update the ignored counter
      ++_engine->_stats.writesIgnored;
    }
    else {
      // bubble up the error
      if (errorMessage != nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(code, *errorMessage);
      }
      else {
        THROW_ARANGO_EXCEPTION(code);
      }
    }
  }
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

void RemoveBlock::work (std::vector<AqlItemBlock*>& blocks) {
  auto ep = static_cast<RemoveNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  auto trxCollection = _trx->trxCollection(_collection->cid());
      
  if (ep->_outVariable == nullptr) {
    // don't return anything

    // loop over all blocks
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      auto res = (*it);
      auto document = res->getDocumentCollection(registerId);
      
      size_t const n = res->size();
    
      // loop over the complete block
      for (size_t i = 0; i < n; ++i) {
        AqlValue a = res->getValue(i, registerId);

        std::string key;
        int errorCode = TRI_ERROR_NO_ERROR;

        if (a.isArray()) {
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
        }
        
        handleResult(errorCode, ep->_options.ignoreErrors); 
      }
      // done with a block

      // now free it already
      (*it) = nullptr;  
      delete res;
    }
  }
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

void InsertBlock::work (std::vector<AqlItemBlock*>& blocks) {
  auto ep = static_cast<InsertNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  auto trxCollection = _trx->trxCollection(_collection->cid());

  bool const isEdgeCollection = _collection->isEdgeCollection();

  if (ep->_outVariable == nullptr) {
    // don't return anything
         
    // initialize an empty edge container
    TRI_document_edge_t edge;
    edge._fromCid = 0;
    edge._toCid   = 0;
    edge._fromKey = nullptr;
    edge._toKey   = nullptr;

    std::string from;
    std::string to;

    // loop over all blocks
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      auto res = (*it);
      auto document = res->getDocumentCollection(registerId);
      
      size_t const n = res->size();
    
      // loop over the complete block
      for (size_t i = 0; i < n; ++i) {
        AqlValue a = res->getValue(i, registerId);

        int errorCode = TRI_ERROR_NO_ERROR;

        if (a.isArray()) {
          // value is an array
        
          if (isEdgeCollection) {
            // array must have _from and _to attributes
            TRI_json_t const* json;

            Json member(a.extractArrayMember(_trx, document, TRI_VOC_ATTRIBUTE_FROM));
            json = member.json();

            if (TRI_IsStringJson(json)) {
              errorCode = resolve(json->_value._string.data, edge._fromCid, from);
            }
            else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
            }
         
            if (errorCode == TRI_ERROR_NO_ERROR) { 
              Json member(a.extractArrayMember(_trx, document, TRI_VOC_ATTRIBUTE_TO));
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
          auto json = a.toJson(_trx, document);

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
        }

        handleResult(errorCode, ep->_options.ignoreErrors); 
      }
      // done with a block

      // now free it already
      (*it) = nullptr;  
      delete res;
    }
  }
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

void UpdateBlock::work (std::vector<AqlItemBlock*>& blocks) {
  auto ep = static_cast<UpdateNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  std::string errorMessage;
  
  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }
  
  auto* trxCollection = _trx->trxCollection(_collection->cid());

  if (ep->_outVariable == nullptr) {
    // don't return anything
         
    // loop over all blocks
    for (auto& b : blocks) {
      auto* res = b;   // This is intentionally a copy!
      auto document = res->getDocumentCollection(docRegisterId);
      decltype(document) keyDocument = nullptr;

      if (hasKeyVariable) {
        keyDocument = res->getDocumentCollection(keyRegisterId);
      }

      size_t const n = res->size();
    
      // loop over the complete block
      for (size_t i = 0; i < n; ++i) {
        AqlValue a = res->getValue(i, docRegisterId);

        int errorCode = TRI_ERROR_NO_ERROR;
        std::string key;

        if (a.isArray()) {
          // value is an array
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
          errorMessage += std::string("expecting 'array', got: ") +
            a.getTypeString() + 
            std::string(" while handling: ") + 
            _exeNode->getTypeString();
        }

        if (errorCode == TRI_ERROR_NO_ERROR) {
          TRI_doc_mptr_copy_t mptr;
          auto json = a.toJson(_trx, document);

          // read old document
          TRI_doc_mptr_copy_t oldDocument;
          errorCode = _trx->readSingle(trxCollection, &oldDocument, key);

          if (errorCode == TRI_ERROR_NO_ERROR) {
            if (oldDocument.getDataPtr() != nullptr) {
              TRI_shaped_json_t shapedJson;
              TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, oldDocument.getDataPtr()); // PROTECTED by trx here
              TRI_json_t* old = TRI_JsonShapedJson(_collection->documentCollection()->getShaper(), &shapedJson);

              if (old != nullptr) {
                TRI_json_t* patchedJson = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old, json.json(), ep->_options.nullMeansRemove);
                TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, old); 

                if (patchedJson != nullptr) {
                  // all exceptions are caught in _trx->update()
                  errorCode = _trx->update(trxCollection, key, 0, &mptr, patchedJson, TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
                  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, patchedJson);
                }
                else {
                  errorCode = TRI_ERROR_OUT_OF_MEMORY;
                }
              }
              else {
                errorCode = TRI_ERROR_OUT_OF_MEMORY;
              }
            }
            else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
            }
          }
        }

        handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage); 
      }
      // done with a block

      // now free it already
      b = nullptr;  
      delete res;
    }
  }
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

void ReplaceBlock::work (std::vector<AqlItemBlock*>& blocks) {
  auto ep = static_cast<ReplaceNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  
  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  auto trxCollection = _trx->trxCollection(_collection->cid());

  if (ep->_outVariable == nullptr) {
    // don't return anything
         
    // loop over all blocks
    for (auto& b : blocks) {
      auto* res = b;  // This is intentionally a copy
      auto document = res->getDocumentCollection(registerId);
      decltype(document) keyDocument = nullptr;

      if (hasKeyVariable) {
        keyDocument = res->getDocumentCollection(keyRegisterId);
      }
      
      size_t const n = res->size();
    
      // loop over the complete block
      for (size_t i = 0; i < n; ++i) {
        AqlValue a = res->getValue(i, registerId);

        int errorCode = TRI_ERROR_NO_ERROR;
        std::string key;

        if (a.isArray()) {
          // value is an array
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

        if (errorCode == TRI_ERROR_NO_ERROR) {
          TRI_doc_mptr_copy_t mptr;
          auto json = a.toJson(_trx, document);
          
          // all exceptions are caught in _trx->update()
          errorCode = _trx->update(trxCollection, key, 0, &mptr, json.json(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
        }

        handleResult(errorCode, ep->_options.ignoreErrors); 
      }
      // done with a block

      // now free it already
      b = nullptr;  
      delete res;
    }
  }
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
    for (auto p : en->getElements()) {
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
 
  if (!_isSimple) {
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
  for (auto x: _dependencies) {
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
  for (auto x : _dependencies) {
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
      if(_dependencies.at(i)->hasMore()) {
        return true;
      }
    }
  }
  else {
    for (size_t i = 0; i < _gatherBlockBuffer.size(); i++){
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
  size_t index;         // an index of a non-empty buffer
  
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
    colls.push_back(_gatherBlockBuffer.at(index).front()->
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
  size_t index;         // an index of a non-empty buffer
  
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
    colls.push_back(_gatherBlockBuffer.at(index).front()->
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
  TRI_ASSERT(0 <= i && i < _dependencies.size());
  TRI_ASSERT(! _isSimple);
  AqlItemBlock* docs = _dependencies.at(i)->getSome(atLeast, atMost);
  if (docs != nullptr) {
    try {
      _gatherBlockBuffer.at(i).push_back(docs);
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
  for (auto reg : _sortRegisters) {

    int cmp = AqlValue::Compare(
        _trx,
        _gatherBlockBuffer.at(a.first).front()->getValue(a.second, reg.first),
        _colls[i],
        _gatherBlockBuffer.at(b.first).front()->getValue(b.second, reg.first),
        _colls[i]);

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
    _initOrShutdown(true) {

  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
}                                  

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown
////////////////////////////////////////////////////////////////////////////////

int BlockWithClients::shutdown (int errorCode) {
  ENTER_BLOCK
  if (! _initOrShutdown) {
    return TRI_ERROR_NO_ERROR;
  }

  _initOrShutdown = false;
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
  size_t skipped = 0;
  AqlItemBlock* result = nullptr;
  int out = getOrSkipSomeForShard(atLeast, atMost, false, result, skipped, shardId);
  if (out != TRI_ERROR_NO_ERROR) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief preInitCursor: check if we should really init the cursor, and reset
/// _doneForClient
////////////////////////////////////////////////////////////////////////////////

bool BlockWithClients::preInitCursor () {
  ENTER_BLOCK
  if (! _initOrShutdown) {
    return false;
  }

  _doneForClient.clear();
  _doneForClient.reserve(_nrClients);

  for (size_t i = 0; i < _nrClients; i++) {
    _doneForClient.push_back(false);
  }

  _initOrShutdown = false;
  return true;
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
  if (! preInitCursor()) {
    return TRI_ERROR_NO_ERROR;
  }
  
  int res = ExecutionBlock::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _posForClient.clear();
  
  for (size_t i = 0; i < _nrClients; i++) {
    _posForClient.push_back(std::make_pair(0, 0));
  }
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

  std::pair<size_t,size_t> pos = _posForClient.at(clientId); 
  // (i, j) where i is the position in _buffer, and j is the position in
  // _buffer.at(i) we are sending to <clientId>

  if (pos.first > _buffer.size()) {
    _initOrShutdown = true;
    if (! getBlock(DefaultBatchSize, DefaultBatchSize)) {
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
    _initOrShutdown = true;
    if (! getBlock(atLeast, atMost)) {
      _doneForClient.at(clientId) = true;
      return TRI_ERROR_NO_ERROR;
    }
  }
  
  size_t available = _buffer.at(pos.first)->size() - pos.second;
  // available should be non-zero  
  
  skipped = (std::min)(available, atMost); //nr rows in outgoing block
  
  if (! skipping){ 
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
      delete(_buffer.front());
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
    _collection(collection) {
    
  // get the variable to inspect . . .
  VariableId varId = ep->_varId;
  
  // get the register id of the variable to inspect . . .
  auto it = ep->getRegisterPlan()->varInfo.find(varId);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  _regId = (*it).second.registerId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializeCursor
////////////////////////////////////////////////////////////////////////////////

int DistributeBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK
  if (! preInitCursor()) {
    return TRI_ERROR_NO_ERROR;
  }
  
  int res = ExecutionBlock::initializeCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _distBuffer.clear();
  _distBuffer.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _distBuffer.emplace_back();
  }

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
    for (auto x : collector) {
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
      for (size_t i = 0; i < skipped; i++){
        buf.pop_front();
      }
      freeCollector();
      return TRI_ERROR_NO_ERROR; 
    } 
   
    size_t i = 0;
    while (i < skipped) {
      std::vector<size_t> chosen;
      size_t n = buf.front().first;
      while (buf.front().first == n && i < skipped) { 
        chosen.push_back(buf.front().second);
        buf.pop_front();
        i++;
      }
      unique_ptr<AqlItemBlock> more(_buffer.at(n)->slice(chosen, 0, chosen.size()));
      collector.push_back(more.get());
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
      // inspect cur in row _pos and check to which shard it should be sent . .
      size_t id = sendToClient(cur->getValue(_pos, _regId));
      buf.at(id).push_back(make_pair(_index, _pos++));
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
/// @brief sendToClient: for each row of the incoming AqlItemBlock use the
/// attributes <shardKeys> of the Aql value <val> to determine to which shard
/// the row should be sent and return its clientId
////////////////////////////////////////////////////////////////////////////////

size_t DistributeBlock::sendToClient (AqlValue val) {
  ENTER_BLOCK
  TRI_json_t const* json;
  if (val._type == AqlValue::JSON) {
    json = val._json->json();
  } 
  /*
  // TODO: if the DistributeBlock is supposed to run on coordinators only, it
  // cannot have any AqlValues of type SHAPED. These are only present when there
  // are physical collections
  else if (val._type == AqlValue::SHAPED) {
    json = val.toJson(_trx, _collection->documentCollection()).json();
  } 
  */
  else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, 
        "DistributeBlock: can only send JSON or SHAPED");
  }
  
  std::string shardId;
  bool usesDefaultShardingAttributes;  
  auto clusterInfo = triagens::arango::ClusterInfo::instance();
  auto const planId = triagens::basics::StringUtils::itoa(_collection->getPlanId());

  int res = clusterInfo->getResponsibleShard(planId,
                                             json,
                                             true,
                                             shardId,
                                             usesDefaultShardingAttributes);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(!shardId.empty());

  return getClientId(shardId); 
  LEAVE_BLOCK
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
      std::string errorMessage = std::string("Error message received from shard '") + 
        std::string(res->shardID) + 
        std::string("' on cluster node '") +
        std::string(res->serverID) +
        std::string("': ");
    }

    if (TRI_IsArrayJson(json)) {
      TRI_json_t const* v = TRI_LookupArrayJson(json, "errorNum");

      if (TRI_IsNumberJson(v)) {
        if (static_cast<int>(v->_value._number) != TRI_ERROR_NO_ERROR) {
          /* if we've got an error num, error has to be true. */
          TRI_ASSERT(errorNum == TRI_ERROR_INTERNAL);
          errorNum = static_cast<int>(v->_value._number);
        }
      }

      v = TRI_LookupArrayJson(json, "errorMessage");
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
  TRI_ASSERT((ExecutionEngine::isCoordinator() && ownName.empty()) ||
             (! ExecutionEngine::isCoordinator() && ! ownName.empty()));
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
  CoordTransactionID const coordTransactionId = 1;
  std::map<std::string, std::string> headers;
  if (! _ownName.empty()) {
    headers.insert(make_pair("Shard-Id", _ownName));
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

  Json body(Json::Array, 2);
  if (items == nullptr) {
    // first call, items is still a nullptr
    body("exhausted", Json(true))
        ("error", Json(false));
  }
  else {
    body("pos", Json(static_cast<double>(pos)))
      ("items", items->toJson(_engine->getQuery()->trx()));
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
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* RemoteBlock::getSome (size_t atLeast,
                                    size_t atMost) {
  ENTER_BLOCK
  // For every call we simply forward via HTTP

  Json body(Json::Array, 2);
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
    
  auto items = new triagens::aql::AqlItemBlock(responseBodyJson);

  return items;
  LEAVE_BLOCK
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

size_t RemoteBlock::skipSome (size_t atLeast, size_t atMost) {
  ENTER_BLOCK
  // For every call we simply forward via HTTP

  Json body(Json::Array, 2);
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
