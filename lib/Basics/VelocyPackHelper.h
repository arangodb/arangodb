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

  
  /// @brief hash computation based on key only comparator for VelocyPack values
  struct VPackKeyHash {
    size_t operator()(arangodb::velocypack::Slice const&) const;
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
  static T getNumericValue(VPackSlice const& slice, T defaultValue) {
    if (slice.isNumber()) {
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a boolean sub-element, or a default if it does not exist
  //////////////////////////////////////////////////////////////////////////////

  static bool getBooleanValue(VPackSlice const&, char const*, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string sub-element, or throws if <name> does not exist
  /// or it is not a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string checkAndGetStringValue(VPackSlice const&, char const*);

  static std::string checkAndGetStringValue(VPackSlice const&,
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

  static uint64_t stringUInt64(VPackSlice const&);

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

  static uint64_t hashByAttributes(VPackSlice, std::vector<std::string> const&,
                                   bool, int&, std::string const& key = "");

  static inline arangodb::velocypack::Slice NullValue() {
    return arangodb::velocypack::Slice::nullSlice();
  }

  static inline arangodb::velocypack::Slice TrueValue() {
    return arangodb::velocypack::Slice::trueSlice();
  }

  static inline arangodb::velocypack::Slice FalseValue() {
    return arangodb::velocypack::Slice::falseSlice();
  }

  static inline arangodb::velocypack::Slice BooleanValue(bool value) {
    if (value) {
      return arangodb::velocypack::Slice::trueSlice();
    }
    return arangodb::velocypack::Slice::falseSlice();
  }

  static inline arangodb::velocypack::Slice ZeroValue() {
    return arangodb::velocypack::Slice::zeroSlice();
  }

  static inline arangodb::velocypack::Slice EmptyArrayValue() {
    return arangodb::velocypack::Slice::emptyArraySlice();
  }

  static inline arangodb::velocypack::Slice EmptyObjectValue() {
    return arangodb::velocypack::Slice::emptyObjectSlice();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief "constant" global object for illegal slices
  ///        Are used in Array Indexes to distinguish NULL and not existent.
  //////////////////////////////////////////////////////////////////////////////

  static inline arangodb::velocypack::Slice IllegalValue() {
    return arangodb::velocypack::Slice::illegalSlice();
  }

  static void SanitizeExternals(arangodb::velocypack::Slice const,
                                arangodb::velocypack::Builder&);

  static bool hasExternals(arangodb::velocypack::Slice const);

  static VPackBuffer<uint8_t> sanitizeExternalsChecked(
      arangodb::velocypack::Slice const,
      VPackOptions const* options = &VPackOptions::Options::Defaults,
      bool checkExternals = true);

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

//////////////////////////////////////////////////////////////////////////////
/// @brief Simple and limited logging of VelocyPack slices
//////////////////////////////////////////////////////////////////////////////

arangodb::LoggerStream& operator<<(arangodb::LoggerStream&,
                                   arangodb::velocypack::Slice const&);

#endif
