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

#include "EnumerateListBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

EnumerateListBlock::EnumerateListBlock(ExecutionEngine* engine, EnumerateListNode const* en)
    : ExecutionBlock(engine, en),
      _index(0),
      _docVecSize(0),
      _inVarRegId(ExecutionNode::MaxRegisterId),
      _inflight(0) {
  auto it = en->getRegisterPlan()->varInfo.find(en->_inVariable->id);

  if (it == en->getRegisterPlan()->varInfo.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "variable not found");
  }

  _inVarRegId = (*it).second.registerId;
  TRI_ASSERT(_inVarRegId < ExecutionNode::MaxRegisterId);
}

EnumerateListBlock::~EnumerateListBlock() {}

std::pair<ExecutionState, arangodb::Result> EnumerateListBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING || !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  // handle local data (if any)
  _index = 0;  // index in _inVariable for next run
  _inflight = 0;

  return res;
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> EnumerateListBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  std::unique_ptr<AqlItemBlock> res;

  while (res == nullptr) {
    // repeatedly try to get more stuff from upstream
    // note that the value of the variable we have to loop over
    // can contain zero entries, in which case we have to
    // try again!

    size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
    BufferState bufferState = getBlockIfNeeded(toFetch);
    if (bufferState == BufferState::WAITING) {
      TRI_ASSERT(res == nullptr);
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return {ExecutionState::WAITING, nullptr};
    }
    if (bufferState == BufferState::NO_MORE_BLOCKS) {
      break;
    }

    TRI_ASSERT(bufferState == BufferState::HAS_BLOCKS ||
               bufferState == BufferState::HAS_NEW_BLOCK);
    TRI_ASSERT(!_buffer.empty());

    // if we make it here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    // get the thing we are looping over
    AqlValue const& inVarReg = cur->getValueReference(_pos, _inVarRegId);

    if (!inVarReg.isArray()) {
      throwArrayExpectedException(inVarReg);
    }

    size_t sizeInVar;
    if (inVarReg.isDocvec()) {
      // special handling here. calculate docvec length only once
      if (_index == 0) {
        // we require the total number of items
        _docVecSize = inVarReg.length();
      }
      sizeInVar = _docVecSize;
    } else {
      sizeInVar = inVarReg.length();
    }

    if (sizeInVar == 0) {
      res = nullptr;
    } else {
      size_t toSend = (std::min)(atMost, sizeInVar - _index);

      // create the result
      res.reset(requestBlock(toSend, getNrOutputRegisters()));

      inheritRegisters(cur, res.get(), _pos);

      for (size_t j = 0; j < toSend; j++) {
        // add the new register value . . .
        bool mustDestroy;
        AqlValue a = getAqlValue(inVarReg, sizeInVar, mustDestroy);
        AqlValueGuard guard(a, mustDestroy);

        // deep copy of the inVariable.at(_pos) with correct memory
        // requirements
        // Note that _index has been increased by 1 by getAqlValue!
        TRI_IF_FAILURE("EnumerateListBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        res->setValue(j, cur->getNrRegs(), a);
        guard.steal();  // itemblock is now responsible for value

        if (j > 0) {
          // re-use already copied AqlValues
          res->copyValuesFromFirstRow(j, cur->getNrRegs());
        }
      }
    }

    if (_index == sizeInVar) {
      _index = 0;

      AqlItemBlock* removedBlock = advanceCursor(1, cur->size());
      returnBlockUnlessNull(removedBlock);
    }
  }

  // Clear out registers no longer needed later:
  _inflight = 0;
  clearRegisters(res.get());
  traceGetSomeEnd(res.get(), getHasMoreState());
  return {getHasMoreState(), std::move(res)};
}

std::pair<ExecutionState, size_t> EnumerateListBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    size_t skipped = _inflight;
    _inflight = 0;
    traceSkipSomeEnd(skipped, ExecutionState::DONE);
    return {ExecutionState::DONE, skipped};
  }
  while (_inflight < atMost) {
    size_t toFetch = (std::min)(DefaultBatchSize(), atMost - _inflight);
    BufferState bufferState = getBlockIfNeeded(toFetch);
    if (bufferState == BufferState::WAITING) {
      traceSkipSomeEnd(0, ExecutionState::WAITING);
      return {ExecutionState::WAITING, 0};
    }
    if (bufferState == BufferState::NO_MORE_BLOCKS) {
      break;
    }

    TRI_ASSERT(bufferState == BufferState::HAS_BLOCKS ||
               bufferState == BufferState::HAS_NEW_BLOCK);
    TRI_ASSERT(!_buffer.empty());

    // if we make it here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();
    TRI_ASSERT(cur != nullptr);

    // get the thing we are looping over
    AqlValue const& inVarReg = cur->getValueReference(_pos, _inVarRegId);
    // get the size of the thing we are looping over
    if (!inVarReg.isArray()) {
      throwArrayExpectedException(inVarReg);
    }

    size_t sizeInVar;
    if (inVarReg.isDocvec()) {
      // special handling here. calculate docvec length only once
      if (_index == 0) {
        // we require the total number of items
        _docVecSize = inVarReg.docvecSize();
      }
      sizeInVar = _docVecSize;
    } else {
      sizeInVar = inVarReg.length();
    }

    if (atMost < sizeInVar - _index) {
      // eat just enough of inVariable . . .
      _index += atMost;
      _inflight += atMost;
    } else {
      // eat the whole of the current inVariable and proceed . . .
      atMost -= (sizeInVar - _index);
      _inflight += (sizeInVar - _index);
      _index = 0;
      if (++_pos == cur->size()) {
        returnBlock(cur);
        _buffer.pop_front();  // does not throw
        _pos = 0;
      }
    }
  }

  size_t skipped = _inflight;
  _inflight = 0;
  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

/// @brief create an AqlValue from the inVariable using the current _index
AqlValue EnumerateListBlock::getAqlValue(AqlValue const& inVarReg, size_t n, bool& mustDestroy) {
  TRI_IF_FAILURE("EnumerateListBlock::getAqlValue") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return inVarReg.at(_trx, _index++, n, mustDestroy, true);
}

void EnumerateListBlock::throwArrayExpectedException(AqlValue const& value) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_ARRAY_EXPECTED,
      std::string("collection or ") + TRI_errno_string(TRI_ERROR_QUERY_ARRAY_EXPECTED) +
          std::string(
              " as operand to FOR loop; you provided a value of type '") +
          value.getTypeString() + std::string("'"));
}
