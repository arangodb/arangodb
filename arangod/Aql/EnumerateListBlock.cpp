////////////////////////////////////////////////////////////////////////////////
/// @brief EnumerateListBlock
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

#include "EnumerateListBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;

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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
