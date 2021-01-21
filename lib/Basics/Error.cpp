////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Error.h"

#include "Basics/error.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <ostream>

using namespace arangodb;

Error::Error() noexcept(noexcept(std::allocator<char>()))
    : _errorNumber(TRI_ERROR_NO_ERROR) {}

Error::Error(int errorNumber) noexcept(noexcept(std::allocator<char>()))
    : _errorNumber(errorNumber) {}

Error::Error(int errorNumber, std::string const& errorMessage)
    : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

Error::Error(int errorNumber, std::string&& errorMessage) noexcept
    : _errorNumber(errorNumber), _errorMessage(std::move(errorMessage)) {}

Error::Error(int errorNumber, std::string_view const& errorMessage)
    : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

Error::Error(int errorNumber, const char* errorMessage)
    : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

Error::Error(Error const& other)
    : _errorNumber(other._errorNumber), _errorMessage(other._errorMessage) {}

Error::Error(Error&& other) noexcept
    : _errorNumber(other._errorNumber),
      _errorMessage(std::move(other._errorMessage)) {}

Error& Error::operator=(Error const& other) {
  _errorNumber = other._errorNumber;
  _errorMessage = other._errorMessage;
  return *this;
}

Error& Error::operator=(Error&& other) noexcept {
  _errorNumber = other._errorNumber;
  _errorMessage = std::move(other._errorMessage);
  return *this;
}

bool Error::ok() const noexcept { return _errorNumber == TRI_ERROR_NO_ERROR; }

bool Error::fail() const noexcept { return !ok(); }

int Error::errorNumber() const noexcept { return _errorNumber; }

bool Error::is(int errorNumber) const noexcept {
  return _errorNumber == errorNumber;
}

bool Error::isNot(int errorNumber) const { return !is(errorNumber); }

Error& Error::reset() { return reset(TRI_ERROR_NO_ERROR); }

Error& Error::reset(int errorNumber) {
  _errorNumber = errorNumber;

  if (!_errorMessage.empty()) {
    _errorMessage.clear();
  }

  return *this;
}

Error& Error::reset(int errorNumber, std::string const& errorMessage) {
  _errorNumber = errorNumber;
  _errorMessage = errorMessage;
  return *this;
}

Error& Error::reset(int errorNumber, std::string&& errorMessage) noexcept {
  _errorNumber = errorNumber;
  _errorMessage = std::move(errorMessage);
  return *this;
}

Error& Error::reset(Error const& other) {
  _errorNumber = other._errorNumber;
  _errorMessage = other._errorMessage;
  return *this;
}

Error& Error::reset(Error&& other) noexcept {
  _errorNumber = other._errorNumber;
  _errorMessage = std::move(other._errorMessage);
  return *this;
}

std::string Error::errorMessage() const& {
  if (!_errorMessage.empty()) {
    return _errorMessage;
  }
  return TRI_errno_string(_errorNumber);
}

std::string Error::errorMessage() && {
  if (!_errorMessage.empty()) {
    return std::move(_errorMessage);
  }
  return TRI_errno_string(_errorNumber);
}

std::ostream& operator<<(std::ostream& out, arangodb::Error const& error) {
  VPackBuilder dump;
  {
    VPackObjectBuilder b(&dump);
    dump.add(StaticStrings::ErrorNum, VPackValue(error.errorNumber()));
    dump.add(StaticStrings::ErrorMessage, VPackValue(error.errorMessage()));
  }
  out << dump.slice().toJson();
  return out;
}
