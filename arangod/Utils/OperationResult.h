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
#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
  
struct OperationResult {

  OperationResult() 
      : result(), wasSynchronous(false) {}
/*  
  explicit OperationResult(int code) 
      : buffer(std::make_shared<VPackBuffer<uint8_t>>()), 
        customTypeHandler(), 
        code(code), 
        wasSynchronous(false) { 
    if (code != TRI_ERROR_NO_ERROR) {
      errorMessage = TRI_errno_string(code);
    }
  }
  */
  /*
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
*/
  OperationResult(int code, std::string const& message) 
      : result(code, message),
        buffer(std::make_shared<VPackBuffer<uint8_t>>()), 
        customTypeHandler(), 
        wasSynchronous(false) { 
    TRI_ASSERT(code != TRI_ERROR_NO_ERROR);
  }
  
  //TODO FIXME -- more and better ctors for creation from Result
  explicit OperationResult(Result const& other) 
      : result(other),
        buffer(std::make_shared<VPackBuffer<uint8_t>>()), 
        customTypeHandler(), 
        wasSynchronous(false) {}

  OperationResult(std::shared_ptr<VPackBuffer<uint8_t>> const& buffer,
                  std::shared_ptr<VPackCustomTypeHandler> const& handler,
                  std::string const& message, int code, bool wasSynchronous)
      : result(code, message),
        buffer(buffer),
        customTypeHandler(handler),
        wasSynchronous(wasSynchronous) {}

  OperationResult(std::shared_ptr<VPackBuffer<uint8_t>> const& buffer,
                  std::shared_ptr<VPackCustomTypeHandler> const& handler,
                  std::string const& message, int code, bool wasSynchronous,
                  std::unordered_map<int, size_t> const& countErrorCodes)
      : result(code, message),
        buffer(buffer),
        customTypeHandler(handler),
        wasSynchronous(wasSynchronous),
        countErrorCodes(countErrorCodes) {}

  ~OperationResult() {}

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
  bool wasSynchronous;

  // Executive summary for baby operations: reports all errors that did occur
  // during these operations. Details are stored in the respective positions of
  // the failed documents.
  std::unordered_map<int, size_t> countErrorCodes;
};

}

#endif
