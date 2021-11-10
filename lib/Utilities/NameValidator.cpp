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

#include "NameValidator.h"

#include <velocypack/Utf8Helper.h>

#include <cstdint>
#include <string>

#include "Basics/debugging.h"

namespace arangodb {

/// @brief determine whether a name is a system object name
bool NameValidator::isSystemName(std::string_view name) noexcept {
  return !name.empty() && name[0] == '_';
}

/// @brief checks if a database name is valid
/// returns true if the name is allowed and false otherwise
bool DatabaseNameValidator::isAllowedName(bool allowSystem, bool extendedNames,
                                          std::string_view name) noexcept {
  std::size_t length = 0;

  for (char const* ptr = name.data(); length < name.size(); ++ptr, ++length) {
    unsigned char c = static_cast<unsigned char>(*ptr);
    bool ok = true;

    if (extendedNames) {
      // forward slashes are disallowed inside database names because we use the
      // forward slash for splitting _everywhere_
      ok &= (c != '/');

      // colons are disallowed inside database names. they are used to separate
      // database names from analyzer names in some analyzer keys
      ok &= (c != ':');

      // non visible characters below ASCII code 32 (control characters) not
      // allowed, including '\0'
      ok &= (c >= 32U);
    } else {
      if (length == 0) {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (allowSystem && c == '_');
      } else {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') ||
              (c == '-') || (c >= '0' && c <= '9');
      }
    }

    if (!ok) {
      return false;
    }
  }

  if (extendedNames && !name.empty()) {
    unsigned char c = static_cast<unsigned char>(name[0]);
    // a database name must not start with a digit, because then it can be
    // confused with numeric database ids
    bool ok = (c < '0' || c > '9');
    // a database name must not start with an underscore unless it is the system
    // database (which is not created via any checked API)
    ok &= (c != '_' || allowSystem);

    // a database name must not start with a dot, because this is used for
    // hidden agency entries
    ok &= (c != '.');

    // leading spaces are not allowed
    ok &= (c != ' ');

    // trailing spaces are not allowed
    c = static_cast<unsigned char>(name.back());
    ok &= (c != ' ');

    // new naming convention allows Unicode characters. we need to
    // make sure everything is valid UTF-8 now.
    ok &= velocypack::Utf8Helper::isValidUtf8(
        reinterpret_cast<std::uint8_t const*>(name.data()), name.size());

    if (!ok) {
      return false;
    }
  }

  // database names must be within the expected length limits
  return (length > 0 && length <= maxNameLength(extendedNames));
}

/// @brief checks if a collection name is valid
/// returns true if the name is allowed and false otherwise
bool CollectionNameValidator::isAllowedName(bool allowSystem,
                                            bool extendedNames,
                                            std::string_view name) noexcept {
  std::size_t length = 0;

  for (char const* ptr = name.data(); length < name.size(); ++ptr, ++length) {
    unsigned char c = static_cast<unsigned char>(*ptr);
    bool ok = true;

    if (extendedNames) {
      // forward slashes are disallowed inside collection names because we use
      // the forward slash for splitting _everywhere_
      ok &= (c != '/');

      // non visible characters below ASCII code 32 (control characters) not
      // allowed, including '\0'
      ok &= (c >= 32U);

      if (length == 0) {
        // a collection name must not start with a digit, because then it can be
        // confused with numeric collection ids
        ok &= (c < '0' || c > '9');

        // a collection name must not start with an underscore unless it is the
        // system collection
        ok &= (c != '_' || allowSystem);

        // finally, a collection name must not start with a dot, because this is
        // used for hidden agency entries
        ok &= (c != '.');
      }
    } else {
      if (length == 0) {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (allowSystem && c == '_');
      } else {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') ||
              (c == '-') || (c >= '0' && c <= '9');
      }
    }

    if (!ok) {
      return false;
    }
  }

  if (extendedNames &&
      !velocypack::Utf8Helper::isValidUtf8(
          reinterpret_cast<std::uint8_t const*>(name.data()), name.size())) {
    // new naming convention allows Unicode characters. we need to
    // make sure everything is valid UTF-8 now.
    return false;
  }

  // collection names must be within the expected length limits
  return (length > 0 && length <= maxNameLength(extendedNames));
}

/// @brief checks if a view name is valid
/// returns true if the name is allowed and false otherwise
bool ViewNameValidator::isAllowedName(bool allowSystem, bool extendedNames,
                                      std::string_view name) noexcept {
  std::size_t length = 0;

  for (char const* ptr = name.data(); length < name.size(); ++ptr, ++length) {
    unsigned char c = static_cast<unsigned char>(*ptr);
    bool ok = true;

    if (extendedNames) {
      // forward slashes are disallowed inside view names because we use the
      // forward slash for splitting _everywhere_
      ok &= (c != '/');

      // non visible characters below ASCII code 32 (control characters) not
      // allowed, including '\0'
      ok &= (c >= 32U);

      if (length == 0) {
        // a view name must not start with a digit, because then it can be
        // confused with numeric view ids
        ok &= (c < '0' || c > '9');

        // a view name must not start with an underscore (unless it is a system
        // view)
        ok &= (c != '_' || allowSystem);

        // finally, a view name must not start with a dot, because this is used
        // for hidden agency entries
        ok &= (c != '.');
      }
    } else {
      if (length == 0) {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (allowSystem && c == '_');
      } else {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') ||
              (c == '-') || (c >= '0' && c <= '9');
      }
    }

    if (!ok) {
      return false;
    }
  }

  if (extendedNames &&
      !velocypack::Utf8Helper::isValidUtf8(
          reinterpret_cast<std::uint8_t const*>(name.data()), name.size())) {
    // new naming convention allows Unicode characters. we need to
    // make sure everything is valid UTF-8 now.
    return false;
  }

  // view names must be within the expected length limits
  return (length > 0 && length <= maxNameLength(extendedNames));
}

/// @brief checks if an index name is valid
/// returns true if the name is allowed and false otherwise
bool IndexNameValidator::isAllowedName(bool extendedNames,
                                       std::string_view name) noexcept {
  std::size_t length = 0;

  for (char const* ptr = name.data(); length < name.size(); ++ptr, ++length) {
    unsigned char c = static_cast<unsigned char>(*ptr);
    bool ok = true;

    if (extendedNames) {
      // forward slashes are disallowed inside index names because we use the
      // forward slash for splitting _everywhere_
      ok &= (c != '/');

      // non visible characters below ASCII code 32 (control characters) not
      // allowed, including '\0'
      ok &= (c >= 32U);

      if (length == 0) {
        // an index name must not start with a digit, because then it can be
        // confused with numeric index ids
        ok &= (c < '0' || c > '9');
      }
    } else {
      if (length == 0) {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
      } else {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') ||
              (c == '-') || (c >= '0' && c <= '9');
      }
    }

    if (!ok) {
      return false;
    }
  }

  if (extendedNames &&
      !velocypack::Utf8Helper::isValidUtf8(
          reinterpret_cast<std::uint8_t const*>(name.data()), name.size())) {
    // new naming convention allows Unicode characters. we need to
    // make sure everything is valid UTF-8 now.
    return false;
  }

  // index names must be within the expected length limits
  return (length > 0 && length <= maxNameLength(extendedNames));
}

/// @brief checks if an analyzer name is valid
/// returns true if the name is allowed and false otherwise
bool AnalyzerNameValidator::isAllowedName(bool extendedNames,
                                          std::string_view name) noexcept {
  std::size_t length = 0;

  for (char const* ptr = name.data(); length < name.size(); ++ptr, ++length) {
    unsigned char c = static_cast<unsigned char>(*ptr);
    bool ok = true;

    if (extendedNames) {
      // forward slashes are disallowed inside analyzer names because we use the
      // forward slash for splitting _everywhere_
      ok &= (c != '/');

      // colons are used to separate database names from analyzer names
      ok &= (c != ':');

      // non visible characters below ASCII code 32 (control characters) not
      // allowed, including '\0'
      ok &= (c >= 32U);

      if (length == 0) {
        // an analyzer name must not start with a digit, because then it can be
        // confused with numeric ids
        ok &= (c < '0' || c > '9');
      }
    } else {
      if (length == 0) {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
      } else {
        ok &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') ||
              (c == '-') || (c >= '0' && c <= '9');
      }
    }

    if (!ok) {
      return false;
    }
  }

  if (extendedNames &&
      !velocypack::Utf8Helper::isValidUtf8(
          reinterpret_cast<std::uint8_t const*>(name.data()), name.size())) {
    // new naming convention allows Unicode characters. we need to
    // make sure everything is valid UTF-8 now.
    return false;
  }

  // analyzer names must be within the expected length limits
  return (length > 0 && length <= maxNameLength(extendedNames));
}

}  // namespace arangodb
