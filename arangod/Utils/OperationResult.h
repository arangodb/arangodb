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

#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/Common.h"
#include "Basics/Result.h"

namespace arangodb {

// TODO FIXME -- This class shoulb be based on the arangodb::Result class
struct OperationResult {
  OperationResult() : code(TRI_ERROR_NO_ERROR), wasSynchronous(false) {}

  explicit OperationResult(int code)
      : buffer(std::make_shared<VPackBuffer<uint8_t>>()),
        customTypeHandler(),
        code(code),
        wasSynchronous(false) {
    if (code != TRI_ERROR_NO_ERROR) {
      errorMessage = TRI_errno_string(code);
    }
  }

  OperationResult(OperationResult&& other)
      : buffer(std::move(other.buffer)),
        customTypeHandler(std::move(other.customTypeHandler)),
        errorMessage(std::move(other.errorMessage)),
        code(other.code),
        wasSynchronous(other.wasSynchronous),
        countErrorCodes(std::move(other.countErrorCodes)) {}

  OperationResult& operator=(OperationResult&& other) {
    if (this != &other) {
      buffer = std::move(other.buffer);
      customTypeHandler = std::move(other.customTypeHandler);
      errorMessage = std::move(other.errorMessage);
      code = other.code;
      wasSynchronous = other.wasSynchronous;
      countErrorCodes = std::move(other.countErrorCodes);
    }
    return *this;
  }

  OperationResult(int code, std::string const& message)
      : buffer(std::make_shared<VPackBuffer<uint8_t>>()),
        customTypeHandler(),
        errorMessage(message),
        code(code),
        wasSynchronous(false) {
    TRI_ASSERT(code != TRI_ERROR_NO_ERROR);
  }

  // TODO FIXME -- more and better ctors for creation from Result
  explicit OperationResult(Result const& other)
      : buffer(std::make_shared<VPackBuffer<uint8_t>>()),
        customTypeHandler(),
        errorMessage(other.errorMessage()),
        code(other.errorNumber()),
        wasSynchronous(false) {}

  OperationResult(std::shared_ptr<VPackBuffer<uint8_t>> const& buffer,
                  std::shared_ptr<VPackCustomTypeHandler> const& handler,
                  std::string const& message, int code, bool wasSynchronous)
      : buffer(buffer),
        customTypeHandler(handler),
        errorMessage(message),
        code(code),
        wasSynchronous(wasSynchronous) {}

  OperationResult(std::shared_ptr<VPackBuffer<uint8_t>> const& buffer,
                  std::shared_ptr<VPackCustomTypeHandler> const& handler,
                  std::string const& message, int code, bool wasSynchronous,
                  std::unordered_map<int, size_t> const& countErrorCodes)
      : buffer(buffer),
        customTypeHandler(handler),
        errorMessage(message),
        code(code),
        wasSynchronous(wasSynchronous),
        countErrorCodes(countErrorCodes) {}

  ~OperationResult() {}

  bool successful() const { return code == TRI_ERROR_NO_ERROR; }

  bool failed() const { return !successful(); }

  inline VPackSlice slice() const {
    TRI_ASSERT(buffer != nullptr);
    return VPackSlice(buffer->data());
  }

  // TODO: add a slice that points to either buffer or raw data
  std::shared_ptr<VPackBuffer<uint8_t>> buffer;

  std::shared_ptr<VPackCustomTypeHandler> customTypeHandler;
  std::string errorMessage;
  int code;
  bool wasSynchronous;

  // Executive summary for baby operations: reports all errors that did occur
  // during these operations. Details are stored in the respective positions of
  // the failed documents.
  std::unordered_map<int, size_t> countErrorCodes;
};

}  // namespace arangodb

#endif
