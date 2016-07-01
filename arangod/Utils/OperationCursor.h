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

#ifndef ARANGOD_UTILS_OPERATION_CURSOR_H
#define ARANGOD_UTILS_OPERATION_CURSOR_H 1

#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "Utils/OperationResult.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

// FORWARD declaration
class IndexIterator;
struct OperationResult;

struct OperationCursor {

 public:
  int                            code;

 private:

  std::unique_ptr<IndexIterator> _indexIterator;
  bool                           _hasMore;
  uint64_t                       _limit;
  uint64_t const                 _originalLimit;
  uint64_t const                 _batchSize;

 public:
  explicit OperationCursor(int code)
      : code(code),
        _hasMore(false),
        _limit(0),
        _originalLimit(0),
        _batchSize(1000) {}

  OperationCursor(IndexIterator* iterator, uint64_t limit, uint64_t batchSize)
      : code(TRI_ERROR_NO_ERROR), 
        _indexIterator(iterator),
        _hasMore(true),
        _limit(limit),  // _limit is modified later on
        _originalLimit(limit),
        _batchSize(batchSize) {
    if (_indexIterator == nullptr) {
      _hasMore = false;
    }
  }

  ~OperationCursor() {}
  
  IndexIterator* indexIterator() const {
    return _indexIterator.get();
  }

  inline bool hasMore() const { return _hasMore; }

  bool successful() const {
    return code == TRI_ERROR_NO_ERROR;
  }
  
  bool failed() const {
    return !successful();
  }

//////////////////////////////////////////////////////////////////////////////
/// @brief Reset the cursor
//////////////////////////////////////////////////////////////////////////////

  void reset();

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements.
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<OperationResult> getMore(uint64_t batchSize = UINT64_MAX,
                                           bool useExternals = true);

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements.
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

  void getMore(std::shared_ptr<OperationResult>&, uint64_t batchSize = UINT64_MAX,
               bool useExternals = true);

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements. mptr variant
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////
 public:
  std::vector<TRI_doc_mptr_t*> getMoreMptr(uint64_t batchSize = UINT64_MAX);

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements. mptr variant
///        Defaults to _batchSize
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
///        NOTE: The result vector handed in will be cleared.
//////////////////////////////////////////////////////////////////////////////

  void getMoreMptr(std::vector<TRI_doc_mptr_t*>& result, uint64_t batchSize = UINT64_MAX);

//////////////////////////////////////////////////////////////////////////////
/// @brief Skip the next toSkip many elements.
///        skipped will be increased by the amount of skipped elements afterwards
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

  int skip(uint64_t, uint64_t&);

};

}

#endif
