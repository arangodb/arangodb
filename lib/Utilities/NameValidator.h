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
#include <string>
#include <string_view>

namespace arangodb {

struct NameValidator {
  /// @brief determine whether a data-source name is a system data-source name
  static bool isSystemName(std::string_view name) noexcept;
};

struct DatabaseNameValidator {
  /// @brief maximal database name length, in bytes (old convention, used when
  /// `--database.extended-names-databases=false`)
  static constexpr std::size_t maxNameLengthTraditional = 64;
  /// @brief maximal database name length, in bytes (new convention, used when
  /// `--database.extended-names-databases=true`)
  static constexpr std::size_t maxNameLengthExtended = 128;

  /// @brief maximum length of a database name (in bytes), based on convention
  static constexpr std::size_t maxNameLength(bool extendedNames) noexcept {
    return extendedNames ? maxNameLengthExtended : maxNameLengthTraditional;
  }

  /// @brief checks if a database name is allowed in the given context.
  /// returns true if the name is allowed and false otherwise
  static bool isAllowedName(bool allowSystem, bool extendedNames,
                            std::string_view name) noexcept;
};

struct CollectionNameValidator {
  /// @brief maximal collection name length, in bytes
  static constexpr std::size_t maxNameLengthTraditional = 256;
  static constexpr std::size_t maxNameLengthExtended = maxNameLengthTraditional;

  /// @brief maximum length of a collection name (in bytes), (not yet) based on convention
  static constexpr std::size_t maxNameLength(bool extendedNames) noexcept {
    return extendedNames ? maxNameLengthExtended : maxNameLengthTraditional;
  }

  /// @brief checks if a collection name is allowed in the given context.
  /// returns true if the name is allowed and false otherwise
  static bool isAllowedName(bool allowSystem, bool extendedNames,
                            std::string_view name) noexcept;
};

struct ViewNameValidator {
  /// @brief maximal view name length, in bytes (old convention, used when
  /// `--database.extended-names-views=false`)
  static constexpr std::size_t maxNameLengthTraditional = 64;
  /// @brief maximal view name length, in bytes (new convention, used when
  /// `--database.extended-names-views=true`)
  static constexpr std::size_t maxNameLengthExtended = 256;

  /// @brief maximum length of a view name (in bytes), based on convention
  static constexpr std::size_t maxNameLength(bool extendedNames) noexcept {
    return extendedNames ? maxNameLengthExtended : maxNameLengthTraditional;
  }

  /// @brief checks if a view name is allowed in the given context.
  /// returns true if the name is allowed and false otherwise
  static bool isAllowedName(bool allowSystem, bool extendedNames,
                            std::string_view name) noexcept;
};

struct IndexNameValidator {
  /// @brief maximal index name length, in bytes 
  static constexpr std::size_t maxNameLengthTraditional = 256;
  static constexpr std::size_t maxNameLengthExtended = maxNameLengthTraditional;

  /// @brief maximum length of an index name (in bytes), (not yet) based on convention
  static constexpr std::size_t maxNameLength(bool extendedNames) noexcept {
    return extendedNames ? maxNameLengthExtended : maxNameLengthTraditional;
  }

  /// @brief checks if a index name is allowed in the given context.
  /// returns true if the name is allowed and false otherwise
  static bool isAllowedName(bool extendedNames,
                            std::string_view name) noexcept;
};

struct AnalyzerNameValidator {
  /// @brief maximal analyzer name length, in bytes 
  static constexpr std::size_t maxNameLengthTraditional = 64;
  static constexpr std::size_t maxNameLengthExtended = maxNameLengthTraditional;

  /// @brief maximum length of an analyzer name (in bytes), (not yet) based on convention
  static constexpr std::size_t maxNameLength(bool extendedNames) noexcept {
    return extendedNames ? maxNameLengthExtended : maxNameLengthTraditional;
  }

  /// @brief checks if an analyzer name is allowed in the given context.
  /// returns true if the name is allowed and false otherwise
  static bool isAllowedName(bool extendedNames,
                            std::string_view name) noexcept;
};

}
