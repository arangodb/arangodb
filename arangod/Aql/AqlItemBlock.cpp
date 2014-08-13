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

using namespace triagens::aql;

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
  : _nrItems(nrItems), _nrRegs(nrRegs) {

  TRI_ASSERT(nrItems > 0);  // no, empty AqlItemBlocks are not allowed!

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
/// TODO: dtor is not exception-safe
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock::~AqlItemBlock () {
  for (size_t i = 0; i < _nrItems * _nrRegs; i++) {
    if (! _data[i].isEmpty()) {
      auto it = _valueCount.find(_data[i]);
      if (it != _valueCount.end()) { // if we know it, we are still responsible
        if (--(it->second) == 0) {
          _data[i].destroy();
          try {
            _valueCount.erase(it);
          }
          catch (...) {
          }
        }
      }
      // Note that if we do not know it the thing it has been stolen from us!
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
/// @brief slice/clone, this does a deep copy of all entries
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
/// @brief slice/clone chosen rows for a subset, this does a deep copy
/// of all entries
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::slice (std::vector<size_t>& chosen, 
                                   size_t from, 
                                   size_t to) const {
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
        AqlValue const& a(_data[chosen[row] * _nrRegs + col]);

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
/// @brief steal for a subset, this does not copy the entries, rather,
/// it remembers which it has taken. This is stored in the
/// this by removing the value counts in _valueCount. 
/// It is highly recommended to delete this object right after this
/// operation, because it is unclear, when the values to which our
/// AqlValues point will vanish! In particular, do not use setValue
/// any more.
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::steal (vector<size_t>& chosen, 
                                   size_t from, size_t to) {
  TRI_ASSERT(from < to && to <= chosen.size());

  AqlItemBlock* res = new AqlItemBlock(to - from, _nrRegs);
  for (RegisterId col = 0; col < _nrRegs; col++) {
    res->_docColls[col] = _docColls[col];
  }
  try {
    for (size_t row = from; row < to; row++) {
      for (RegisterId col = 0; col < _nrRegs; col++) {
        AqlValue& a(_data[chosen[row] * _nrRegs + col]);

        if (! a.isEmpty()) {
          steal(a);
          try {
            res->setValue(row - from, col, a);
          }
          catch (...) {
            a.destroy();
          }
          eraseValue(chosen[row], col);
        }
      }
    }
  }
  catch (...) {
    delete res;
    throw;
  }
  return res;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::concatenate (std::vector<AqlItemBlock*> const& blocks) {
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
  try {
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
          if (! (*it)->getValue(row, col).isEmpty()) {
            res->setValue(pos + row, col, (*it)->getValue(row, col));
          }
        }
      }
      (*it)->eraseAll();
      pos += (*it)->size();
    }
  }
  catch (...) {
    delete res;
    throw;
  }

  return res;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

