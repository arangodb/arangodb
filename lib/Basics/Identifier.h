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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_IDENTIFIER_H
#define ARANGODB_BASICS_IDENTIFIER_H 1

#include <cstdint>
#include <functional>
#include <iosfwd>

namespace arangodb::basics {

/// @brief a typed wrapper to help prevent unintentional casting of identifiers
class Identifier {
 public:
  typedef std::uint64_t BaseType;

 public:
  constexpr Identifier() noexcept : _id(0) {}
  constexpr explicit Identifier(BaseType id) noexcept : _id(id) {}
  Identifier(Identifier const& other) noexcept = default;
  Identifier& operator=(Identifier const& other) noexcept = default;
  Identifier(Identifier&& other) noexcept = default;
  Identifier& operator=(Identifier&& other) noexcept = default;

  /// @brief return the document id
  BaseType id() const noexcept;

  /// @brief return a pointer to the underlying identifier
  BaseType const* data() const noexcept;

  // equivalent to isSet()
  explicit operator bool() const noexcept;

  /// @brief check if two identifiers are equal
  bool operator==(Identifier const& other) const;

  /// @brief check if two identifiers are equal
  bool operator!=(Identifier const& other) const;

  /// @brief check if this identifier is less than another
  bool operator<(Identifier const& other) const;

  /// @brief check if this identifier is at most another
  bool operator<=(Identifier const& other) const;

  /// @brief check if this identifier is greater than another
  bool operator>(Identifier const& other) const;

  /// @brief check if this identifier is at least another
  bool operator>=(Identifier const& other) const;

 private:
  BaseType _id;
};

// Identifier should not be bigger than the BaseType
static_assert(sizeof(Identifier) == sizeof(Identifier::BaseType),
              "invalid size of Identifier");
}  // namespace arangodb::basics

std::ostream& operator<<(std::ostream& s, arangodb::basics::Identifier const& i);

#define DECLARE_HASH_FOR_IDENTIFIER(T)                        \
  namespace std {                                             \
  template <>                                                 \
  struct hash<T> {                                            \
    inline size_t operator()(T const& value) const noexcept { \
      return value.id();                                      \
    }                                                         \
  };                                                          \
  }  // namespace std
DECLARE_HASH_FOR_IDENTIFIER(arangodb::basics::Identifier)

#define DECLARE_EQUAL_FOR_IDENTIFIER(T)                                      \
  namespace std {                                                            \
  template <>                                                                \
  struct equal_to<T> {                                                       \
    bool operator()(T const& lhs, T const& rhs) const { return lhs == rhs; } \
  };                                                                         \
  }  // namespace std
DECLARE_EQUAL_FOR_IDENTIFIER(arangodb::basics::Identifier)

#endif
