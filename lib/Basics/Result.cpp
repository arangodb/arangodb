////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

Result::Result() noexcept(noexcept(std::allocator<char>()))
    : _errorNumber(TRI_ERROR_NO_ERROR) {}

Result::Result(int errorNumber) noexcept(noexcept(std::allocator<char>()))
    : _errorNumber(errorNumber) {}

Result::Result(int errorNumber, std::string const& errorMessage)
    : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

Result::Result(int errorNumber, std::string&& errorMessage) noexcept
    : _errorNumber(errorNumber), _errorMessage(std::move(errorMessage)) {}

Result::Result(int errorNumber, std::string_view const& errorMessage)
    : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

Result::Result(int errorNumber, const char* errorMessage)
    : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

Result::Result(Result const& other)
    : _errorNumber(other._errorNumber), _errorMessage(other._errorMessage) {}

Result::Result(Result&& other) noexcept
    : _errorNumber(other._errorNumber),
      _errorMessage(std::move(other._errorMessage)) {}

Result& Result::operator=(Result const& other) {
  _errorNumber = other._errorNumber;
  _errorMessage = other._errorMessage;
  return *this;
}

Result& Result::operator=(Result&& other) noexcept {
  _errorNumber = other._errorNumber;
  _errorMessage = std::move(other._errorMessage);
  return *this;
}

bool Result::ok() const noexcept { return _errorNumber == TRI_ERROR_NO_ERROR; }

bool Result::fail() const noexcept { return !ok(); }

int Result::errorNumber() const noexcept { return _errorNumber; }

bool Result::is(int errorNumber) const noexcept {
  return _errorNumber == errorNumber;
}

bool Result::isNot(int errorNumber) const { return !is(errorNumber); }

Result& Result::reset() { return reset(TRI_ERROR_NO_ERROR); }

Result& Result::reset(int errorNumber) {
  _errorNumber = errorNumber;

  if (!_errorMessage.empty()) {
    _errorMessage.clear();
  }

  return *this;
}

Result& Result::reset(int errorNumber, std::string const& errorMessage) {
  _errorNumber = errorNumber;
  _errorMessage = errorMessage;
  return *this;
}

Result& Result::reset(int errorNumber, std::string&& errorMessage) noexcept {
  _errorNumber = errorNumber;
  _errorMessage = std::move(errorMessage);
  return *this;
}

Result& Result::reset(Result const& other) {
  _errorNumber = other._errorNumber;
  _errorMessage = other._errorMessage;
  return *this;
}

Result& Result::reset(Result&& other) noexcept {
  _errorNumber = other._errorNumber;
  _errorMessage = std::move(other._errorMessage);
  return *this;
}

std::string Result::errorMessage() const& {
  if (!_errorMessage.empty()) {
    return _errorMessage;
  }
  return TRI_errno_string(_errorNumber);
}

std::string Result::errorMessage() && {
  if (!_errorMessage.empty()) {
    return std::move(_errorMessage);
  }
  return TRI_errno_string(_errorNumber);
}

std::ostream& operator<<(std::ostream& out, arangodb::Result const& result) {
  VPackBuilder dump;
  {
    VPackObjectBuilder b(&dump);
    dump.add(StaticStrings::ErrorNum, VPackValue(result.errorNumber()));
    dump.add(StaticStrings::ErrorMessage, VPackValue(result.errorMessage()));
  }
  out << dump.slice().toJson();
  return out;
}
