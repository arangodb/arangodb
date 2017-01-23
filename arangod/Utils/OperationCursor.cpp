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
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

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
/// @brief Get next batchSize many elements. mptr variant
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

std::vector<IndexLookupResult> OperationCursor::getMoreMptr(uint64_t batchSize) {
  std::vector<IndexLookupResult> res;
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

void OperationCursor::getMoreMptr(std::vector<IndexLookupResult>& result,
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
