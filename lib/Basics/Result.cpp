////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include <ostream>

#include "Result.h"

#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::result;

Result::Result(ErrorCode errorNumber)
    : _error(errorNumber == TRI_ERROR_NO_ERROR ? nullptr : std::make_unique<Error>(errorNumber)) {
}

Result::Result(ErrorCode errorNumber, std::string const& errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, errorMessage)) {
  TRI_ASSERT(errorNumber != TRI_ERROR_NO_ERROR || errorMessage.empty());
}

Result::Result(ErrorCode errorNumber, std::string&& errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, std::move(errorMessage))) {
  TRI_ASSERT(errorNumber != TRI_ERROR_NO_ERROR || errorMessage.empty());
}

Result::Result(ErrorCode errorNumber, std::string_view errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, errorMessage)) {
  TRI_ASSERT(errorNumber != TRI_ERROR_NO_ERROR || errorMessage.empty());
}

Result::Result(ErrorCode errorNumber, const char* errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, errorMessage)) {
  TRI_ASSERT(errorNumber != TRI_ERROR_NO_ERROR || 0 == strcmp("", errorMessage));
}

Result::Result(Result const& other)
    : _error(other._error == nullptr ? nullptr : std::make_unique<Error>(*other._error)) {}

auto Result::operator=(Result const& other) -> Result& {
  _error = other._error == nullptr ? nullptr : std::make_unique<Error>(*other._error);
  return *this;
}

auto Result::ok() const noexcept -> bool {
  return errorNumber() == TRI_ERROR_NO_ERROR;
}

auto Result::fail() const noexcept -> bool { return !ok(); }

auto Result::errorNumber() const noexcept -> ErrorCode {
  if (_error == nullptr) {
    return TRI_ERROR_NO_ERROR;
  } else {
    return _error->errorNumber();
  }
}

auto Result::is(ErrorCode errorNumber) const noexcept -> bool {
  return this->errorNumber() == errorNumber;
}

auto Result::isNot(ErrorCode errorNumber) const noexcept -> bool {
  return !is(errorNumber);
}

auto Result::reset() noexcept -> Result& {
  _error.reset();
  return *this;
}

auto Result::reset(ErrorCode errorNumber) -> Result& {
  return reset(errorNumber, std::string{});
}

auto Result::reset(ErrorCode errorNumber, std::string_view errorMessage) -> Result& {
  return reset(errorNumber, std::string{errorMessage});
}

auto Result::reset(ErrorCode errorNumber, const char* errorMessage) -> Result& {
  return reset(errorNumber, std::string{errorMessage});
}

auto Result::reset(ErrorCode errorNumber, std::string&& errorMessage) -> Result& {
  if (errorNumber == TRI_ERROR_NO_ERROR) {
    // The error message will be ignored
    TRI_ASSERT(errorMessage.empty());
    _error = nullptr;
  } else {
    _error = std::make_unique<Error>(errorNumber, std::move(errorMessage));
  }

  return *this;
}

auto Result::reset(Result const& other) -> Result& { return *this = other; }

auto Result::reset(Result&& other) noexcept -> Result& {
  return *this = std::move(other);
}

auto Result::errorMessage() const& noexcept -> std::string_view {
  if (_error == nullptr) {
    // Return a view of the empty string, not a nullptr!
    return {""};
  } else {
    return _error->errorMessage();
  }
}

auto Result::errorMessage() && noexcept -> std::string {
  if (_error == nullptr) {
    return {};
  } else {
    return std::move(*_error).errorMessage();
  }
}

auto operator<<(std::ostream& out, arangodb::Result const& result) -> std::ostream& {
  VPackBuilder dump;
  {
    VPackObjectBuilder b(&dump);
    dump.add(StaticStrings::ErrorNum, VPackValue(result.errorNumber()));
    dump.add(StaticStrings::ErrorMessage, VPackValue(result.errorMessage()));
  }
  out << dump.slice().toJson();
  return out;
}
