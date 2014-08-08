////////////////////////////////////////////////////////////////////////////////
/// @brief Aql item block
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

#include "Aql/AqlItemBlock.h"
//#include "Basics/JsonHelper.h"
//#include "Utils/AqlTransaction.h"
//#include "Utils/V8TransactionContext.h"
//#include "V8/v8-conv.h"
//#include "V8Server/v8-wrapshapedjson.h"
//#include "VocBase/voc-shaper.h"

using namespace triagens::aql;
//using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                                      AqlItemBlock
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the block
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock::AqlItemBlock (size_t nrItems, 
                            RegisterId nrRegs)
  : _nrItems(nrItems), 
    _nrRegs(nrRegs) {

  if (nrItems > 0 && nrRegs > 0) {
    _data.reserve(nrItems * nrRegs);
    for (size_t i = 0; i < nrItems * nrRegs; ++i) {
      _data.emplace_back();
    }
 }
 
 if (nrRegs > 0) {
   _docColls.reserve(nrRegs);
   for (size_t i = 0; i < nrRegs; ++i) {
     _docColls.push_back(nullptr);
   }
 }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the block
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock::~AqlItemBlock () {
  std::unordered_set<AqlValue> cache;

  for (size_t i = 0; i < _nrItems * _nrRegs; i++) {
    if (! _data[i].isEmpty()) {
      auto it = cache.find(_data[i]);

      if (it == cache.end()) {
        cache.insert(_data[i]);
        _data[i].destroy();
      }
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shrink the block to the specified number of rows
////////////////////////////////////////////////////////////////////////////////

void AqlItemBlock::shrink (size_t nrItems) {
  TRI_ASSERT(nrItems > 0);

  if (nrItems == _nrItems) {
    // nothing to do
    return;
  }
  
  if (nrItems > _nrItems) {
    // cannot use shrink() to increase the size of the block
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // erase all stored values in the region that we freed
  for (size_t i = nrItems; i < _nrItems; ++i) {
    for (RegisterId j = 0; j < _nrRegs; ++j) {
      eraseValue(i, j);
    }
  }

  // adjust the size of the block
  _nrItems = nrItems;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::slice (size_t from, 
                                   size_t to) const {
  TRI_ASSERT(from < to && to <= _nrItems);

  std::unordered_map<AqlValue, AqlValue> cache;

  AqlItemBlock* res = nullptr;
  try {
    res = new AqlItemBlock(to - from, _nrRegs);

    for (RegisterId col = 0; col < _nrRegs; col++) {
      res->_docColls[col] = _docColls[col];
    }
    for (size_t row = from; row < to; row++) {
      for (RegisterId col = 0; col < _nrRegs; col++) {
        AqlValue const& a(_data[row * _nrRegs + col]);

        if (! a.isEmpty()) {
          auto it = cache.find(a);
          if (it == cache.end()) {
            AqlValue b = a.clone();
            res->_data[(row - from) * _nrRegs + col] = b;
            cache.insert(make_pair(a,b));
          }
          else {
            res->_data[(row - from) * _nrRegs + col] = it->second;
          }
        }
      }
    }

    return res;
  }
  catch (...) {
    delete res;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone chosen columns for a subset
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::slice (std::vector<size_t>& chosen, 
                                   size_t from, 
                                   size_t to) {
  TRI_ASSERT(from < to && to <= chosen.size());

  std::unordered_map<AqlValue, AqlValue> cache;

  AqlItemBlock* res = nullptr;
  try {
    res = new AqlItemBlock(to - from, _nrRegs);

    for (RegisterId col = 0; col < _nrRegs; col++) {
      res->_docColls[col] = _docColls[col];
    }
    for (size_t row = from; row < to; row++) {
      for (RegisterId col = 0; col < _nrRegs; col++) {
        AqlValue& a(_data[chosen[row] * _nrRegs + col]);

        if (! a.isEmpty()) {
          auto it = cache.find(a);
          if (it == cache.end()) {
            AqlValue b = a.clone();
            res->_data[(row - from) * _nrRegs + col] = b;
            cache.insert(make_pair(a,b));
          }
          else {
            res->_data[(row - from) * _nrRegs + col] = it->second;
          }
        }
      }
    }

    return res;
  }
  catch (...) {
    delete res;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief splice multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::splice (std::vector<AqlItemBlock*> const& blocks) {
  TRI_ASSERT(! blocks.empty());

  auto it = blocks.begin();
  TRI_ASSERT(it != blocks.end());
  size_t totalSize = (*it)->size();
  RegisterId nrRegs = (*it)->getNrRegs();

  while (true) {
    if (++it == blocks.end()) {
      break;
    }
    totalSize += (*it)->size();
    TRI_ASSERT((*it)->getNrRegs() == nrRegs);
  }

  TRI_ASSERT(totalSize > 0);
  TRI_ASSERT(nrRegs > 0);

  auto res = new AqlItemBlock(totalSize, nrRegs);
  size_t pos = 0;
  for (it = blocks.begin(); it != blocks.end(); ++it) {
    // handle collections
    if (it == blocks.begin()) {
      // copy collection info over
      for (RegisterId col = 0; col < nrRegs; ++col) {
        res->setDocumentCollection(col, (*it)->getDocumentCollection(col));
      }
    }
    else {
      for (RegisterId col = 0; col < nrRegs; ++col) {
        TRI_ASSERT(res->getDocumentCollection(col) ==
                   (*it)->getDocumentCollection(col));
      }
    }

    TRI_ASSERT((*it) != res);
    size_t const n = (*it)->size();
    for (size_t row = 0; row < n; ++row) {
      for (RegisterId col = 0; col < nrRegs; ++col) {
        // copy over value
        res->setValue(pos + row, col, (*it)->getValue(row, col));
        // delete old value
        (*it)->eraseValue(row, col);
      }
    }
    pos += (*it)->size();
  }

  return res;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

