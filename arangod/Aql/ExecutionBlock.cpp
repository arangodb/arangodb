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
/// @author Michael Hackstein
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/json-utilities.h"
/*
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Traverser.h"
#include "Cluster/ClusterMethods.h"
#include "Dispatcher/DispatcherThread.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/HashIndex.h"
#include "Utils/ShapedJsonTransformer.h"
#include "V8/v8-globals.h"
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"
*/

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::aql;

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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
