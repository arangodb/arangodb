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

EnumerateListBlock::EnumerateListBlock(ExecutionEngine* engine,
                                       EnumerateListNode const* en)
    : ExecutionBlock(engine, en),
      _index(0),
      _thisBlock(0),
      _seen(0),
      _docVecSize(0),
      _inVarRegId(ExecutionNode::MaxRegisterId) {
  auto it = en->getRegisterPlan()->varInfo.find(en->_inVariable->id);

  if (it == en->getRegisterPlan()->varInfo.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "variable not found");
  }

  _inVarRegId = (*it).second.registerId;
  TRI_ASSERT(_inVarRegId < ExecutionNode::MaxRegisterId);
}

EnumerateListBlock::~EnumerateListBlock() {}

int EnumerateListBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();  
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // handle local data (if any)
  _index = 0;      // index in _inVariable for next run
  _thisBlock = 0;  // the current block in the _inVariable DOCVEC
  _seen = 0;       // the sum of the sizes of the blocks in the _inVariable

  return TRI_ERROR_NO_ERROR;
  DEBUG_END_BLOCK();  
}

AqlItemBlock* EnumerateListBlock::getSome(size_t, size_t atMost) {
  DEBUG_BEGIN_BLOCK();  
  traceGetSomeBegin();
  if (_done) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> res(nullptr);

  do {
    // repeatedly try to get more stuff from upstream
    // note that the value of the variable we have to loop over
    // can contain zero entries, in which case we have to
    // try again!

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
        _done = true;
        traceGetSomeEnd(nullptr);
        return nullptr;
      }
      _pos = 0;  // this is in the first block
    }

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
        _docVecSize = inVarReg.docvecSize();
      }
      sizeInVar = _docVecSize;
    }
    else {
      sizeInVar = inVarReg.length();
    }

    if (sizeInVar == 0) {
      res = nullptr;
    } else {
      size_t toSend = (std::min)(atMost, sizeInVar - _index);

      // create the result
      res.reset(requestBlock(toSend, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

      inheritRegisters(cur, res.get(), _pos);

      for (size_t j = 0; j < toSend; j++) {
        // add the new register value . . .
        bool mustDestroy;
        AqlValue a = getAqlValue(inVarReg, mustDestroy);
        AqlValueGuard guard(a, mustDestroy);

        // deep copy of the inVariable.at(_pos) with correct memory
        // requirements
        // Note that _index has been increased by 1 by getAqlValue!
        TRI_IF_FAILURE("EnumerateListBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        res->setValue(j, cur->getNrRegs(), a);
        guard.steal(); // itemblock is now responsible for value
        
        if (j > 0) {
          // re-use already copied AqlValues
          res->copyValuesFromFirstRow(j, cur->getNrRegs());
        }
      }
    }

    if (_index == sizeInVar) {
      _index = 0;
      _thisBlock = 0;
      _seen = 0;
      // advance read position in the current block . . .
      if (++_pos == cur->size()) {
        returnBlock(cur);
        _buffer.pop_front();  // does not throw
        _pos = 0;
      }
    }
  } while (res.get() == nullptr);

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  traceGetSomeEnd(res.get());
  return res.release();
  DEBUG_END_BLOCK();  
}

size_t EnumerateListBlock::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();  
  if (_done) {
    return 0;
  }

  size_t skipped = 0;

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;  // this is in the first block
    }

    // if we make it here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

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
    }
    else {
      sizeInVar = inVarReg.length();
    }

    if (atMost < sizeInVar - _index) {
      // eat just enough of inVariable . . .
      _index += atMost;
      skipped += atMost;
    } else {
      // eat the whole of the current inVariable and proceed . . .
      skipped += (sizeInVar - _index);
      _index = 0;
      _thisBlock = 0;
      _seen = 0;
      if (++_pos == cur->size()) {
        returnBlock(cur);
        _buffer.pop_front();  // does not throw
        _pos = 0;
      }
    }
  }
  return skipped;
  DEBUG_END_BLOCK();  
}

/// @brief create an AqlValue from the inVariable using the current _index
AqlValue EnumerateListBlock::getAqlValue(AqlValue const& inVarReg, bool& mustDestroy) {
  DEBUG_BEGIN_BLOCK();  
  TRI_IF_FAILURE("EnumerateListBlock::getAqlValue") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (inVarReg.isDocvec()) {
    // special handling here, to save repeated evaluation of all itemblocks
    AqlItemBlock* block = inVarReg.docvecAt(_thisBlock);
    AqlValue out = block->getValueReference(_index - _seen, 0).clone();
    if (++_index == block->size() + _seen) {
      _seen += block->size();
      _thisBlock++;
    }
    mustDestroy = true;
    return out;
  }

  return inVarReg.at(_trx, _index++, mustDestroy, true);
  DEBUG_END_BLOCK();  
}

void EnumerateListBlock::throwArrayExpectedException(AqlValue const& value) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_ARRAY_EXPECTED,
      std::string("collection or ") +
      TRI_errno_string(TRI_ERROR_QUERY_ARRAY_EXPECTED) +
          std::string(" as operand to FOR loop; you provided a value of type '") +
        value.getTypeString () +
        std::string("'"));
}
