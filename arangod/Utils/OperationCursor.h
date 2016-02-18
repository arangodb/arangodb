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
#include "Utils/OperationResult.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"

#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

struct OperationCursor : public OperationResult {

 private:

  std::unique_ptr<IndexIterator> _indexIterator;
  bool                           _hasMore;

 public:

  explicit OperationCursor(int code) 
      : OperationResult(code), _hasMore(false) { 
  }

  OperationCursor(int code, std::string const& message) 
      : OperationResult(code, message), _hasMore(false) {
  }

  OperationCursor(std::shared_ptr<VPackBuffer<uint8_t>> buffer,
                  VPackCustomTypeHandler* handler,
                  std::string const& message,
                  int code,
                  bool wasSynchronous)
      : OperationResult(buffer, handler, message, code, wasSynchronous),
        _hasMore(true) {
  }

  ~OperationCursor() {
    // TODO: handle destruction of customTypeHandler
  }

  bool hasMore() {
    return _hasMore;
  }

  int getMore(uint64_t count);
};

}

#endif
