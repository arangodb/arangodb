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
#include "Basics/StringUtils.h"
#include "Basics/json-utilities.h"
#include "HashIndex/hash-index.h"
#include "Utils/Exception.h"
#include "VocBase/index.h"
#include "VocBase/vocbase.h"

using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

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
    _trx(engine->getTransaction()), 
    _exeNode(ep), 
    _varOverview(nullptr),
    _depth(0),
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

int ExecutionBlock::initCursor (AqlItemBlock* items, size_t pos) {
  for (auto d : _dependencies) {
    int res = d->initCursor(items, pos);
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

  // Now the children in their natural order:
  for (auto c : _dependencies) {
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
/// @brief static analysis
////////////////////////////////////////////////////////////////////////////////

struct StaticAnalysisDebugger : public WalkerWorker<ExecutionBlock> {
  StaticAnalysisDebugger () : indent(0) {};
  ~StaticAnalysisDebugger () {};

  int indent;

  bool enterSubquery (ExecutionBlock* super, ExecutionBlock* sub) {
    indent++;
    return true;
  }

  void leaveSubquery (ExecutionBlock* super, ExecutionBlock* sub) {
    indent--;
  }

  void after (ExecutionBlock* eb) {
    ExecutionNode const* ep = eb->getPlanNode();
    for (int i = 0; i < indent; i++) {
      std::cout << " ";
    }
    std::cout << ep->getTypeString() << " ";
    std::cout << "regsUsedHere: ";
    for (auto v : ep->getVariablesUsedHere()) {
      std::cout << eb->_varOverview->varInfo.find(v->id)->second.registerId
                << " ";
    }
    std::cout << "regsSetHere: ";
    for (auto v : ep->getVariablesSetHere()) {
      std::cout << eb->_varOverview->varInfo.find(v->id)->second.registerId
                << " ";
    }
    std::cout << "regsToClear: ";
    for (auto r : eb->_regsToClear) {
      std::cout << r << " ";
    }
    std::cout << std::endl;
  }
};

void ExecutionBlock::staticAnalysis (ExecutionBlock* super) {
  // The super is only for the case of subqueries.
  shared_ptr<VarOverview> v;
  if (super == nullptr) {
    v.reset(new VarOverview());
  }
  else {
    v.reset(new VarOverview(*(super->_varOverview), super->_depth));
  }
  v->setSharedPtr(&v);
  walk(v.get());
  // Now handle the subqueries:
  for (auto s : v->subQueryBlocks) {
    auto sq = static_cast<SubqueryBlock*>(s);
    sq->getSubquery()->staticAnalysis(s);
  }
  v->reset();

  // Just for debugging:
  /*
  std::cout << std::endl;
  StaticAnalysisDebugger debugger;
  walk(&debugger);
  std::cout << std::endl;
  */
}

int ExecutionBlock::initialize () {
  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    int res = (*it)->initialize();
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

int ExecutionBlock::shutdown () {
  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    int res = (*it)->shutdown();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
    delete *it;
  }
  _buffer.clear();
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         protected
// -----------------------------------------------------------------------------

void ExecutionBlock::inheritRegisters (AqlItemBlock const* src,
                                       AqlItemBlock* dst,
                                       size_t row) {
  RegisterId const n = src->getNrRegs();

  for (RegisterId i = 0; i < n; i++) {
    if (_regsToClear.find(i) == _regsToClear.end()) {
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

AqlItemBlock* ExecutionBlock::getSome(size_t atLeast, size_t atMost) {
  std::unique_ptr<AqlItemBlock> result(getSomeWithoutRegisterClearout(atLeast, atMost));
  clearRegisters(result.get());
  return result.release();
}

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
    result->clearRegisters(_regsToClear);
  }
}

size_t ExecutionBlock::skipSome (size_t atLeast, size_t atMost) {
  std::cout << "ExecutionBlock::skipSome\n";
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
  std::cout << "ExecutionBlock::skip\n";
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
  
  std::cout << "ExecutionBlock::getOrSkipSome\n";
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
                         std::max(atMost - skipped, DefaultBatchSize))) {
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
/// @brief initCursor, store a copy of the register values coming from above
////////////////////////////////////////////////////////////////////////////////

int SingletonBlock::initCursor (AqlItemBlock* items, size_t pos) {
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

int SingletonBlock::shutdown () {
  int res = ExecutionBlock::shutdown();
  if (_inputRegisterValues != nullptr) {
    delete _inputRegisterValues;
    _inputRegisterValues = nullptr;
  }
  return res;
}

int SingletonBlock::getOrSkipSome (size_t atLeast,
                                   size_t atMost,
                                   bool skipping,
                                   AqlItemBlock*& result,
                                   size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return TRI_ERROR_NO_ERROR;
  }

  if(! skipping){
    result = new AqlItemBlock(1, _varOverview->nrRegs[_depth]);
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

  return (! _documents.empty());
}

int EnumerateCollectionBlock::initialize () {
  return ExecutionBlock::initialize();
}

int EnumerateCollectionBlock::initCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  initDocuments();

  if (_totalCount == 0) {
    _done = true;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* EnumerateCollectionBlock::getSome (size_t atLeast,
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
  size_t toSend = std::min(atMost, available);

  unique_ptr<AqlItemBlock> res(new AqlItemBlock(toSend, _varOverview->nrRegs[_depth]));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());

  // only copy 1st row of registers inherited from previous frame(s)
  inheritRegisters(cur, res.get(), _pos);

  // set our collection for our output register
  res->setDocumentCollection(curRegs, _trx->documentCollection(_collection->cid()));

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
    // we do not need to do a lookup in _varOverview->varInfo,
    // but can just take cur->getNrRegs() as registerId:
    res->setValue(j, curRegs,
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
      initDocuments();
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
        initDocuments();
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
                                  IndexRangeNode const* ep)
  : ExecutionBlock(engine, ep),
    _collection(ep->_collection),
    _posInDocs(0) {
  
   std::cout << "USING INDEX: " << ep->_index->_iid << ", " <<
     TRI_TypeNameIndex(ep->_index->_type) << "\n";

}

IndexRangeBlock::~IndexRangeBlock () {
}

bool IndexRangeBlock::readIndex () {

  if (_documents.empty()) {
    _documents.reserve(DefaultBatchSize);
  }

  _documents.clear();
  
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
 
    
  if (en->_index->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
    readSkiplistIndex();
  }
  else if (en->_index->_type == TRI_IDX_TYPE_HASH_INDEX) {
    readHashIndex();
  }
  else {
    TRI_ASSERT(false);
  }
  return (!_documents.empty());
}

int IndexRangeBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res == TRI_ERROR_NO_ERROR) {
    if (_trx->orderBarrier(_trx->trxCollection(_collection->cid())) == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
    }
  }
  
  readIndex();
  return res;
}

int IndexRangeBlock::initCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initCursor(items, pos);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  _pos = 0;
  _posInDocs = 0;
  
  if (_documents.size() == 0){
    _done = true;
  }

  return TRI_ERROR_NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* IndexRangeBlock::getSome (size_t atLeast,
                                        size_t atMost) {
  std::cout << "IndexRangeBlock::getSome\n";
  if (_done) {
    return nullptr;
  }

  if (_buffer.empty()) {
    if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
      _done = true;
      return nullptr;
    }
    _pos = 0;           // this is in the first block
    _posInDocs = 0;     // position in _documents . . .
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  size_t const curRegs = cur->getNrRegs();

  size_t available = _documents.size() - _posInDocs;
  size_t toSend = std::min(atMost, available);

  unique_ptr<AqlItemBlock> res(new AqlItemBlock(toSend, _varOverview->nrRegs[_depth]));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());

  // only copy 1st row of registers inherited from previous frame(s)
  inheritRegisters(cur, res.get(), _pos);

  // set our collection for our output register
  res->setDocumentCollection(curRegs, _trx->documentCollection(_collection->cid()));

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
    // we do not need to do a lookup in _varOverview->varInfo,
    // but can just take cur->getNrRegs() as registerId:
    res->setValue(j, curRegs,
                  AqlValue(reinterpret_cast<TRI_df_marker_t
                           const*>(_documents[_posInDocs++].getDataPtr())));
    // No harm done, if the setValue throws!
  }

  // Advance read position:
  if (_posInDocs >= _documents.size()) {
    // we have exhausted our local documents buffer
    _posInDocs = 0;

    if (++_pos >= cur->size()) {
      _buffer.pop_front();  // does not throw
      delete cur;
      _pos = 0;
    }
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

size_t IndexRangeBlock::skipSome (size_t atLeast,
                                  size_t atMost) {
  
  std::cout << "IndexRangeBlock::skipSome\n";
  
  if (_done) {
    return 0;
  }

  size_t skipped = 0;

  while (skipped < atLeast ){
    if (_buffer.empty()) {
      if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
        _done = true;
        return 0;
      }
      _pos = 0;           // this is in the first block
    }

    // If we get here, we do have _buffer.front()
    AqlItemBlock* cur = _buffer.front();

    _posInDocs += std::min(atMost, _documents.size() - _posInDocs);

    
    if (atMost < _documents.size() - _posInDocs){
      // eat just enough of _documents . . .
      _posInDocs += atMost;
      skipped = atMost;
    }
    else {
      // eat the whole of the current inVariable and proceed . . .
      skipped += (_documents.size() - _posInDocs);
      _posInDocs = 0;
      delete cur;
      _buffer.pop_front();
      _pos = 0;

    }
  }

  return skipped; 
}

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

void IndexRangeBlock::readSkiplistIndex () {
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  TRI_index_t* idx = en->_index;
  TRI_ASSERT(idx != nullptr);
  
  std::vector<std::vector<RangeInfo*>> ranges = en->_ranges;

  TRI_shaper_t* shaper = _collection->documentCollection()->getShaper(); 
  TRI_ASSERT(shaper != nullptr);
  
  TRI_index_operator_t* skiplistOperator = nullptr; 

  Json parameters(Json::List); 
  size_t i; 
  for (i = 0; i < idx->_fields._length && i < ranges.at(0).size(); i++) {
    // TODO doing 1 dim case at the moment . . .
    // FIXME assume that ranges.at(0) is of the correct type!
    for (auto x: ranges.at(0)) {
      if (std::string(idx->_fields._buffer[i]) == x->_attr) {
        if (x->is1ValueRangeInfo()) {   // it's an equality . . . 
          parameters(x->_low->_bound.get("value").copy());
        } 
        else {                          // it's not an equality and the final comparison 
          if (parameters.size() != 0) {
            skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, NULL,
                NULL, parameters.copy().steal(), shaper, NULL, i, NULL);
          }
          if (x->_low != nullptr) {
            auto op = x->_low->toIndexOperator(false, parameters.copy(), shaper);
            if (skiplistOperator != nullptr) {
              skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
                  skiplistOperator, op, NULL, shaper, NULL, 2, NULL);
            } else {
              skiplistOperator = op;
            }
          }
          if (x->_high != nullptr) {
            auto op = x->_high->toIndexOperator(true, parameters.copy(), shaper);
            if (skiplistOperator != nullptr) {
              skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
                  skiplistOperator, op, NULL, shaper, NULL, 2, NULL);
            } else {
              skiplistOperator = op;
            }
          }
        }
        break; // we found idx->_fields._buffer[i] and can move onto the next one . . 
      }
    }
  }

  if(skiplistOperator == nullptr){      // only have equalities . . .
    skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, NULL,
        NULL, parameters.steal(), shaper, NULL, i, NULL);
  }

  TRI_skiplist_iterator_t* skiplistIterator = TRI_LookupSkiplistIndex(idx, skiplistOperator);
  //skiplistOperator is deleted by the prev line 
  
  if (skiplistIterator == nullptr) {
    int res = TRI_errno();
    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      return;
    }

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  while (true) { 
    TRI_skiplist_index_element_t* indexElement = skiplistIterator->_next(skiplistIterator);

    if (indexElement == nullptr) {
      break;
    }
    _documents.push_back(*(indexElement->_document));
  }

  TRI_FreeSkiplistIterator(skiplistIterator);
}

void IndexRangeBlock::readHashIndex () {
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  TRI_index_t* idx = en->_index;
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

      for (auto x: en->_ranges.at(0)) {
        if (x->_attr == std::string(name)){//found attribute
          auto shaped = TRI_ShapedJsonJson(shaper, 
              JsonHelper::getArrayElement(x->_low->_bound.json(), "value"), false); 
          // here x->_low->_bound = x->_high->_bound 
          searchValue._values[i] = *shaped;
          
        }
      }

      std::cout << "PID: " << pid << ", NAME: " << name << "\n";
    }
  };
 
  setupSearchValue();  
  TRI_index_result_t list = TRI_LookupHashIndex(idx, &searchValue);

  destroySearchValue();
  
  size_t const n = list._length;
  for (size_t i = 0; i < n; ++i) {
    _documents.push_back(*(list._documents[i]));
  }
  
  TRI_DestroyIndexResult(&list);
}

// -----------------------------------------------------------------------------
// --SECTION--                                          class EnumerateListBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, here we get the inVariable
////////////////////////////////////////////////////////////////////////////////

int EnumerateListBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  auto en = reinterpret_cast<EnumerateListNode const*>(_exeNode);

  // get the inVariable register id . . .
  // staticAnalysis has been run, so _varOverview is set up
  auto it = _varOverview->varInfo.find(en->_inVariable->id);

  if (it == _varOverview->varInfo.end()){
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "variable not found");
  }

  _inVarRegId = (*it).second.registerId;

  return TRI_ERROR_NO_ERROR;
}

int EnumerateListBlock::initCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initCursor(items, pos);

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

AqlItemBlock* EnumerateListBlock::getSome (size_t atLeast, size_t atMost) {
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
      size_t toSend = std::min(atMost, sizeInVar - _index);

      // create the result
      res.reset(new AqlItemBlock(toSend, _varOverview->nrRegs[_depth]));

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
        sizeInVar = inVarReg._range->_high - inVarReg._range->_low + 1;
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
      return AqlValue(new Json(inVarReg._json->at(_index++).copy()));
    }
    case AqlValue::RANGE: {
      return AqlValue(new Json(static_cast<double>(inVarReg._range->_low + _index++)));
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

int CalculationBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // We know that staticAnalysis has been run, so _varOverview is set up
  auto en = static_cast<CalculationNode const*>(getPlanNode());

  std::unordered_set<Variable*> inVars = _expression->variables();
  _inVars.clear();
  _inRegs.clear();

  for (auto it = inVars.begin(); it != inVars.end(); ++it) {
    _inVars.push_back(*it);
    auto it2 = _varOverview->varInfo.find((*it)->id);

    TRI_ASSERT(it2 != _varOverview->varInfo.end());
    _inRegs.push_back(it2->second.registerId);
  }

  // check if the expression is "only" a reference to another variable
  // this allows further optimizations inside doEvaluation
  _isReference = (_expression->node()->type == NODE_TYPE_REFERENCE);

  if (_isReference) {
    TRI_ASSERT(_inRegs.size() == 1);
  }

  auto it3 = _varOverview->varInfo.find(en->_outVariable->id);
  TRI_ASSERT(it3 != _varOverview->varInfo.end());
  _outReg = it3->second.registerId;

  return TRI_ERROR_NO_ERROR;
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
      // must clone, otherwise all the results become invalid
      AqlValue a = result->getValue(i, _inRegs[0]).clone();
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

int SubqueryBlock::initialize () {
  int res = ExecutionBlock::initialize();
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // We know that staticAnalysis has been run, so _varOverview is set up

  auto en = static_cast<SubqueryNode const*>(getPlanNode());

  auto it3 = _varOverview->varInfo.find(en->_outVariable->id);
  TRI_ASSERT(it3 != _varOverview->varInfo.end());
  _outReg = it3->second.registerId;

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
    int ret = _subquery->initCursor(res.get(), i);
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

int FilterBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // We know that staticAnalysis has been run, so _varOverview is set up

  auto en = static_cast<FilterNode const*>(getPlanNode());

  auto it = _varOverview->varInfo.find(en->_inVariable->id);
  TRI_ASSERT(it != _varOverview->varInfo.end());
  _inReg = it->second.registerId;

  return TRI_ERROR_NO_ERROR;
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
    _chosen.clear();
    AqlItemBlock* cur = _buffer.front();
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
        if(! skipping) {
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
    else if (collector.size() > 1 ) {
      try {
        result = AqlItemBlock::concatenate(collector);
      }
      catch (...){
        for (auto x : collector){
          delete x;
        }
        throw;
      }
      for (auto x : collector){
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

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

int AggregateBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  auto en = static_cast<AggregateNode const*>(getPlanNode());

  // Reinitialise if we are called a second time:
  _aggregateRegisters.clear();
  _variableNames.clear();

  for (auto p : en->_aggregateVariables){
    //We know that staticAnalysis has been run, so _varOverview is set up
    auto itOut = _varOverview->varInfo.find(p.first->id);
    TRI_ASSERT(itOut != _varOverview->varInfo.end());

    auto itIn = _varOverview->varInfo.find(p.second->id);
    TRI_ASSERT(itIn != _varOverview->varInfo.end());
    _aggregateRegisters.push_back(make_pair((*itOut).second.registerId, (*itIn).second.registerId));
  }

  if (en->_outVariable != nullptr) {
    auto it = _varOverview->varInfo.find(en->_outVariable->id);
    TRI_ASSERT(it != _varOverview->varInfo.end());
    _groupRegister = (*it).second.registerId;

    TRI_ASSERT(_groupRegister > 0);

    // construct a mapping of all register ids to variable names
    // we need this mapping to generate the grouped output

    for (size_t i = 0; i < _varOverview->varInfo.size(); ++i) {
      _variableNames.push_back(""); // init with some default value
    }

    // iterate over all our variables
    for (auto it = _varOverview->varInfo.begin(); it != _varOverview->varInfo.end(); ++it) {
      // find variable in the global variable map
      auto itVar = en->_variableMap.find((*it).first);

      if (itVar != en->_variableMap.end()) {
        _variableNames[(*it).second.registerId] = (*itVar).second;
      }
    }
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

  if(!skipping){
    size_t const curRegs = cur->getNrRegs();
    res.reset(new AqlItemBlock(atMost, _varOverview->nrRegs[_depth]));

    TRI_ASSERT(curRegs <= res->getNrRegs());
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

int SortBlock::initialize () {
  int res = ExecutionBlock::initialize();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  auto en = static_cast<SortNode const*>(getPlanNode());

  _sortRegisters.clear();

  for( auto p: en->_elements){
    //We know that staticAnalysis has been run, so _varOverview is set up
    auto it = _varOverview->varInfo.find(p.first->id);
    TRI_ASSERT(it != _varOverview->varInfo.end());
    _sortRegisters.push_back(make_pair(it->second.registerId, p.second));
  }

  return TRI_ERROR_NO_ERROR;
}

int SortBlock::initCursor (AqlItemBlock* items, size_t pos) {

  int res = ExecutionBlock::initCursor(items, pos);
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
      size_t sizeNext = std::min(sum - count, DefaultBatchSize);
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

int LimitBlock::initCursor (AqlItemBlock* items, size_t pos) {
  int res = ExecutionBlock::initCursor(items, pos);
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
  auto it = _varOverview->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != _varOverview->varInfo.end());
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
/// @brief resolve a collection name and return cid and document key
////////////////////////////////////////////////////////////////////////////////
  
int ModificationBlock::resolve (char const* handle, 
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
                                      bool ignoreErrors) {
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
      THROW_ARANGO_EXCEPTION(code);
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
  auto it = _varOverview->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != _varOverview->varInfo.end());
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
  auto it = _varOverview->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != _varOverview->varInfo.end());
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
            errorCode = _trx->create(trxCollection, TRI_DOC_MARKER_KEY_EDGE, &mptr, json.json(), &edge, ep->_options.waitForSync);
          }
          else {
            // document
            errorCode = _trx->create(trxCollection, TRI_DOC_MARKER_KEY_DOCUMENT, &mptr, json.json(), nullptr, ep->_options.waitForSync);
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
  auto it = _varOverview->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != _varOverview->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  
  if (hasKeyVariable) {
    it = _varOverview->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != _varOverview->varInfo.end());
    keyRegisterId = it->second.registerId;
  }
  
  auto trxCollection = _trx->trxCollection(_collection->cid());

  if (ep->_outVariable == nullptr) {
    // don't return anything
         
    // loop over all blocks
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      auto res = (*it);
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
  auto it = _varOverview->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != _varOverview->varInfo.end());
  RegisterId const registerId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization

  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  
  if (hasKeyVariable) {
    it = _varOverview->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != _varOverview->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  auto trxCollection = _trx->trxCollection(_collection->cid());

  if (ep->_outVariable == nullptr) {
    // don't return anything
         
    // loop over all blocks
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      auto res = (*it);
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
      (*it) = nullptr;  
      delete res;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                struct ExecutionBlock::VarOverview
// -----------------------------------------------------------------------------

// Copy constructor used for a subquery:
ExecutionBlock::VarOverview::VarOverview (VarOverview const& v,
                                          unsigned int newdepth)
  : varInfo(v.varInfo),
    nrRegsHere(v.nrRegsHere),
    nrRegs(v.nrRegs),
    subQueryBlocks(),
    depth(newdepth+1),
    totalNrRegs(v.nrRegs[newdepth]),
    me(nullptr) {
  nrRegs.resize(depth);
  nrRegsHere.resize(depth);
  nrRegsHere.push_back(0);
  nrRegs.push_back(nrRegs.back());
}

void ExecutionBlock::VarOverview::after (ExecutionBlock *eb) {
  switch (eb->getPlanNode()->getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION: 
    case ExecutionNode::INDEX_RANGE: {
      depth++;
      nrRegsHere.push_back(1);
      nrRegs.push_back(1 + nrRegs.back());
      auto ep = static_cast<EnumerateCollectionNode const*>(eb->getPlanNode());
      TRI_ASSERT(ep != nullptr);
      varInfo.insert(make_pair(ep->_outVariable->id,
                               VarInfo(depth, totalNrRegs)));
      totalNrRegs++;
      break;
    }
    case ExecutionNode::ENUMERATE_LIST: {
      depth++;
      nrRegsHere.push_back(1);
      nrRegs.push_back(1 + nrRegs.back());
      auto ep = static_cast<EnumerateListNode const*>(eb->getPlanNode());
      TRI_ASSERT(ep != nullptr);
      varInfo.insert(make_pair(ep->_outVariable->id,
                               VarInfo(depth, totalNrRegs)));
      totalNrRegs++;
      break;
    }
    case ExecutionNode::CALCULATION: {
      nrRegsHere[depth]++;
      nrRegs[depth]++;
      auto ep = static_cast<CalculationNode const*>(eb->getPlanNode());
      TRI_ASSERT(ep != nullptr);
      varInfo.insert(make_pair(ep->_outVariable->id,
                               VarInfo(depth, totalNrRegs)));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::SUBQUERY: {
      nrRegsHere[depth]++;
      nrRegs[depth]++;
      auto ep = static_cast<SubqueryNode const*>(eb->getPlanNode());
      TRI_ASSERT(ep != nullptr);
      varInfo.insert(make_pair(ep->_outVariable->id,
                               VarInfo(depth, totalNrRegs)));
      totalNrRegs++;
      subQueryBlocks.push_back(eb);
      break;
    }

    case ExecutionNode::AGGREGATE: {
      depth++;
      nrRegsHere.push_back(0);
      nrRegs.push_back(nrRegs.back());

      auto ep = static_cast<AggregateNode const*>(eb->getPlanNode());
      for (auto p : ep->_aggregateVariables) {
        // p is std::pair<Variable const*,Variable const*>
        // and the first is the to be assigned output variable
        // for which we need to create a register in the current
        // frame:
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.insert(make_pair(p.first->id,
                                 VarInfo(depth, totalNrRegs)));
        totalNrRegs++;
      }
      if (ep->_outVariable != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.insert(make_pair(ep->_outVariable->id,
                                 VarInfo(depth, totalNrRegs)));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::SORT: {
      // sort sorts in place and does not produce new registers
      break;
    }
    
    case ExecutionNode::RETURN: {
      // return is special. it produces a result but is the last step in the pipeline
      break;
    }

    case ExecutionNode::REMOVE: {
      auto ep = static_cast<RemoveNode const*>(eb->getPlanNode());
      if (ep->_outVariable != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.insert(make_pair(ep->_outVariable->id,
                                 VarInfo(depth, totalNrRegs)));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::INSERT: {
      auto ep = static_cast<InsertNode const*>(eb->getPlanNode());
      if (ep->_outVariable != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.insert(make_pair(ep->_outVariable->id,
                                 VarInfo(depth, totalNrRegs)));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::UPDATE: {
      auto ep = static_cast<UpdateNode const*>(eb->getPlanNode());
      if (ep->_outVariable != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.insert(make_pair(ep->_outVariable->id,
                                 VarInfo(depth, totalNrRegs)));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::REPLACE: {
      auto ep = static_cast<ReplaceNode const*>(eb->getPlanNode());
      if (ep->_outVariable != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.insert(make_pair(ep->_outVariable->id,
                                 VarInfo(depth, totalNrRegs)));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::SINGLETON:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::NORESULTS: {
      // these node types do not produce any new registers
      break;
    }

    case ExecutionNode::INTERSECTION: 
    case ExecutionNode::LOOKUP_JOIN:
    case ExecutionNode::MERGE_JOIN:
    case ExecutionNode::LOOKUP_INDEX_UNIQUE:
    case ExecutionNode::LOOKUP_INDEX_RANGE:
    case ExecutionNode::LOOKUP_FULL_COLLECTION:
    case ExecutionNode::CONCATENATION:
    case ExecutionNode::MERGE:
    case ExecutionNode::REMOTE: {
      // TODO: these node types need to be implemented!
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "node type not implemented");
    }
    
    case ExecutionNode::ILLEGAL: {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }

  eb->_depth = depth;
  eb->_varOverview = *me;

  // Now find out which registers ought to be erased after this node:
  auto ep = eb->getPlanNode();
  if (ep->getType() != ExecutionNode::RETURN) {
    // ReturnBlocks are special, since they return a single column anyway
    std::unordered_set<Variable const*> const& varsUsedLater = ep->getVarsUsedLater();
    std::vector<Variable const*> const& varsUsedHere = ep->getVariablesUsedHere();
    
    // We need to delete those variables that have been used here but are not
    // used any more later:
    std::unordered_set<RegisterId> regsToClear;
    for (auto v : varsUsedHere) {
      auto it = varsUsedLater.find(v);
      if (it == varsUsedLater.end()) {
        auto it2 = varInfo.find(v->id);
        TRI_ASSERT(it2 != varInfo.end());
        RegisterId r = it2->second.registerId;
        regsToClear.insert(r);
      }
    }
    eb->setRegsToClear(regsToClear);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                              class NoResultsBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initCursor, only call base
////////////////////////////////////////////////////////////////////////////////

int NoResultsBlock::initCursor (AqlItemBlock* items, size_t pos) {
  _done = true;
  return TRI_ERROR_NO_ERROR;
}

int NoResultsBlock::getOrSkipSome (size_t atLeast,
                                   size_t atMost,
                                   bool skipping,
                                   AqlItemBlock*& result,
                                   size_t& skipped) {

  TRI_ASSERT(result == nullptr && skipped == 0);
  return TRI_ERROR_NO_ERROR;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
