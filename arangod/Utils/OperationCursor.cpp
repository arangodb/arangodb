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
#include "Logger/Logger.h"
#include "VocBase/MasterPointer.h"

using namespace arangodb;

void OperationCursor::reset() {
  _builder.clear();

  if (_indexIterator != nullptr) {
    _indexIterator->reset();
    _hasMore = true;
    _limit = _originalLimit;
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next default batchSize many elements.
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

int OperationCursor::getMore() {
  return getMore(_batchSize);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements.
///        Check hasMore()==true before using this
///        If useExternals is set to true all elements in the vpack are
///        externals. Otherwise they are inlined.
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

int OperationCursor::getMore(uint64_t batchSize, bool useExternals) {
  // This may throw out of memory
  if (!hasMore()) {
    TRI_ASSERT(false);
    // You requested more even if you should have checked it before.
    return TRI_ERROR_FORBIDDEN;
  }
  // We restart the builder
  _builder.clear();


  VPackArrayBuilder guard(&_builder);
  TRI_doc_mptr_t* mptr = nullptr;
  // TODO: Improve this for baby awareness
  while (batchSize > 0 && _limit > 0 && (mptr = _indexIterator->next()) != nullptr) {
    --batchSize;
    --_limit;
#if 0    
    if (useExternals) {
      _builder.add(VPackValue(mptr->vpack(), VPackValueType::External));
    } else {
#endif      
    _builder.add(VPackSlice(mptr->vpack()));
#if 0      
    }
#endif    
  }
  if (batchSize > 0 || _limit == 0) {
    // Iterator empty, there is no more
    _hasMore = false;
  }

  return TRI_ERROR_NO_ERROR;
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
