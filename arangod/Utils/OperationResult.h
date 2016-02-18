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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_OPERATION_RESULT_H
#define ARANGOD_UTILS_OPERATION_RESULT_H 1

#include "Basics/Common.h"

#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

struct OperationResult {
  OperationResult() : buffer(), customTypeHandler(nullptr), code(TRI_ERROR_NO_ERROR), wasSynchronous(false) {}
  OperationResult(std::shared_ptr<VPackBuffer<uint8_t>> buffer, VPackCustomTypeHandler* handler) : buffer(buffer), customTypeHandler(handler), code(TRI_ERROR_NO_ERROR), wasSynchronous(false) {}
  OperationResult(int code, std::shared_ptr<VPackBuffer<uint8_t>> buffer) : buffer(buffer), customTypeHandler(nullptr), code(code), wasSynchronous(false) {}
  OperationResult(std::shared_ptr<VPackBuffer<uint8_t>> buffer, bool wasSynchronous) : buffer(buffer), customTypeHandler(nullptr), code(TRI_ERROR_NO_ERROR), wasSynchronous(wasSynchronous) {}
  explicit OperationResult(std::shared_ptr<VPackBuffer<uint8_t>> buffer) : OperationResult(TRI_ERROR_NO_ERROR, buffer) {}
  explicit OperationResult(int code) : OperationResult(code, nullptr) { }

  ~OperationResult() {
    // TODO: handle destruction of customTypeHandler
  }

  bool successful() const {
    return code == TRI_ERROR_NO_ERROR;
  }
  
  bool failed() const {
    return !successful();
  }

  VPackSlice slice() const {
    TRI_ASSERT(buffer != nullptr); 
    return VPackSlice(buffer->data());
  }

  std::shared_ptr<VPackBuffer<uint8_t>> buffer;
  VPackCustomTypeHandler* customTypeHandler;
  int code;
  bool wasSynchronous;
};

}

#endif
