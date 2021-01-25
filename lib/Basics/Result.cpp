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
#include "Basics/error.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::result;

Result::Result(int errorNumber)
    : _error(errorNumber == TRI_ERROR_NO_ERROR ? nullptr : std::make_unique<Error>(errorNumber)) {
}

Result::Result(int errorNumber, std::string const& errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, errorMessage)) {}

Result::Result(int errorNumber, std::string&& errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, std::move(errorMessage))) {}

Result::Result(int errorNumber, std::string_view const& errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, errorMessage)) {}

Result::Result(int errorNumber, const char* errorMessage)
    : _error(errorNumber == TRI_ERROR_NO_ERROR
                 ? nullptr
                 : std::make_unique<Error>(errorNumber, errorMessage)) {}

Result::Result(Result const& other)
    : _error(other._error == nullptr ? nullptr : std::make_unique<Error>(*other._error)) {}

Result::Result(Result&& other) noexcept : _error(std::move(other._error)) {}

auto Result::operator=(Result const& other) -> Result& {
  _error = other._error == nullptr ? nullptr : std::make_unique<Error>(*other._error);
  return *this;
}

auto Result::operator=(Result&& other) noexcept -> Result& {
  _error = std::move(other._error);
  return *this;
}

auto Result::ok() const noexcept -> bool { return _error == nullptr; }

auto Result::fail() const noexcept -> bool { return !ok(); }

auto Result::errorNumber() const noexcept -> int {
  if (_error == nullptr) {
    return TRI_ERROR_NO_ERROR;
  } else {
    return _error->errorNumber();
  }
}

auto Result::is(int errorNumber) const noexcept -> bool {
  return this->errorNumber() == errorNumber;
}

auto Result::isNot(int errorNumber) const -> bool { return !is(errorNumber); }

auto Result::reset() -> Result& {
  _error.reset();
  return *this;
}

auto Result::reset(int errorNumber) -> Result& {
  if (errorNumber == TRI_ERROR_NO_ERROR) {
    _error = nullptr;
  } else {
    _error = std::make_unique<Error>(errorNumber);
  }

  return *this;
}

auto Result::reset(int errorNumber, std::string const& errorMessage) -> Result& {
  if (errorNumber == TRI_ERROR_NO_ERROR) {
    _error = nullptr;
  } else {
    _error = std::make_unique<Error>(errorNumber, errorMessage);
  }

  return *this;
}

Result& Result::reset(int errorNumber, std::string_view errorMessage) {
  return reset(errorNumber, std::string{errorMessage});
}

Result& Result::reset(int errorNumber, const char* errorMessage) {
  return reset(errorNumber, std::string{errorMessage});
}

auto Result::reset(int errorNumber, std::string&& errorMessage) noexcept -> Result& {
  if (errorNumber == TRI_ERROR_NO_ERROR) {
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

auto Result::errorMessage() const -> std::string_view {
  if (_error == nullptr) {
    return {};
  } else {
    return _error->errorMessage();
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
