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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_VELOCY_PACK_HELPER_H
#define ARANGODB_BASICS_VELOCY_PACK_HELPER_H 1

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <system_error>
#include <type_traits>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/ValueType.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/system-compiler.h"

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
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

namespace detail {
template <class>
inline constexpr bool always_false_v = false;
template <typename... Ts, std::size_t... Is>
auto unpackTuple(VPackArrayIterator& iter, std::index_sequence<Is...>) -> std::tuple<Ts...> {
  return {  // evaluation order is sequenced https://stackoverflow.com/questions/37254813/evaluation-order-of-elements-in-an-initializer-list
      ([&](VPackSlice slice) {
        TRI_ASSERT(!slice.isNone());
        if constexpr (std::is_integral_v<Ts>) {
          TRI_ASSERT(slice.template isNumber<Ts>());
          return slice.template getNumericValue<Ts>();
        } else if constexpr (std::is_same_v<Ts, double>) {
          TRI_ASSERT(slice.isDouble());
          return slice.getDouble();
        } else if constexpr (std::is_same_v<Ts, std::string>) {
          TRI_ASSERT(slice.isString());
          return slice.copyString();
        } else if constexpr (std::is_same_v<Ts, std::string_view>) {
          TRI_ASSERT(slice.isString());
          return slice.stringView();
        } else if constexpr (std::is_same_v<Ts, VPackStringRef>) {
          TRI_ASSERT(slice.isString());
          return slice.stringRef();
        } else if constexpr (std::is_same_v<Ts, VPackSlice>) {
          return slice;
        } else {
          static_assert(always_false_v<Ts>, "Unhandled value type requested");
        }
      }(*(iter++)))
      ...};
}
}

class VelocyPackHelper {
 private:
  VelocyPackHelper() = delete;
  ~VelocyPackHelper() = delete;

 public:
  /// @brief static initializer for all VPack values
  static void initialize();
  static void disableAssemblerFunctions();

  static arangodb::velocypack::AttributeTranslator* getTranslator();

  struct VPackHash {
    size_t operator()(arangodb::velocypack::Slice const&) const;
  };

  struct VPackStringHash {
    size_t operator()(arangodb::velocypack::Slice const&) const noexcept;
  };

  /// @brief equality comparator for VelocyPack values
  struct VPackEqual {
   private:
    arangodb::velocypack::Options const* _options;

   public:
    VPackEqual() : _options(nullptr) {}
    explicit VPackEqual(arangodb::velocypack::Options const* opts)
        : _options(opts) {}

    bool operator()(arangodb::velocypack::Slice const&,
                    arangodb::velocypack::Slice const&) const;
  };

  struct VPackStringEqual {
    bool operator()(arangodb::velocypack::Slice const&,
                    arangodb::velocypack::Slice const&) const noexcept;
  };

  /// @brief less comparator for VelocyPack values
  template <bool useUtf8>
  struct VPackLess {
    VPackLess(arangodb::velocypack::Options const* options = &arangodb::velocypack::Options::Defaults,
              arangodb::velocypack::Slice const* lhsBase = nullptr,
              arangodb::velocypack::Slice const* rhsBase = nullptr)
        : options(options), lhsBase(lhsBase), rhsBase(rhsBase) {}

    inline bool operator()(arangodb::velocypack::Slice const& lhs,
                           arangodb::velocypack::Slice const& rhs) const {
      return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase, rhsBase) < 0;
    }

    arangodb::velocypack::Options const* options;
    arangodb::velocypack::Slice const* lhsBase;
    arangodb::velocypack::Slice const* rhsBase;
  };

  template <bool useUtf8>
  struct VPackSorted {
    explicit VPackSorted(bool reverse,
                arangodb::velocypack::Options const* options = &arangodb::velocypack::Options::Defaults,
                arangodb::velocypack::Slice const* lhsBase = nullptr,
                arangodb::velocypack::Slice const* rhsBase = nullptr)
        : _reverse(reverse), options(options), lhsBase(lhsBase), rhsBase(rhsBase) {}

    inline bool operator()(arangodb::velocypack::Slice const& lhs,
                           arangodb::velocypack::Slice const& rhs) const {
      if (_reverse) {
        return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase, rhsBase) > 0;
      }
      return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase, rhsBase) < 0;
    }

    bool _reverse;
    arangodb::velocypack::Options const* options;
    arangodb::velocypack::Slice const* lhsBase;
    arangodb::velocypack::Slice const* rhsBase;
  };

  struct AttributeSorterUTF8 {
    bool operator()(std::string const& l, std::string const& r) const;
  };

  struct AttributeSorterUTF8StringRef {
    bool operator()(arangodb::velocypack::StringRef const& l, arangodb::velocypack::StringRef const& r) const;
  };

  struct AttributeSorterBinary {
    bool operator()(std::string const& l, std::string const& r) const noexcept;
  };

  struct AttributeSorterBinaryStringRef {
    bool operator()(arangodb::velocypack::StringRef const& l, arangodb::velocypack::StringRef const& r) const noexcept;
  };


  /// @brief unpacks an array as tuple. Use like this: auto&& [a, b, c] = unpack<size_t, std::string, double>(slice);
  template <typename... Ts>
  static std::tuple<Ts...> unpackTuple(VPackSlice slice) {
    VPackArrayIterator iter(slice);
    return detail::unpackTuple<Ts...>(iter, std::index_sequence_for<Ts...>{});
  }

  template <typename... Ts>
  static std::tuple<Ts...> unpackTuple(VPackArrayIterator& iter) {
    return detail::unpackTuple<Ts...>(iter, std::index_sequence_for<Ts...>{});
  }

  /// @brief returns a numeric value
  template <typename T>
  static typename std::enable_if<std::is_signed<T>::value, T>::type getNumericValue(
      VPackSlice const& slice, T defaultValue) {
    if (slice.isNumber()) {
      return slice.getNumber<T>();
    }
    return defaultValue;
  }

  template <typename T>
  static typename std::enable_if<std::is_unsigned<T>::value, T>::type getNumericValue(
      VPackSlice const& slice, T defaultValue) {
    if (slice.isNumber()) {
      if (slice.isInt() && slice.getInt() < 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "cannot assign negative value to unsigned type");
      }
      if (slice.isDouble() && slice.getDouble() < 0.0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "cannot assign negative value to unsigned type");
      }
      return slice.getNumber<T>();
    }
    return defaultValue;
  }

  /// @brief returns a numeric sub-element, or a default if it does not exist
  template <typename T, typename NumberType = T, typename NameType>
  static T getNumericValue(VPackSlice const& slice, NameType const& name, T defaultValue) {
    if (slice.isObject()) {
      VPackSlice sub = slice.get(name);
      if (sub.isNumber()) {
        return static_cast<T>(sub.getNumber<NumberType>());
      }
    }
    return defaultValue;
  }

  /// @brief returns a boolean sub-element, or a default if it does not exist
  template <typename NameType>
  static bool getBooleanValue(VPackSlice const& slice, NameType const& name, bool defaultValue) {
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
  static std::string checkAndGetStringValue(VPackSlice const&, char const*);

  /// @brief ensures a sub-element is of type string
  static std::string checkAndGetStringValue(VPackSlice const&, std::string const&);

  static void ensureStringValue(VPackSlice const&, std::string const&);

  /// @brief returns a Numeric sub-element, or throws if <name> does not exist
  /// or it is not a Number
  template <typename T>
  static T checkAndGetNumericValue(VPackSlice const& slice, char const* name) {
    TRI_ASSERT(slice.isObject());
    VPackSlice const sub = slice.get(name);
    if (sub.isNone()) {
      std::string msg =
          "The attribute '" + std::string(name) + "' was not found.";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
    }
    if (!sub.isNumber()) {
      std::string msg =
          "The attribute '" + std::string(name) + "' is not a number.";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
    }
    return sub.getNumericValue<T>();
  }

  /// @return string ref, or the defaultValue if slice is not a string
  static arangodb::velocypack::StringRef getStringRef(
      arangodb::velocypack::Slice slice,
      arangodb::velocypack::StringRef const& defaultValue) noexcept {
    if (slice.isExternal()) {
      slice = arangodb::velocypack::Slice(reinterpret_cast<uint8_t const*>(slice.getExternal()));
    }

    if (slice.isString()) {
      return arangodb::velocypack::StringRef(slice);
    }
    return defaultValue;
  }

  /// @return string ref, or the defaultValue if slice[key] is not a string
  template <typename T>
  static arangodb::velocypack::StringRef getStringRef(
      arangodb::velocypack::Slice slice, T const& key,
      arangodb::velocypack::StringRef const& defaultValue) noexcept {
    if (slice.isExternal()) {
      slice = arangodb::velocypack::Slice(reinterpret_cast<uint8_t const*>(slice.getExternal()));
    }

    if (slice.isObject()) {
      return getStringRef(slice.get(key), defaultValue);
    }
    return defaultValue;
  }

  /// @brief returns a string value, or the default value if it is not a string
  static std::string getStringValue(VPackSlice slice, std::string const& defaultValue);

  /// @brief returns a string sub-element, or the default value if it does not
  /// exist or it is not a string
  template <typename T>
  static std::string getStringValue(VPackSlice slice, T const& name, std::string const& defaultValue) {
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
  static bool velocyPackToFile(std::string const& filename,
                               VPackSlice slice, bool syncFile);

  /// @brief compares two VelocyPack number values
  static int compareNumberValues(arangodb::velocypack::ValueType,
                                 arangodb::velocypack::Slice lhs,
                                 arangodb::velocypack::Slice rhs);

  /// @brief compares two VelocyPack string values
  static int compareStringValues(char const* left, VPackValueLength nl,
                                 char const* right, VPackValueLength nr, bool useUTF8);

  /// @brief Compares two VelocyPack slices
  /// returns 0 if the two slices are equal, < 0 if lhs < rhs, and > 0 if rhs > lhs
  static int compare(arangodb::velocypack::Slice lhs,
                     arangodb::velocypack::Slice rhs, bool useUTF8,
                     arangodb::velocypack::Options const* options = &arangodb::velocypack::Options::Defaults,
                     arangodb::velocypack::Slice const* lhsBase = nullptr,
                     arangodb::velocypack::Slice const* rhsBase = nullptr) ADB_WARN_UNUSED_RESULT;

  /// @brief Compares two VelocyPack slices for equality
  /// returns true if the slices are equal, false otherwise
  static bool equal(arangodb::velocypack::Slice lhs,
                    arangodb::velocypack::Slice rhs, bool useUTF8,
                    arangodb::velocypack::Options const* options = &arangodb::velocypack::Options::Defaults,
                    arangodb::velocypack::Slice const* lhsBase = nullptr,
                    arangodb::velocypack::Slice const* rhsBase = nullptr) {
    return compare(lhs, rhs, useUTF8, options, lhsBase, rhsBase) == 0;
  }

  /// @brief Transforms any VelocyPack to a double value. The second parameter
  ///        indicates if the transformation was successful.
  static double toDouble(VPackSlice const&, bool&);

  // modify a VPack double value in place
  static void patchDouble(VPackSlice slice, double value);

  static bool hasNonClientTypes(arangodb::velocypack::Slice,
                                bool checkExternals, bool checkCustom);

  static void sanitizeNonClientTypes(arangodb::velocypack::Slice input,
                                     arangodb::velocypack::Slice base,
                                     arangodb::velocypack::Builder& output,
                                     arangodb::velocypack::Options const*,
                                     bool sanitizeExternals, bool sanitizeCustom,
                                     bool allowUnindexed = false);

  static VPackBuffer<uint8_t> sanitizeNonClientTypesChecked(
      arangodb::velocypack::Slice,
      VPackOptions const* options = &VPackOptions::Options::Defaults,
      bool sanitizeExternals = true, bool sanitizeCustom = true);

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
}  // namespace basics
}  // namespace arangodb

namespace std {

template <>
struct less<arangodb::velocypack::StringRef> {
  bool operator()(arangodb::velocypack::StringRef const& lhs,
                  arangodb::velocypack::StringRef const& rhs) const noexcept {
    return lhs.compare(rhs) < 0;
  }
};

}  // namespace std

/// @brief Simple and limited logging of VelocyPack slices
arangodb::LoggerStream& operator<<(arangodb::LoggerStream&,
                                   arangodb::velocypack::Slice const&);

#endif
