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
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

// FORWARD declaration
class IndexIterator;

struct OperationCursor : public OperationResult {

 private:

  std::shared_ptr<IndexIterator> _indexIterator;
  arangodb::velocypack::Builder  _builder;
  bool                           _hasMore;
  uint64_t                       _limit;
  uint64_t                       _batchSize;

 public:

  explicit OperationCursor(int code) 
      : OperationResult(code), _hasMore(false), _limit(0), _batchSize(1000) { 
  }

  OperationCursor(int code, std::string const& message) 
      : OperationResult(code, message), _hasMore(false), _limit(0), _batchSize(1000) {
  }

  OperationCursor(std::shared_ptr<VPackBuffer<uint8_t>> buffer,
                  std::shared_ptr<VPackCustomTypeHandler> handler,
                  std::string const& message,
                  int code,
                  bool wasSynchronous)
      : OperationResult(buffer, handler, message, code, wasSynchronous),
        _hasMore(false),
        _limit(0),
        _batchSize(1000) {
  }

  OperationCursor(std::shared_ptr<VPackCustomTypeHandler> handler, IndexIterator* iterator,
                  uint64_t limit, uint64_t batchSize)
      : OperationResult(std::make_shared<VPackBuffer<uint8_t>>(), handler, "",
                        TRI_ERROR_NO_ERROR, false),
        _indexIterator(iterator),
        _builder(buffer),
        _hasMore(true),
        _limit(limit),
        _batchSize(batchSize) {
          if (_indexIterator == nullptr) {
            _hasMore = false;
          }
        }

  ~OperationCursor() {
  }
  
  IndexIterator* indexIterator() const {
    return _indexIterator.get();
  }

  bool hasMore() const {
    return _hasMore;
  }

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next batchSize many elements.
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

  int getMore(uint64_t batchSize);

//////////////////////////////////////////////////////////////////////////////
/// @brief Get next default batchSize many elements.
///        Check hasMore()==true before using this
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

  int getMore();
};

}

#endif
