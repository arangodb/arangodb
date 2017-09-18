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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "EnumerateViewBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

EnumerateViewBlock::EnumerateViewBlock(ExecutionEngine* engine,
                                       EnumerateViewNode const* en)
    : ExecutionBlock(engine, en),
      _view(en->view()),
      _outVariable(en->_outVariable),
      _iter(nullptr),
      _mmdr(new ManagedDocumentResult), // TODO
      _hasMore(true) // has more data initially
{
  TRI_ASSERT(_view != nullptr);

  // no filter means 'RETURN *'
  AstNode* filter = nullptr;

  if (en->condition()) {
    filter = en->condition()->root();
  }

  _iter.reset(_view->iteratorForCondition(transaction(), filter, _outVariable,
                                          en->sortCondition().get()));
  TRI_ASSERT(_iter != nullptr);
}

EnumerateViewBlock::~EnumerateViewBlock() {}

int EnumerateViewBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  DEBUG_BEGIN_BLOCK();
  if (_iter.get() != nullptr) {
    _iter->reset();
  }
  DEBUG_END_BLOCK();

  _hasMore = true; // has more data initially

  return TRI_ERROR_NO_ERROR;
  DEBUG_END_BLOCK();
}

AqlItemBlock* EnumerateViewBlock::getSome(size_t, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();
  TRI_ASSERT(_iter != nullptr);

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
        _iter->reset();
      }

      // If we get here, we do have _buffer.front()
      cur = _buffer.front();

      if (!_hasMore) {
        needMore = true;
        _hasMore = true;
        // we have exhausted this cursor
        // re-initialize fetching of documents
        _iter->reset();
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        }
      }
    } while (needMore);

    TRI_ASSERT(cur != nullptr);

    size_t curRegs = cur->getNrRegs();

    RegisterId nrRegs =
        getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];

    res.reset(requestBlock(atMost, nrRegs));
    // automatically freed if we throw
    TRI_ASSERT(curRegs <= res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    auto cb = [&](DocumentIdentifierToken const& tkn) {
      if (_iter->readDocument(tkn, *_mmdr)) {
        // The result is in the first variable of this depth,
        // we do not need to do a lookup in
        // getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        uint8_t const* vpack = _mmdr->vpack();
        if (_mmdr->canUseInExternal()) {
          res->setValue(send, static_cast<arangodb::aql::RegisterId>(curRegs),
                        AqlValue(AqlValueHintNoCopy(vpack)));
        } else {
          res->setValue(send, static_cast<arangodb::aql::RegisterId>(curRegs),
                        AqlValue(AqlValueHintCopy(vpack)));
        }
      }

      if (send > 0) {
        // re-use already copied AQLValues
        res->copyValuesFromFirstRow(send, static_cast<RegisterId>(curRegs));
      }
      ++send;
    };

    throwIfKilled();  // check if we were aborted

    TRI_IF_FAILURE("EnumerateViewBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasMore = _iter->next(cb, atMost);

    // If the collection is actually empty we cannot forward an empty block
  } while (send == 0);
  // _engine->_stats.scannedFull += static_cast<int64_t>(send);
  // TODO stats?
  TRI_ASSERT(res != nullptr);

  if (send < atMost) {
    // The collection did not have enough results
    res->shrink(send, false);
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  traceGetSomeEnd(res.get());

  return res.release();

  DEBUG_END_BLOCK();
}

size_t EnumerateViewBlock::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = 0;
  TRI_ASSERT(_iter != nullptr);

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
      _iter->reset();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();
    uint64_t skippedHere = 0;

    _iter->skip(atMost - skipped, skippedHere);

    skipped += skippedHere;

    if (skipped < atLeast) {
      // not skipped enough re-initialize fetching of documents
      _iter->reset();
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      }
    }
  }

  // _engine->_stats.scannedFull += static_cast<int64_t>(skipped);
  // TODO stats?
  // We skipped atLeast documents
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}
