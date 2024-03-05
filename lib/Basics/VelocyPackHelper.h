////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#include <absl/strings/str_cat.h>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Exception.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/ValueType.h>

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"
#include "Basics/voc-errors.h"
#include "Inspection/VPack.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {
class LoggerStream;
namespace velocypack {
struct AttributeExcludeHandler;
class AttributeTranslator;
}  // namespace velocypack

namespace basics {

struct VPackHashedSlice {
  arangodb::velocypack::Slice slice;
  uint64_t hash;

  constexpr VPackHashedSlice() noexcept : slice(), hash(0) {}
  VPackHashedSlice(arangodb::velocypack::Slice slice, uint64_t hash) noexcept
      : slice(slice), hash(hash) {}
  explicit VPackHashedSlice(arangodb::velocypack::Slice slice)
      : slice(slice), hash(slice.hash()) {}

  VPackHashedSlice(VPackHashedSlice const& other) noexcept
      : slice(other.slice), hash(other.hash) {}
  VPackHashedSlice(VPackHashedSlice&& other) noexcept
      : slice(other.slice), hash(other.hash) {}
  // cppcheck-suppress operatorEqVarError
  VPackHashedSlice& operator=(VPackHashedSlice const& other) noexcept {
    slice = other.slice;
    hash = other.hash;
    return *this;
  }
  // cppcheck-suppress operatorEqVarError
  VPackHashedSlice& operator=(VPackHashedSlice&& other) noexcept {
    slice = other.slice;
    hash = other.hash;
    return *this;
  }

  ~VPackHashedSlice() = default;
};

class VelocyPackHelper {
 private:
  VelocyPackHelper() = delete;
  ~VelocyPackHelper() = delete;

 public:
  /// @brief static initializer for all VPack values
  static void initialize();

  static arangodb::velocypack::AttributeTranslator* getTranslator();

  struct VPackHash {
    size_t operator()(arangodb::velocypack::Slice slice) const;
  };

  struct VPackStringHash {
    size_t operator()(arangodb::velocypack::Slice slice) const noexcept;
  };

  /// @brief equality comparator for VelocyPack values
  struct VPackEqual {
   private:
    arangodb::velocypack::Options const* _options;

   public:
    VPackEqual() : _options(nullptr) {}
    explicit VPackEqual(arangodb::velocypack::Options const* opts)
        : _options(opts) {}

    bool operator()(arangodb::velocypack::Slice lhs,
                    arangodb::velocypack::Slice rhs) const;
  };

  struct VPackStringEqual {
    bool operator()(arangodb::velocypack::Slice lhs,
                    arangodb::velocypack::Slice rhs) const noexcept;
  };

  /// @brief less comparator for VelocyPack values
  template<bool useUtf8>
  struct VPackLess {
    VPackLess(arangodb::velocypack::Options const* options =
                  &arangodb::velocypack::Options::Defaults,
              arangodb::velocypack::Slice const* lhsBase = nullptr,
              arangodb::velocypack::Slice const* rhsBase = nullptr)
        : options(options), lhsBase(lhsBase), rhsBase(rhsBase) {}

    bool operator()(arangodb::velocypack::Slice lhs,
                    arangodb::velocypack::Slice rhs) const {
      return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase,
                                       rhsBase) < 0;
    }

    arangodb::velocypack::Options const* options;
    arangodb::velocypack::Slice const* lhsBase;
    arangodb::velocypack::Slice const* rhsBase;
  };

  template<bool useUtf8>
  struct VPackSorted {
    explicit VPackSorted(bool reverse,
                         arangodb::velocypack::Options const* options =
                             &arangodb::velocypack::Options::Defaults,
                         arangodb::velocypack::Slice const* lhsBase = nullptr,
                         arangodb::velocypack::Slice const* rhsBase = nullptr)
        : _reverse(reverse),
          options(options),
          lhsBase(lhsBase),
          rhsBase(rhsBase) {}

    bool operator()(arangodb::velocypack::Slice lhs,
                    arangodb::velocypack::Slice rhs) const {
      if (_reverse) {
        return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase,
                                         rhsBase) > 0;
      }
      return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase,
                                       rhsBase) < 0;
    }

    bool _reverse;
    arangodb::velocypack::Options const* options;
    arangodb::velocypack::Slice const* lhsBase;
    arangodb::velocypack::Slice const* rhsBase;
  };

  struct AttributeSorterUTF8StringView {
    bool operator()(std::string_view l, std::string_view r) const;
  };

  struct AttributeSorterBinaryStringView {
    bool operator()(std::string_view l, std::string_view r) const noexcept;
  };

  /// @brief unpacks an array as tuple. Use like this: auto&& [a, b, c] =
  /// unpack<size_t, std::string, double>(slice);
  template<typename... Ts>
  static std::tuple<Ts...> unpackTuple(VPackSlice slice) {
    return slice.template unpackTuple<Ts...>();
  }

  template<typename... Ts>
  static std::tuple<Ts...> unpackTuple(VPackArrayIterator& iter) {
    return iter.template unpackPrefixAsTuple<Ts...>();
  }

  /// @brief returns a numeric value
  template<typename T>
  static typename std::enable_if<std::is_signed<T>::value, T>::type
  getNumericValue(VPackSlice slice, T defaultValue) {
    if (slice.isNumber()) {
      try {
        return slice.getNumber<T>();
      } catch (VPackException const& ex) {
        if (ex.errorCode() == VPackException::ExceptionType::NumberOutOfRange) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                         "invalid value for signed integer");
        }
        throw;
      }
    }
    return defaultValue;
  }

  template<typename T>
  static typename std::enable_if<std::is_unsigned<T>::value, T>::type
  getNumericValue(VPackSlice slice, T defaultValue) {
    if (slice.isNumber()) {
      if (slice.isInt() && slice.getInt() < 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "cannot assign negative value to unsigned type");
      }
      if (slice.isDouble() && slice.getDouble() < 0.0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "cannot assign negative value to unsigned type");
      }
      try {
        return slice.getNumber<T>();
      } catch (VPackException const& ex) {
        if (ex.errorCode() == VPackException::ExceptionType::NumberOutOfRange) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                         "invalid value for unsigned number");
        }
        throw;
      }
    }
    return defaultValue;
  }

  /// @brief returns a numeric sub-element, or a default if it does not exist
  template<typename T, typename NumberType = T, typename NameType>
  static T getNumericValue(VPackSlice slice, NameType const& name,
                           T defaultValue);

  /// @brief returns a boolean sub-element, or a default if it does not exist or
  /// is not boolean
  template<typename NameType>
  static bool getBooleanValue(VPackSlice slice, NameType const& name,
                              bool defaultValue) {
    if (slice.isObject()) {
      VPackSlice sub = slice.get(name);
      if (sub.isBoolean()) {
        return sub.getBoolean();
      }
    }
    return defaultValue;
  }

  /// @brief returns a string sub-element, or throws if <name> does not exist
  /// or it is not a string
  static std::string checkAndGetStringValue(VPackSlice slice,
                                            std::string_view name);

  static void ensureStringValue(VPackSlice slice, std::string_view name);

  /// @brief returns a Numeric sub-element, or throws if <name> does not exist
  /// or it is not a Number
  template<typename T>
  static T checkAndGetNumericValue(VPackSlice slice, std::string_view name) {
    TRI_ASSERT(slice.isObject());
    VPackSlice sub = slice.get(name);
    if (sub.isNone()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("attribute '", name, "' was not found"));
    }
    if (!sub.isNumber()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("attribute '", name, "' is not a number"));
    }
    try {
      return sub.getNumericValue<T>();
    } catch (VPackException const& ex) {
      if (ex.errorCode() == VPackException::ExceptionType::NumberOutOfRange) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("invalid value for numeric attribute '", name, "'"));
      }
      throw;
    }
  }

  /// @return string view, or the defaultValue if slice is not a string
  static std::string_view getStringView(
      arangodb::velocypack::Slice slice,
      std::string_view defaultValue) noexcept {
    if (slice.isExternal()) {
      slice = arangodb::velocypack::Slice(
          reinterpret_cast<uint8_t const*>(slice.getExternal()));
    }

    if (slice.isString()) {
      return slice.stringView();
    }
    return defaultValue;
  }

  /// @return string view, or the defaultValue if slice[key] is not a string
  template<typename T>
  static std::string_view getStringView(
      arangodb::velocypack::Slice slice, T const& key,
      std::string_view defaultValue) noexcept {
    if (slice.isExternal()) {
      slice = arangodb::velocypack::Slice(
          reinterpret_cast<uint8_t const*>(slice.getExternal()));
    }

    if (slice.isObject()) {
      return getStringView(slice.get(key), defaultValue);
    }
    return defaultValue;
  }

  /// @brief returns a string value, or the default value if it is not a string
  static std::string getStringValue(VPackSlice slice,
                                    std::string const& defaultValue);

  /// @brief returns a string sub-element, or the default value if it does not
  /// exist or it is not a string
  template<typename T>
  static std::string getStringValue(VPackSlice slice, T const& name,
                                    std::string const& defaultValue) {
    if (slice.isExternal()) {
      slice = VPackSlice(reinterpret_cast<uint8_t const*>(slice.getExternal()));
    }

    if (slice.isObject()) {
      VPackSlice sub = slice.get(name);
      if (sub.isString()) {
        return sub.copyString();
      }
    }
    return defaultValue;
  }

  /// @brief convert an Object sub value into a uint64
  static uint64_t stringUInt64(VPackSlice slice);

  template<typename T>
  static uint64_t stringUInt64(VPackSlice slice, T const& name) {
    return stringUInt64(slice.get(name));
  }

  /// @brief parses a json file to VelocyPack
  static VPackBuilder velocyPackFromFile(std::string const&);

  /// @brief writes a VelocyPack to a file
  static bool velocyPackToFile(std::string const& filename, VPackSlice slice,
                               bool syncFile);

  /// @brief compares two VelocyPack number values
  static int compareNumberValues(arangodb::velocypack::ValueType,
                                 arangodb::velocypack::Slice lhs,
                                 arangodb::velocypack::Slice rhs);

  /// @brief compares two VelocyPack string values
  static int compareStringValues(char const* left, VPackValueLength nl,
                                 char const* right, VPackValueLength nr,
                                 bool useUTF8);

  /// @brief Compares two VelocyPack slices
  /// returns 0 if the two slices are equal, < 0 if lhs < rhs, and > 0 if rhs >
  /// lhs
  static int compare(arangodb::velocypack::Slice lhs,
                     arangodb::velocypack::Slice rhs, bool useUTF8,
                     arangodb::velocypack::Options const* options =
                         &arangodb::velocypack::Options::Defaults,
                     arangodb::velocypack::Slice const* lhsBase = nullptr,
                     arangodb::velocypack::Slice const* rhsBase = nullptr)
      ADB_WARN_UNUSED_RESULT;

  /// @brief Compares two VelocyPack slices for equality
  /// returns true if the slices are equal, false otherwise
  static bool equal(arangodb::velocypack::Slice lhs,
                    arangodb::velocypack::Slice rhs, bool useUTF8,
                    arangodb::velocypack::Options const* options =
                        &arangodb::velocypack::Options::Defaults,
                    arangodb::velocypack::Slice const* lhsBase = nullptr,
                    arangodb::velocypack::Slice const* rhsBase = nullptr) {
    return compare(lhs, rhs, useUTF8, options, lhsBase, rhsBase) == 0;
  }

  static bool hasNonClientTypes(arangodb::velocypack::Slice,
                                bool checkExternals, bool checkCustom);

  static void sanitizeNonClientTypes(arangodb::velocypack::Slice input,
                                     arangodb::velocypack::Slice base,
                                     arangodb::velocypack::Builder& output,
                                     arangodb::velocypack::Options const*,
                                     bool sanitizeExternals,
                                     bool sanitizeCustom,
                                     bool allowUnindexed = false);

  static uint64_t extractIdValue(VPackSlice const& slice);

  static arangodb::velocypack::Options strictRequestValidationOptions;
  static arangodb::velocypack::Options looseRequestValidationOptions;

  static uint8_t const KeyAttribute = 0x31;
  static uint8_t const RevAttribute = 0x32;
  static uint8_t const IdAttribute = 0x33;
  static uint8_t const FromAttribute = 0x34;
  static uint8_t const ToAttribute = 0x35;

  static uint8_t const AttributeBase = 0x30;

  static_assert(KeyAttribute < RevAttribute,
                "invalid value for _key attribute");
  static_assert(RevAttribute < IdAttribute, "invalid value for _rev attribute");
  static_assert(IdAttribute < FromAttribute, "invalid value for _id attribute");
  static_assert(FromAttribute < ToAttribute,
                "invalid value for _from attribute");
};

template<typename T, typename NumberType, typename NameType>
T VelocyPackHelper::getNumericValue(VPackSlice slice, NameType const& name,
                                    T defaultValue) {
  if (slice.isObject()) {
    VPackSlice sub = slice.get(name);
    if (sub.isNumber()) {
      try {
        return static_cast<T>(sub.getNumber<NumberType>());
      } catch (VPackException const& ex) {
        if (ex.errorCode() == VPackException::ExceptionType::NumberOutOfRange) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("invalid value for numeric attribute '", name, "'"));
        }
        throw;
      }
    }
  }
  return defaultValue;
}

/// @brief specializations for ErrorCode, shortcut to avoid back-and-forth
/// casts from and to int.
template<>
inline ErrorCode
VelocyPackHelper::getNumericValue<ErrorCode, ErrorCode, std::string_view>(
    VPackSlice slice, std::string_view const& name, ErrorCode defaultValue) {
  return ErrorCode{
      getNumericValue<int>(slice, name, static_cast<int>(defaultValue))};
}

namespace velocypackhelper {

template<typename T, typename = void>
struct HasToVelocyPack : std::false_type {};
// Check whether a `T::toVelocyPack(Builder&)` exists:
template<typename T>
struct HasToVelocyPack<T, decltype(std::declval<T>().toVelocyPack(
                              std::declval<velocypack::Builder&>()))>
    : std::true_type {};
template<typename T>
constexpr bool HasToVelocyPack_v = HasToVelocyPack<T>::value;

/// @brief Convert any object which provides a `toVelocyPack(Builder&)` method
/// to a JSON string, by first converting it to VelocyPack.
///
/// The enable_if part would have liked to be a
///   requires requires(T thing, velocypack::Builder builder) {
///     thing.toVelocyPack(builder);
///   }
/// constraint (or HasToVelocyPack an equivalent concept), but AppleClang
/// doesn't like C++20.
template<typename T>
std::string toJson(T const& thing) {
  if constexpr (HasToVelocyPack_v<T>) {
    auto builder = velocypack::Builder();
    thing.toVelocyPack(builder);
    return builder.toJson();
  } else {
    static_assert(
        inspection::detail::IsInspectable<T,
                                          inspection::VPackSaveInspector<>>(),
        "No toVelocyPack or inspectable");
    velocypack::Builder builder;
    velocypack::serialize(builder, thing);
    return builder.toJson();
  }
}
}  // namespace velocypackhelper

}  // namespace basics
}  // namespace arangodb

/// @brief Simple and limited logging of VelocyPack slices
arangodb::LoggerStream& operator<<(arangodb::LoggerStream&,
                                   arangodb::velocypack::Slice const&);
