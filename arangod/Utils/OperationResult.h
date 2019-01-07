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
#include "Basics/Result.h"
#include "Utils/OperationOptions.h"

#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

struct OperationResult {
  OperationResult() {}

  // create from integer status code
  explicit OperationResult(int code) : result(code) {}
  explicit OperationResult(int code, OperationOptions const& options)
      : result(code), _options(options) {}

  // create from Result
  explicit OperationResult(Result const& other) : result(other) {}
  explicit OperationResult(Result const& other, OperationOptions const& options)
      : result(other), _options(options) {}
  explicit OperationResult(Result&& other) : result(std::move(other)) {}
  explicit OperationResult(Result&& other, OperationOptions const& options)
      : result(std::move(other)), _options(options) {}

  // copy
  OperationResult(OperationResult const& other) = delete;
  OperationResult& operator=(OperationResult const& other) = delete;

  // move
  OperationResult(OperationResult&& other) = default;
  OperationResult& operator=(OperationResult&& other) {
    if (this != &other) {
      result = std::move(other.result);
      buffer = std::move(other.buffer);
      customTypeHandler = std::move(other.customTypeHandler);
      _options = other._options;
      countErrorCodes = std::move(other.countErrorCodes);
    }
    return *this;
  }

  // create result with details
  OperationResult(Result&& result, std::shared_ptr<VPackBuffer<uint8_t>> const& buffer,
                  std::shared_ptr<VPackCustomTypeHandler> const& handler,
                  OperationOptions const& options = {},
                  std::unordered_map<int, size_t> const& countErrorCodes =
                      std::unordered_map<int, size_t>())
      : result(std::move(result)),
        buffer(buffer),
        customTypeHandler(handler),
        _options(options),
        countErrorCodes(countErrorCodes) {
    if (result.ok()) {
      TRI_ASSERT(buffer != nullptr);
      TRI_ASSERT(buffer->data() != nullptr);
    }
  }

  ~OperationResult() = default;

  // Result-like interface
  bool ok() const { return result.ok(); }
  bool fail() const { return result.fail(); }
  int errorNumber() const { return result.errorNumber(); }
  bool is(int errorNumber) const { return result.errorNumber() == errorNumber; }
  bool isNot(int errorNumber) const { return !is(errorNumber); }
  std::string errorMessage() const { return result.errorMessage(); }

  inline VPackSlice slice() const {
    TRI_ASSERT(buffer != nullptr);
    return VPackSlice(buffer->data());
  }

  Result result;
  // TODO: add a slice that points to either buffer or raw data
  std::shared_ptr<VPackBuffer<uint8_t>> buffer;
  std::shared_ptr<VPackCustomTypeHandler> customTypeHandler;
  OperationOptions _options;

  // Executive summary for baby operations: reports all errors that did occur
  // during these operations. Details are stored in the respective positions of
  // the failed documents.
  std::unordered_map<int, size_t> countErrorCodes;
};

}  // namespace arangodb

#endif
