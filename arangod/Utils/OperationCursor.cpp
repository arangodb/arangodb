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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "OperationCursor.h"
#include "OperationResult.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "VocBase/MasterPointer.h"

using namespace arangodb;

void OperationCursor::reset() {
  code = TRI_ERROR_NO_ERROR;

  if (_indexIterator != nullptr) {
    _indexIterator->reset();
    _hasMore = true;
    _limit = _originalLimit;
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements.
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<OperationResult> OperationCursor::getMore(uint64_t batchSize,
                                                          bool useExternals) {
  auto res = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
  getMore(res, batchSize, useExternals);
  return res;
}

void OperationCursor::getMore(std::shared_ptr<OperationResult>& opRes,
                              uint64_t batchSize, bool useExternals) {
  if (opRes == nullptr) {
    // Create a valid pointer if none is given
    auto tmp = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
    opRes.swap(tmp);
  } 
  
  // This may throw out of memory
  if (!hasMore()) {
    TRI_ASSERT(false);
    // You requested more even if you should have checked it before.
    opRes->code = TRI_ERROR_FORBIDDEN;
    return;
  }
  if (batchSize == UINT64_MAX) {
    batchSize = _batchSize;
  }
  
  VPackBuilder builder(opRes->buffer);
  builder.clear();
  try {
    VPackArrayBuilder guard(&builder);
    TRI_doc_mptr_t* mptr = nullptr;
    // TODO: Improve this for baby awareness
    while (batchSize > 0 && _limit > 0 && (mptr = _indexIterator->next()) != nullptr) {
      --batchSize;
      --_limit;
      if (useExternals) {
        builder.addExternal(mptr->vpack());
      } else {
        builder.add(VPackSlice(mptr->vpack()));
      }
    }
    if (batchSize > 0 || _limit == 0) {
      // Iterator empty, there is no more
      _hasMore = false;
    }
    opRes->code = TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& e) {
    opRes->code = e.code();
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements. mptr variant
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_t*> OperationCursor::getMoreMptr(uint64_t batchSize) {
  std::vector<TRI_doc_mptr_t*> res;
  getMoreMptr(res, batchSize);
  return res;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements. mptr variant
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
///        NOTE: The result vector handed in is used to continue index lookups
///              The caller shall NOT modify it.
//////////////////////////////////////////////////////////////////////////////

void OperationCursor::getMoreMptr(std::vector<TRI_doc_mptr_t*>& result,
                                  uint64_t batchSize) {
  if (!hasMore()) {
    TRI_ASSERT(false);
    // You requested more even if you should have checked it before.
    return;
  }
  if (batchSize == UINT64_MAX) {
    batchSize = _batchSize;
  }

  size_t atMost = static_cast<size_t>(batchSize > _limit ? _limit : batchSize);

  _indexIterator->nextBabies(result, atMost);

  TRI_ASSERT(_limit >= atMost);
  _limit -= atMost;

  if (result.empty()) {
    // Index is empty
    _hasMore = false;
    return;
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Skip the next toSkip many elements.
///        skipped will be increased by the amount of skipped elements afterwards
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

int OperationCursor::skip(uint64_t toSkip, uint64_t& skipped) {
  if (!hasMore()) {
    TRI_ASSERT(false);
    // You requested more even if you should have checked it before.
    return TRI_ERROR_FORBIDDEN;
  }

  if (toSkip > _limit) {
    // Short-cut, we jump to the end
    _limit = 0;
    _hasMore = false;
    return TRI_ERROR_NO_ERROR;
  }

  _indexIterator->skip(toSkip, skipped);
  _limit -= skipped;
  if (skipped != toSkip || _limit == 0) {
    _hasMore = false;
  }
  return TRI_ERROR_NO_ERROR;
}
