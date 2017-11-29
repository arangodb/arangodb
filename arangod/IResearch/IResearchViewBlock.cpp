////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewBlock.h"
#include "IResearchViewNode.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"
#include <iostream>

namespace {

inline arangodb::iresearch::IResearchViewNode const& getPlanNode(
    arangodb::iresearch::IResearchViewBlock const& block
) noexcept {
  TRI_ASSERT(block.getPlanNode());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return dynamic_cast<arangodb::iresearch::IResearchViewNode const&>(*block.getPlanNode());
#else
  return static_cast<arangodb::iresearch::IResearchViewNode const&>(*block.getPlanNode());
#endif
}

}

namespace arangodb {
namespace iresearch {

using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                              ViewExpressionContext implementation
// -----------------------------------------------------------------------------

size_t ViewExpressionContext::numRegisters() const {
  return _data->getNrRegs();
}

AqlValue ViewExpressionContext::getVariableValue(
    Variable const* variable,
    bool doCopy,
    bool& mustDestroy) const {
  mustDestroy = false;
  auto const reg = _block->getRegister(variable);

  if (reg == arangodb::aql::ExecutionNode::MaxRegisterId) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto& value = _data->getValueReference(_pos, reg);

  if (doCopy) {
    mustDestroy = true;
    return value.clone();
  }

  return value;
}

// -----------------------------------------------------------------------------
// --SECTION--                                 EnumerateViewBlock implementation
// -----------------------------------------------------------------------------

IResearchViewBlock::IResearchViewBlock(
    ExecutionEngine* engine,
    IResearchViewNode const* en)
  : ExecutionBlock(engine, en),
    _ctx(this),
    _iter(nullptr),
    _hasMore(true), // has more data initially
    _volatileState(en->volatile_state()) {
}

int IResearchViewBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  const int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _hasMore = true; // has more data initially

  return TRI_ERROR_NO_ERROR;
  DEBUG_END_BLOCK();
}

void IResearchViewBlock::refreshIterator() {
  TRI_ASSERT(!_buffer.empty());

  _ctx._data = _buffer.front();
  _ctx._pos = _pos;

  if (!_iter) {
    // initialize `_iter` in lazy fashion
    _iter = ::getPlanNode(*this).iterator(*_trx, _ctx);
  }

  if (!_iter || !_iter->reset(_volatileState ? &_ctx : nullptr)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

AqlItemBlock* IResearchViewBlock::getSome(size_t, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();

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
        size_t const toFetch = (std::min)(DefaultBatchSize(), atMost);
        if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
          _done = true;
          return nullptr;
        }
        _pos = 0;  // this is in the first block
        refreshIterator();
      }

      // If we get here, we do have _buffer.front()
      cur = _buffer.front();

      if (!_hasMore) {
        needMore = true;
        _hasMore = true;

        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        } else {
          // we have exhausted this cursor
          // re-initialize fetching of documents
          refreshIterator();
        }
      }
    } while (needMore);

    TRI_ASSERT(_iter);
    TRI_ASSERT(cur);

    size_t const curRegs = cur->getNrRegs();
    auto const& planNode = ::getPlanNode(*this);
    RegisterId const nrRegs = planNode.getRegisterPlan()->nrRegs[planNode.getDepth()];

    res.reset(requestBlock(atMost, nrRegs));
    // automatically freed if we throw
    TRI_ASSERT(curRegs <= res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    auto cb = [this, &res, &send, curRegs](LocalDocumentId const& tkn) {
      if (_iter->readDocument(tkn, _mmdr)) {
        // The result is in the first variable of this depth,
        // we do not need to do a lookup in
        // getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        uint8_t const* vpack = _mmdr.vpack();

        if (_mmdr.canUseInExternal()) {
          res->setValue(send, static_cast<arangodb::aql::RegisterId>(curRegs),
                        AqlValue(AqlValueHintDocumentNoCopy(vpack)));
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

  TRI_ASSERT(res != nullptr);

  // aggregate stats
   _engine->_stats.scannedIndex += static_cast<int64_t>(send);

  if (send < atMost) {
    // The collection did not have enough results
    res->shrink(send);
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  traceGetSomeEnd(res.get());

  return res.release();

  DEBUG_END_BLOCK();
}

size_t IResearchViewBlock::skipSome(size_t atLeast, size_t atMost) {
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
      refreshIterator();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();
    uint64_t skippedHere = 0;

    _iter->skip(atMost - skipped, skippedHere);

    skipped += skippedHere;

    if (skipped < atLeast) {
      // not skipped enough re-initialize fetching of documents
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      } else {
        // we have exhausted this cursor
        // re-initialize fetching of documents
        refreshIterator();
      }
    }
  }

  // aggregate stats
  _engine->_stats.scannedIndex += static_cast<int64_t>(skipped);

  // We skipped atLeast documents
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

} // iresearch
} // arangodb
