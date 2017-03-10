////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_RESULT_H
#define ARANGODB_BASICS_RESULT_H 1

#include "Basics/Common.h"

namespace arangodb {
class Result {
 public:
  Result() : _errorNumber(TRI_ERROR_NO_ERROR) {}

  Result(int errorNumber)
      : _errorNumber(errorNumber),
        _errorMessage(TRI_errno_string(errorNumber)) {}

  Result(int errorNumber, std::string const& errorMessage)
      : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

  Result(int errorNumber, std::string&& errorMessage)
      : _errorNumber(errorNumber), _errorMessage(std::move(errorMessage)) {}

  virtual ~Result() {}

 public:
  bool ok() const { return _errorNumber == TRI_ERROR_NO_ERROR; }
  int errorNumber() const { return _errorNumber; }

  // the default implementations is const, but sub-classes might
  // really do more work to compute.

  virtual std::string errorMessage() { return _errorMessage; }

 protected:
  int _errorNumber;
  std::string _errorMessage;
};
}

#endif
