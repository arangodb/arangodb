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

#include "ErrorCode.h"

#include <velocypack/Value.h>

#include <ostream>

using namespace arangodb;

auto to_string(::ErrorCode value) -> std::string {
  return std::to_string(value._value);
}

ErrorCode::operator arangodb::velocypack::Value() const noexcept {
  return velocypack::Value(_value);
}

auto operator<<(std::ostream& out, ::ErrorCode const& res) -> std::ostream& {
  return out << static_cast<int>(res);
}
