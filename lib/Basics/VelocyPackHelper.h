////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace velocypack {
struct AttributeExcludeHandler;
class AttributeTranslator;
}

namespace basics {

struct VPackHashedSlice {
  arangodb::velocypack::Slice slice;
  uint64_t hash;

  constexpr VPackHashedSlice() noexcept : slice(), hash(0) {}
  VPackHashedSlice(arangodb::velocypack::Slice slice, uint64_t hash) noexcept : slice(slice), hash(hash) {}
  explicit VPackHashedSlice(arangodb::velocypack::Slice slice) : slice(slice), hash(slice.hash()) {}
  
  VPackHashedSlice(VPackHashedSlice const& other) noexcept : slice(other.slice), hash(other.hash) {}
  VPackHashedSlice(VPackHashedSlice&& other) noexcept : slice(other.slice), hash(other.hash) {}
  VPackHashedSlice& operator=(VPackHashedSlice const& other) noexcept { slice = other.slice; hash = other.hash; return *this; }
  VPackHashedSlice& operator=(VPackHashedSlice&& other) noexcept { slice = other.slice; hash = other.hash; return *this; }

  ~VPackHashedSlice() {}
};

class VelocyPackHelper {
 private:
  VelocyPackHelper() = delete;
  ~VelocyPackHelper() = delete;

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief static initializer for all VPack values
  ////////////////////////////////////////////////////////////////////////////////

  static void initialize();
  static void disableAssemblerFunctions();

  static arangodb::velocypack::AttributeExcludeHandler* getExcludeHandler();
  static arangodb::velocypack::AttributeTranslator* getTranslator();

  struct VPackHash {
    size_t operator()(arangodb::velocypack::Slice const&) const;
  };

  struct VPackStringHash {
    size_t operator()(arangodb::velocypack::Slice const&) const noexcept;
  };
  
  struct VPackKeyHash {
    size_t operator()(arangodb::velocypack::Slice const&) const;
  };
  
  struct VPackHashedStringHash {
    size_t operator()(VPackHashedSlice const& slice) const noexcept { return static_cast<size_t>(slice.hash); }
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief equality comparator for VelocyPack values
  ////////////////////////////////////////////////////////////////////////////////

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
  
  /// @brief Comparator that only takes _id/_key into account.
  struct VPackIdEqual {
    bool operator()(arangodb::velocypack::Slice const&,
                    arangodb::velocypack::Slice const&) const;
  };
  
  struct VPackHashedStringEqual {
    bool operator()(VPackHashedSlice const&,
                    VPackHashedSlice const&) const noexcept;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief less comparator for VelocyPack values
  ////////////////////////////////////////////////////////////////////////////////

  template <bool useUtf8>
  struct VPackLess {
    VPackLess(arangodb::velocypack::Options const* options =
                  &arangodb::velocypack::Options::Defaults,
              arangodb::velocypack::Slice const* lhsBase = nullptr,
              arangodb::velocypack::Slice const* rhsBase = nullptr)
        : options(options), lhsBase(lhsBase), rhsBase(rhsBase) {}

    inline bool operator()(arangodb::velocypack::Slice const& lhs,
                           arangodb::velocypack::Slice const& rhs) const {
      return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase,
                                       rhsBase) < 0;
    }

    arangodb::velocypack::Options const* options;
    arangodb::velocypack::Slice const* lhsBase;
    arangodb::velocypack::Slice const* rhsBase;
  };

  template <bool useUtf8>
  struct VPackGreater {
    VPackGreater(arangodb::velocypack::Options const* options =
                     &arangodb::velocypack::Options::Defaults,
                 arangodb::velocypack::Slice const* lhsBase = nullptr,
                 arangodb::velocypack::Slice const* rhsBase = nullptr)
        : options(options), lhsBase(lhsBase), rhsBase(rhsBase) {}

    inline bool operator()(arangodb::velocypack::Slice const& lhs,
                           arangodb::velocypack::Slice const& rhs) const {
      return VelocyPackHelper::compare(lhs, rhs, useUtf8, options, lhsBase,
                                       rhsBase) > 0;
    }

    arangodb::velocypack::Options const* options;
    arangodb::velocypack::Slice const* lhsBase;
    arangodb::velocypack::Slice const* rhsBase;
  };

  template <bool useUtf8>
  struct VPackSorted {
    VPackSorted(bool reverse, arangodb::velocypack::Options const* options =
                                  &arangodb::velocypack::Options::Defaults,
                arangodb::velocypack::Slice const* lhsBase = nullptr,
                arangodb::velocypack::Slice const* rhsBase = nullptr)
        : _reverse(reverse),
          options(options),
          lhsBase(lhsBase),
          rhsBase(rhsBase) {}

    inline bool operator()(arangodb::velocypack::Slice const& lhs,
                           arangodb::velocypack::Slice const& rhs) const {
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

  struct AttributeSorterUTF8 {
    bool operator()(std::string const& l, std::string const& r) const;
  };

  struct AttributeSorterBinary {
    bool operator()(std::string const& l, std::string const& r) const;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a numeric value
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static typename std::enable_if<std::is_signed<T>::value, T>::type getNumericValue(VPackSlice const& slice, T defaultValue) {
    if (slice.isNumber()) {
      return slice.getNumber<T>();
    }
    return defaultValue;
  }
  
  template <typename T>
  static typename std::enable_if<std::is_unsigned<T>::value, T>::type getNumericValue(VPackSlice const& slice, T defaultValue) {
    if (slice.isNumber()) {
      if (slice.isInt() && slice.getInt() < 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot assign negative value to unsigned type");
      }
      if (slice.isDouble() && slice.getDouble() < 0.0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot assign negative value to unsigned type");
      }
      return slice.getNumber<T>();
    }
    return defaultValue;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a numeric sub-element, or a default if it does not exist
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static T getNumericValue(VPackSlice const& slice, char const* name,
                           T defaultValue) {
    TRI_ASSERT(slice.isObject());
    if (!slice.hasKey(name)) {
      return defaultValue;
    }
    VPackSlice sub = slice.get(name);
    if (sub.isNumber()) {
      return sub.getNumber<T>();
    }
    return defaultValue;
  }

  template <typename T>
  static T readNumericValue(VPackSlice info, std::string const& name, T def) {
    if (!info.isObject()) {
      return def;
    }
    return getNumericValue<T>(info, name.c_str(), def);
  }

  template <typename T, typename BaseType>
  static T readNumericValue(VPackSlice info, std::string const& name, T def) {
    if (!info.isObject()) {
      return def;
    }
    // nice extra conversion required for Visual Studio pickyness
    return static_cast<T>(getNumericValue<BaseType>(info, name.c_str(), static_cast<BaseType>(def)));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a boolean sub-element, or a default if it does not exist
  //////////////////////////////////////////////////////////////////////////////

  static bool getBooleanValue(VPackSlice const&, char const*, bool);
  static bool readBooleanValue(VPackSlice info, std::string const& name,
                               bool def) {
    if (!info.isObject()) {
      return def;
    }
    return getBooleanValue(info, name.c_str(), def);
  }


  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string sub-element, or throws if <name> does not exist
  /// or it is not a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string checkAndGetStringValue(VPackSlice const&, char const*);
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensures a sub-element is of type string
  //////////////////////////////////////////////////////////////////////////////

  static std::string checkAndGetStringValue(VPackSlice const&,
                                            std::string const&);
  
  static void ensureStringValue(VPackSlice const&,
                                std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a Numeric sub-element, or throws if <name> does not exist
  /// or it is not a Number
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static T checkAndGetNumericValue(VPackSlice const& slice, char const* name) {
    TRI_ASSERT(slice.isObject());
    if (!slice.hasKey(name)) {
      std::string msg =
          "The attribute '" + std::string(name) + "' was not found.";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
    }
    VPackSlice const sub = slice.get(name);
    if (!sub.isNumber()) {
      std::string msg =
          "The attribute '" + std::string(name) + "' is not a number.";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
    }
    return sub.getNumericValue<T>();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string value, or the default value if it is not a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string getStringValue(VPackSlice const&, std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string sub-element, or the default value if it does not
  /// exist
  /// or it is not a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string getStringValue(VPackSlice, char const*,
                                    std::string const&);
  static std::string getStringValue(VPackSlice, std::string const&,
                                    std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief convert a Object sub value into a uint64
  //////////////////////////////////////////////////////////////////////////////

  static uint64_t stringUInt64(VPackSlice const& slice);
  static uint64_t stringUInt64(VPackSlice const& slice, char const* name) {
    return stringUInt64(slice.get(name));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses a json file to VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  static std::shared_ptr<VPackBuilder> velocyPackFromFile(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief writes a VelocyPack to a file
  //////////////////////////////////////////////////////////////////////////////

  static bool velocyPackToFile(std::string const& filename,
                               VPackSlice const& slice, bool syncFile);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compares two VelocyPack number values
  //////////////////////////////////////////////////////////////////////////////

  static int compareNumberValues(arangodb::velocypack::ValueType,
                                 arangodb::velocypack::Slice lhs,
                                 arangodb::velocypack::Slice rhs);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compares two VelocyPack string values
  //////////////////////////////////////////////////////////////////////////////

  static int compareStringValues(char const* left, VPackValueLength nl, 
                                 char const* right, VPackValueLength nr, 
                                 bool useUTF8);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Compares two VelocyPack slices
  //////////////////////////////////////////////////////////////////////////////

  static int compare(arangodb::velocypack::Slice lhs,
                     arangodb::velocypack::Slice rhs, bool useUTF8,
                     arangodb::velocypack::Options const* options =
                         &arangodb::velocypack::Options::Defaults,
                     arangodb::velocypack::Slice const* lhsBase = nullptr,
                     arangodb::velocypack::Slice const* rhsBase = nullptr);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Merges two VelocyPack Slices
  //////////////////////////////////////////////////////////////////////////////

  static arangodb::velocypack::Builder merge(arangodb::velocypack::Slice const&,
                                             arangodb::velocypack::Slice const&,
                                             bool, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transforms any VelocyPack to a double value. The second parameter
  ///        indicates if the transformation was successful.
  //////////////////////////////////////////////////////////////////////////////

  static double toDouble(VPackSlice const&, bool&);

  // modify a VPack double value in place 
  static void patchDouble(VPackSlice slice, double value);

  static uint64_t hashByAttributes(VPackSlice, std::vector<std::string> const&,
                                   bool, int&, std::string const& key = "");

  static constexpr arangodb::velocypack::Slice NullValue() {
    return arangodb::velocypack::Slice::nullSlice();
  }

  static constexpr arangodb::velocypack::Slice TrueValue() {
    return arangodb::velocypack::Slice::trueSlice();
  }

  static constexpr arangodb::velocypack::Slice FalseValue() {
    return arangodb::velocypack::Slice::falseSlice();
  }

  static constexpr arangodb::velocypack::Slice BooleanValue(bool value) {
    return value ? arangodb::velocypack::Slice::trueSlice() : arangodb::velocypack::Slice::falseSlice();
  }

  static constexpr arangodb::velocypack::Slice ZeroValue() {
    return arangodb::velocypack::Slice::zeroSlice();
  }

  static constexpr arangodb::velocypack::Slice EmptyArrayValue() {
    return arangodb::velocypack::Slice::emptyArraySlice();
  }

  static constexpr arangodb::velocypack::Slice EmptyObjectValue() {
    return arangodb::velocypack::Slice::emptyObjectSlice();
  }
  
  static constexpr arangodb::velocypack::Slice EmptyString() {
    return arangodb::velocypack::Slice("\x40");
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief "constant" global object for illegal slices
  ///        Are used in Array Indexes to distinguish NULL and not existent.
  //////////////////////////////////////////////////////////////////////////////

  static constexpr arangodb::velocypack::Slice IllegalValue() {
    return arangodb::velocypack::Slice::illegalSlice();
  }
  
  static bool hasNonClientTypes(arangodb::velocypack::Slice, bool checkExternals, bool checkCustom);

  static void sanitizeNonClientTypes(arangodb::velocypack::Slice input,
                                     arangodb::velocypack::Slice base,
                                     arangodb::velocypack::Builder& output,
                                     arangodb::velocypack::Options const*,
                                     bool sanitizeExternals, bool sanitizeCustom);

  static VPackBuffer<uint8_t> sanitizeNonClientTypesChecked(
      arangodb::velocypack::Slice,
      VPackOptions const* options = &VPackOptions::Options::Defaults,
      bool sanitizeExternals = true,
      bool sanitizeCustom = true);

  static uint64_t extractIdValue(VPackSlice const& slice);

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
}
}

namespace std {
template <>
struct hash<arangodb::basics::VPackHashedSlice> {
  inline size_t operator()(arangodb::basics::VPackHashedSlice const& slice) const noexcept {
    return slice.hash;
  }
};

template <>
struct equal_to<arangodb::basics::VPackHashedSlice> {
  bool operator()(arangodb::basics::VPackHashedSlice const& lhs,
                  arangodb::basics::VPackHashedSlice const& rhs) const {
    return lhs.slice.equals(rhs.slice);
  }
};

}


//////////////////////////////////////////////////////////////////////////////
/// @brief Simple and limited logging of VelocyPack slices
//////////////////////////////////////////////////////////////////////////////

arangodb::LoggerStream& operator<<(arangodb::LoggerStream&,
                                   arangodb::velocypack::Slice const&);

#endif
