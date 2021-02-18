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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ResultError.h"

#include "Basics/StaticStrings.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <ostream>

using namespace arangodb;
using namespace arangodb::result;

Error::Error(ErrorCode errorNumber) noexcept(noexcept(decltype(Error::_errorMessage)::allocator_type()))
    : _errorNumber(errorNumber) {}

Error::Error(ErrorCode errorNumber, std::string_view errorMessage)
    : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

auto Error::errorNumber() const noexcept -> ErrorCode { return _errorNumber; }

auto Error::errorMessage() const& noexcept -> std::string_view {
  if (!_errorMessage.empty()) {
    return _errorMessage;
  }
  return TRI_errno_string(_errorNumber);
}

auto Error::errorMessage() && noexcept -> std::string {
  if (!_errorMessage.empty()) {
    return std::move(_errorMessage);
  }
  return std::string{TRI_errno_string(_errorNumber)};
}

auto operator<<(std::ostream& out, arangodb::result::Error const& error) -> std::ostream& {
  VPackBuilder dump;
  {
    VPackObjectBuilder b(&dump);
    dump.add(StaticStrings::ErrorNum, VPackValue(error.errorNumber()));
    dump.add(StaticStrings::ErrorMessage, VPackValue(error.errorMessage()));
  }
  out << dump.slice().toJson();
  return out;
}
