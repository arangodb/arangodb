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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>

namespace arangodb {

namespace velocypack {
class StringRef;
}  // namespace velocypack

struct DatabaseNameValidator {

  /// @brief maximal database name length, in bytes (old convention, used when
  /// `--database.allow-unicode-names-databases=false`)
  static constexpr std::size_t maxNameLength = 64;
  /// @brief maximal database name length, in bytes (new convention, used when
  /// `--database.allow-unicode-names-databases=true`)
  static constexpr std::size_t maxNameLengthUnicode = 256;

  /// @brief checks if a database name is allowed
  /// returns true if the name is allowed and false otherwise
  static bool isAllowedName(bool allowSystem, bool allowUnicode,
                            velocypack::StringRef const& name) noexcept;
};

}
