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

#include "VelocyPackHelper.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

#include <velocypack/AttributeTranslator.h>
#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/velocypack-common.h>

extern "C" {
unsigned long long XXH64(const void* input, size_t length,
                         unsigned long long seed);
}

using namespace arangodb;
using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

static std::unique_ptr<VPackAttributeTranslator> Translator;
static std::unique_ptr<VPackAttributeExcludeHandler> ExcludeHandler;
static std::unique_ptr<VPackCustomTypeHandler> CustomTypeHandler;

// a default custom type handler that prevents throwing exceptions when
// custom types are encountered during Slice.toJson() and family
struct DefaultCustomTypeHandler final : public VPackCustomTypeHandler {
  void dump(VPackSlice const&, VPackDumper* dumper,
            VPackSlice const&) override {
    LOG(WARN) << "DefaultCustomTypeHandler called";
    dumper->appendString("hello from CustomTypeHandler");
  }
  std::string toString(VPackSlice const&, VPackOptions const*,
                       VPackSlice const&) override {
    LOG(WARN) << "DefaultCustomTypeHandler called";
    return "hello from CustomTypeHandler";
  }
};

// attribute exclude handler for skipping over system attributes
struct SystemAttributeExcludeHandler final
    : public VPackAttributeExcludeHandler {
  bool shouldExclude(VPackSlice const& key, int nesting) override final {
    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);

    if (p == nullptr || *p != '_' || keyLength < 3 || keyLength > 5 ||
        nesting > 0) {
      // keep attribute
      return true;
    }

    // exclude these attributes (but not _key!)
    if ((keyLength == 3 &&
         memcmp(p, "_id", static_cast<size_t>(keyLength)) == 0) ||
        (keyLength == 4 &&
         memcmp(p, "_rev", static_cast<size_t>(keyLength)) == 0) ||
        (keyLength == 3 &&
         memcmp(p, "_to", static_cast<size_t>(keyLength)) == 0) ||
        (keyLength == 5 &&
         memcmp(p, "_from", static_cast<size_t>(keyLength)) == 0)) {
      return true;
    }

    // keep attribute
    return false;
  }
};

/// @brief static initializer for all VPack values
void VelocyPackHelper::initialize() {
  LOG(TRACE) << "initializing vpack";

  // initialize attribute translator
  Translator.reset(new VPackAttributeTranslator);

  // these attribute names will be translated into short integer values
  Translator->add(StaticStrings::KeyString, KeyAttribute - AttributeBase);
  Translator->add(StaticStrings::RevString, RevAttribute - AttributeBase);
  Translator->add(StaticStrings::IdString, IdAttribute - AttributeBase);
  Translator->add(StaticStrings::FromString, FromAttribute - AttributeBase);
  Translator->add(StaticStrings::ToString, ToAttribute - AttributeBase);

  Translator->seal();

  // set the attribute translator in the global options
  VPackOptions::Defaults.attributeTranslator = Translator.get();
  VPackOptions::Defaults.unsupportedTypeBehavior =
      VPackOptions::ConvertUnsupportedType;

  CustomTypeHandler.reset(new DefaultCustomTypeHandler);

  VPackOptions::Defaults.customTypeHandler = CustomTypeHandler.get();
  VPackOptions::Defaults.escapeUnicode = false;  // false here, but will be set
                                                 // when converting to JSON for
                                                 // HTTP xfer

  // run quick selfs test with the attribute translator
  TRI_ASSERT(
      VPackSlice(Translator->translate(StaticStrings::KeyString)).getUInt() ==
      KeyAttribute - AttributeBase);
  TRI_ASSERT(
      VPackSlice(Translator->translate(StaticStrings::RevString)).getUInt() ==
      RevAttribute - AttributeBase);
  TRI_ASSERT(
      VPackSlice(Translator->translate(StaticStrings::IdString)).getUInt() ==
      IdAttribute - AttributeBase);
  TRI_ASSERT(
      VPackSlice(Translator->translate(StaticStrings::FromString)).getUInt() ==
      FromAttribute - AttributeBase);
  TRI_ASSERT(
      VPackSlice(Translator->translate(StaticStrings::ToString)).getUInt() ==
      ToAttribute - AttributeBase);

  TRI_ASSERT(VPackSlice(Translator->translate(KeyAttribute - AttributeBase))
                 .copyString() == StaticStrings::KeyString);
  TRI_ASSERT(VPackSlice(Translator->translate(RevAttribute - AttributeBase))
                 .copyString() == StaticStrings::RevString);
  TRI_ASSERT(VPackSlice(Translator->translate(IdAttribute - AttributeBase))
                 .copyString() == StaticStrings::IdString);
  TRI_ASSERT(VPackSlice(Translator->translate(FromAttribute - AttributeBase))
                 .copyString() == StaticStrings::FromString);
  TRI_ASSERT(VPackSlice(Translator->translate(ToAttribute - AttributeBase))
                 .copyString() == StaticStrings::ToString);

  // initialize exclude handler for system attributes
  ExcludeHandler.reset(new SystemAttributeExcludeHandler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief turn off assembler optimizations in vpack
////////////////////////////////////////////////////////////////////////////////

void VelocyPackHelper::disableAssemblerFunctions() {
  arangodb::velocypack::disableAssemblerFunctions();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the (global) attribute exclude handler instance
////////////////////////////////////////////////////////////////////////////////

arangodb::velocypack::AttributeExcludeHandler*
VelocyPackHelper::getExcludeHandler() {
  return ExcludeHandler.get();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the (global) attribute translator instance
////////////////////////////////////////////////////////////////////////////////

arangodb::velocypack::AttributeTranslator* VelocyPackHelper::getTranslator() {
  return Translator.get();
}

bool VelocyPackHelper::AttributeSorterUTF8::operator()(
    std::string const& l, std::string const& r) const {
  // use UTF-8-based comparison of attribute names
  return TRI_compare_utf8(l.c_str(), l.size(), r.c_str(), r.size()) < 0;
}

bool VelocyPackHelper::AttributeSorterBinary::operator()(
    std::string const& l, std::string const& r) const {
  // use binary comparison of attribute names
  size_t cmpLength = (std::min)(l.size(), r.size());
  int res = memcmp(l.c_str(), r.c_str(), cmpLength);
  if (res < 0) {
    return true;
  }
  if (res == 0) {
    return l.size() < r.size();
  }
  return false;
}

size_t VelocyPackHelper::VPackHash::operator()(VPackSlice const& slice) const {
  return static_cast<size_t>(slice.normalizedHash());
};

size_t VelocyPackHelper::VPackStringHash::operator()(
    VPackSlice const& slice) const noexcept {
  return static_cast<size_t>(slice.hashString());
};

size_t VelocyPackHelper::VPackKeyHash::operator()(
    VPackSlice const& slice) const {
  return static_cast<size_t>(slice.get(StaticStrings::KeyString).hashString());
};



bool VelocyPackHelper::VPackEqual::operator()(VPackSlice const& lhs,
                                              VPackSlice const& rhs) const {
  return VelocyPackHelper::compare(lhs, rhs, false, _options) == 0;
};

bool VelocyPackHelper::VPackStringEqual::operator()(VPackSlice const& lhs,
                                                    VPackSlice const& rhs) const
    noexcept {
  auto const lh = lhs.head();
  auto const rh = rhs.head();

  if (lh != rh) {
    return false;
  }

  VPackValueLength size;
  if (lh == 0xbf) {
    // long UTF-8 String
    size = static_cast<VPackValueLength>(
        velocypack::readInteger<VPackValueLength>(lhs.begin() + 1, 8));
    if (size !=
        static_cast<VPackValueLength>(
            velocypack::readInteger<VPackValueLength>(rhs.begin() + 1, 8))) {
      return false;
    }
    return (memcmp(lhs.start() + 1 + 8, rhs.start() + 1 + 8,
                   static_cast<size_t>(size)) == 0);
  }

  size = static_cast<VPackValueLength>(lh - 0x40);
  return (memcmp(lhs.start() + 1, rhs.start() + 1, static_cast<size_t>(size)) ==
          0);
};

bool VelocyPackHelper::VPackIdEqual::operator()(VPackSlice const& lhs,
                                                VPackSlice const& rhs) const {
  if (lhs.get(StaticStrings::IdString) != rhs.get(StaticStrings::IdString)) {
    // short-cut, we are in a different collection
    return false;
  }
  // Same collection now actually compare the key
  VPackSlice lhsKey = lhs.get(StaticStrings::KeyString);
  VPackSlice rhsKey = rhs.get(StaticStrings::KeyString);
  auto const lh = lhsKey.head();
  auto const rh = rhsKey.head();

  if (lh != rh) {
    return false;
  }

  VPackValueLength size;
  if (lh == 0xbf) {
    // long UTF-8 String
    size = static_cast<VPackValueLength>(
        velocypack::readInteger<VPackValueLength>(lhsKey.begin() + 1, 8));
    if (size !=
        static_cast<VPackValueLength>(
            velocypack::readInteger<VPackValueLength>(rhsKey.begin() + 1, 8))) {
      return false;
    }
    return (memcmp(lhsKey.start() + 1 + 8, rhsKey.start() + 1 + 8,
                   static_cast<size_t>(size)) == 0);
  }

  size = static_cast<VPackValueLength>(lh - 0x40);
  return (memcmp(lhsKey.start() + 1, rhsKey.start() + 1, static_cast<size_t>(size)) ==
          0);
};



static int TypeWeight(VPackSlice const& slice) {
  switch (slice.type()) {
    case VPackValueType::MinKey:
      return -99;  // must be lowest
    case VPackValueType::Illegal:
      return -1;
    case VPackValueType::None:
    case VPackValueType::Null:
      return 0;
    case VPackValueType::Bool:
      return 1;
    case VPackValueType::Double:
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt:
    case VPackValueType::UTCDate:
    case VPackValueType::BCD:
      return 2;
    case VPackValueType::String:
    case VPackValueType::Binary:
    case VPackValueType::Custom:
      // custom type is used for _id (which is a string)
      return 3;
    case VPackValueType::Array:
      return 4;
    case VPackValueType::Object:
      return 5;
    case VPackValueType::External:
      return TypeWeight(slice.resolveExternal());
    case VPackValueType::MaxKey:
      return 99;  // must be highest
    default:
      // All other values have equal weight
      return 0;
  }
}

int VelocyPackHelper::compareNumberValues(VPackValueType lhsType,
                                          VPackSlice lhs, VPackSlice rhs) {
  if (lhsType == rhs.type()) {
    // both types are equal
    if (lhsType == VPackValueType::Int || lhsType == VPackValueType::SmallInt) {
      // use exact comparisons. no need to cast to double
      int64_t l = lhs.getInt();
      int64_t r = rhs.getInt();
      if (l == r) {
        return 0;
      }
      return (l < r ? -1 : 1);
    }

    if (lhsType == VPackValueType::UInt) {
      // use exact comparisons. no need to cast to double
      uint64_t l = lhs.getUInt();
      uint64_t r = rhs.getUInt();
      if (l == r) {
        return 0;
      }
      return (l < r ? -1 : 1);
    }
    // fallthrough to double comparison
  }

  double left = lhs.getNumericValue<double>();
  double right = rhs.getNumericValue<double>();
  if (left == right) {
    return 0;
  }
  return (left < right ? -1 : 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default if it is does not exist
////////////////////////////////////////////////////////////////////////////////

bool VelocyPackHelper::getBooleanValue(VPackSlice const& slice,
                                       char const* name, bool defaultValue) {
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey(name)) {
    return defaultValue;
  }
  VPackSlice const& sub = slice.get(name);

  if (sub.isBoolean()) {
    return sub.getBool();
  }

  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or throws if <name> does not exist
/// or it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string VelocyPackHelper::checkAndGetStringValue(VPackSlice const& slice,
                                                     char const* name) {
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey(name)) {
    std::string msg =
        "The attribute '" + std::string(name) + "' was not found.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }
  VPackSlice const sub = slice.get(name);
  if (!sub.isString()) {
    std::string msg =
        "The attribute '" + std::string(name) + "' is not a string.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }
  return sub.copyString();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or throws if <name> does not exist
/// or it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string VelocyPackHelper::checkAndGetStringValue(VPackSlice const& slice,
                                                     std::string const& name) {
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey(name)) {
    std::string msg = "The attribute '" + name + "' was not found.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }
  VPackSlice const sub = slice.get(name);
  if (!sub.isString()) {
    std::string msg = "The attribute '" + name + "' is not a string.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }
  return sub.copyString();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string value, or the default value if it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string VelocyPackHelper::getStringValue(VPackSlice const& slice,
                                             std::string const& defaultValue) {
  if (!slice.isString()) {
    return defaultValue;
  }
  return slice.copyString();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or the default value if it does not
/// exist
/// or it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string VelocyPackHelper::getStringValue(VPackSlice slice, char const* name,
                                             std::string const& defaultValue) {
  if (slice.isExternal()) {
    slice = VPackSlice(slice.getExternal());
  }
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey(name)) {
    return defaultValue;
  }
  VPackSlice const sub = slice.get(name);
  if (!sub.isString()) {
    return defaultValue;
  }
  return sub.copyString();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or the default value if it does not
/// exist
/// or it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string VelocyPackHelper::getStringValue(VPackSlice slice,
                                             std::string const& name,
                                             std::string const& defaultValue) {
  if (slice.isExternal()) {
    slice = VPackSlice(slice.getExternal());
  }
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey(name)) {
    return defaultValue;
  }
  VPackSlice const sub = slice.get(name);
  if (!sub.isString()) {
    return defaultValue;
  }
  return sub.copyString();
}

uint64_t VelocyPackHelper::stringUInt64(VPackSlice const& slice) {
  if (slice.isString()) {
    return arangodb::basics::StringUtils::uint64(slice.copyString());
  }
  if (slice.isNumber()) {
    return slice.getNumericValue<uint64_t>();
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json file to VelocyPack
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> VelocyPackHelper::velocyPackFromFile(
    std::string const& path) {
  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, path.c_str(), &length);
  if (content != nullptr) {
    // The Parser might throw;
    std::shared_ptr<VPackBuilder> b;
    try {
      auto b = VPackParser::fromJson(reinterpret_cast<uint8_t const*>(content),
                                     length);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, content);
      return b;
    } catch (...) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, content);
      throw;
    }
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
}

static bool PrintVelocyPack(int fd, VPackSlice const& slice,
                            bool appendNewline) {
  if (slice.isNone()) {
    // sanity check
    return false;
  }

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  arangodb::basics::VPackStringBufferAdapter bufferAdapter(
      buffer.stringBuffer());
  try {
    VPackDumper dumper(&bufferAdapter);
    dumper.dump(slice);
  } catch (...) {
    // Writing failed
    return false;
  }

  if (buffer.length() == 0) {
    // should not happen
    return false;
  }

  if (appendNewline) {
    // add the newline here so we only need one write operation in the ideal
    // case
    buffer.appendChar('\n');
  }

  char const* p = buffer.begin();
  size_t n = buffer.length();

  while (0 < n) {
    ssize_t m = TRI_WRITE(fd, p, (TRI_write_t)n);

    if (m <= 0) {
      return false;
    }

    n -= m;
    p += m;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a VelocyPack to a file
////////////////////////////////////////////////////////////////////////////////

bool VelocyPackHelper::velocyPackToFile(std::string const& filename,
                                        VPackSlice const& slice,
                                        bool syncFile) {
  std::string const tmp = filename + ".tmp";

  // remove a potentially existing temporary file
  if (TRI_ExistsFile(tmp.c_str())) {
    TRI_UnlinkFile(tmp.c_str());
  }

  int fd = TRI_CREATE(tmp.c_str(),
                      O_CREAT | O_TRUNC | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot create json file '" << tmp
             << "': " << TRI_LAST_ERROR_STR;
    return false;
  }

  if (!PrintVelocyPack(fd, slice, true)) {
    TRI_CLOSE(fd);
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot write to json file '" << tmp
             << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());
    return false;
  }

  if (syncFile) {
    LOG(TRACE) << "syncing tmp file '" << tmp << "'";

    if (!TRI_fsync(fd)) {
      TRI_CLOSE(fd);
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG(ERR) << "cannot sync saved json '" << tmp
               << "': " << TRI_LAST_ERROR_STR;
      TRI_UnlinkFile(tmp.c_str());
      return false;
    }
  }

  int res = TRI_CLOSE(fd);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot close saved file '" << tmp
             << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());
    return false;
  }

  res = TRI_RenameFile(tmp.c_str(), filename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG(ERR) << "cannot rename saved file '" << tmp << "' to '" << filename
             << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());

    return false;
  }

  return true;
}

int VelocyPackHelper::compare(VPackSlice lhs, VPackSlice rhs, bool useUTF8,
                              VPackOptions const* options,
                              VPackSlice const* lhsBase,
                              VPackSlice const* rhsBase) {
  {
    // will resolve externals...
    int lWeight = TypeWeight(lhs);
    int rWeight = TypeWeight(rhs);

    if (lWeight < rWeight) {
      return -1;
    }

    if (lWeight > rWeight) {
      return 1;
    }

    TRI_ASSERT(lWeight == rWeight);
  }

  lhs = lhs.resolveExternal();  // follow externals
  rhs = rhs.resolveExternal();  // follow externals

  // lhs and rhs have equal weights
  if (lhs.isNone() || rhs.isNone()) {
    // either lhs or rhs is none. we cannot be sure here that both are
    // nones.
    // there can also exist the situation that lhs is a none and rhs is a
    // null value
    // (or vice versa). Anyway, the compare value is the same for both,
    return 0;
  }

  auto lhsType = lhs.type();

  switch (lhsType) {
    case VPackValueType::Illegal:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey:
    case VPackValueType::None:
    case VPackValueType::Null:
      return 0;
    case VPackValueType::Bool: {
      bool left = lhs.getBoolean();
      bool right = rhs.getBoolean();
      if (left == right) {
        return 0;
      }
      if (!left && right) {
        return -1;
      }
      return 1;
    }
    case VPackValueType::Double:
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt: {
      return compareNumberValues(lhsType, lhs, rhs);
    }
    case VPackValueType::Custom:
    case VPackValueType::String: {
      std::string lhsString;
      VPackValueLength nl;
      char const* left;
      if (lhs.isCustom()) {
        if (lhsBase == nullptr || options == nullptr ||
            options->customTypeHandler == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "Could not extract custom attribute.");
        }
        lhsString.assign(
            options->customTypeHandler->toString(lhs, options, *lhsBase));
        left = lhsString.c_str();
        nl = lhsString.size();
      } else {
        left = lhs.getString(nl);
      }
      TRI_ASSERT(left != nullptr);

      std::string rhsString;
      VPackValueLength nr;
      char const* right;
      if (rhs.isCustom()) {
        if (rhsBase == nullptr || options == nullptr ||
            options->customTypeHandler == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "Could not extract custom attribute.");
        }
        rhsString.assign(
            options->customTypeHandler->toString(rhs, options, *rhsBase));
        right = rhsString.c_str();
        nr = rhsString.size();
      } else {
        right = rhs.getString(nr);
      }
      TRI_ASSERT(right != nullptr);

      int res;
      if (useUTF8) {
        res = TRI_compare_utf8(left, static_cast<size_t>(nl), right,
                               static_cast<size_t>(nr));
      } else {
        size_t len = static_cast<size_t>(nl < nr ? nl : nr);
        res = memcmp(left, right, len);
      }
      if (res < 0) {
        return -1;
      }
      if (res > 0) {
        return 1;
      }
      // res == 0
      if (nl == nr) {
        return 0;
      }
      // res == 0, but different string lengths
      return nl < nr ? -1 : 1;
    }
    case VPackValueType::Array: {
      VPackValueLength const nl = lhs.length();
      VPackValueLength const nr = rhs.length();
      VPackValueLength const n = (std::max)(nr, nl);
      for (VPackValueLength i = 0; i < n; ++i) {
        VPackSlice lhsValue;
        if (i < nl) {
          lhsValue = lhs.at(i).resolveExternal();
        }
        VPackSlice rhsValue;
        if (i < nr) {
          rhsValue = rhs.at(i).resolveExternal();
        }

        int result = compare(lhsValue, rhsValue, useUTF8, options, &lhs, &rhs);
        if (result != 0) {
          return result;
        }
      }
      return 0;
    }
    case VPackValueType::Object: {
      std::set<std::string, AttributeSorterUTF8> keys;
      VPackCollection::keys(lhs, keys);
      VPackCollection::keys(rhs, keys);
      for (auto const& key : keys) {
        VPackSlice lhsValue = lhs.get(key).resolveExternal();
        if (lhsValue.isNone()) {
          // not present => null
          lhsValue = VPackSlice::nullSlice();
        }
        VPackSlice rhsValue = rhs.get(key).resolveExternal();
        if (rhsValue.isNone()) {
          // not present => null
          rhsValue = VPackSlice::nullSlice();
        }

        int result = compare(lhsValue, rhsValue, useUTF8, options, &lhs, &rhs);
        if (result != 0) {
          return result;
        }
      }

      return 0;
    }
    default:
      // Contains all other ValueTypes of VelocyPack.
      // They are not used in ArangoDB so this cannot occur
      TRI_ASSERT(false);
      return 0;
  }
}

VPackBuilder VelocyPackHelper::merge(VPackSlice const& lhs,
                                     VPackSlice const& rhs,
                                     bool nullMeansRemove, bool mergeObjects) {
  return VPackCollection::merge(lhs, rhs, mergeObjects, nullMeansRemove);
}

double VelocyPackHelper::toDouble(VPackSlice const& slice, bool& failed) {
  TRI_ASSERT(!slice.isNone());

  failed = false;
  switch (slice.type()) {
    case VPackValueType::None:
    case VPackValueType::Null:
      return 0.0;
    case VPackValueType::Bool:
      return (slice.getBoolean() ? 1.0 : 0.0);
    case VPackValueType::Double:
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt:
      return slice.getNumericValue<double>();
    case VPackValueType::String: {
      std::string tmp(slice.copyString());
      try {
        // try converting string to number
        return std::stod(tmp);
      } catch (...) {
        if (tmp.empty()) {
          return 0.0;
        }
        // conversion failed
      }
      break;
    }
    case VPackValueType::Array: {
      VPackValueLength const n = slice.length();

      if (n == 0) {
        return 0.0;
      } else if (n == 1) {
        return VelocyPackHelper::toDouble(slice.at(0).resolveExternal(),
                                          failed);
      }
      break;
    }
    case VPackValueType::External: {
      return VelocyPackHelper::toDouble(slice.resolveExternal(), failed);
    }
    case VPackValueType::Illegal:
    case VPackValueType::Object:
    case VPackValueType::UTCDate:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey:
    case VPackValueType::Binary:
    case VPackValueType::BCD:
    case VPackValueType::Custom:
      break;
  }

  failed = true;
  return 0.0;
}

#ifndef USE_ENTERPRISE
uint64_t VelocyPackHelper::hashByAttributes(
    VPackSlice slice, std::vector<std::string> const& attributes,
    bool docComplete, int& error, std::string const& key) {
  uint64_t hash = TRI_FnvHashBlockInitial();
  error = TRI_ERROR_NO_ERROR;
  slice = slice.resolveExternal();
  if (slice.isObject()) {
    for (auto const& attr : attributes) {
      VPackSlice sub = slice.get(attr).resolveExternal();
      VPackBuilder temporaryBuilder;
      if (sub.isNone()) {
        if (attr == StaticStrings::KeyString && !key.empty()) {
          temporaryBuilder.add(VPackValue(key));
          sub = temporaryBuilder.slice();
        } else {
          if (!docComplete) {
            error = TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN;
          }
          // Null is equal to None/not present
          sub = VPackSlice::nullSlice();
        }
      }
      hash = sub.normalizedHash(hash);
    }
  }
  return hash;
}
#endif

void VelocyPackHelper::SanitizeExternals(VPackSlice const input,
                                         VPackBuilder& output) {
  if (input.isExternal()) {
    output.add(input.resolveExternal());
  } else if (input.isObject()) {
    output.openObject();
    for (auto const& it : VPackObjectIterator(input)) {
      output.add(VPackValue(it.key.copyString()));
      SanitizeExternals(it.value, output);
    }
    output.close();
  } else if (input.isArray()) {
    output.openArray();
    for (auto const& it : VPackArrayIterator(input)) {
      SanitizeExternals(it, output);
    }
    output.close();
  } else {
    output.add(input);
  }
}

bool VelocyPackHelper::hasExternals(VPackSlice input) {
  if (input.isExternal()) {
    return true;
  } else if (input.isObject()) {
    for (auto const& it : VPackObjectIterator(input)) {
      if (hasExternals(it.value) == true) {
        return true;
      };
    }
  } else if (input.isArray()) {
    for (auto const& it : VPackArrayIterator(input)) {
      if (hasExternals(it) == true) {
        return true;
      }
    }
  }
  return false;
}

VPackBuffer<uint8_t> VelocyPackHelper::sanitizeExternalsChecked(
    VPackSlice input, VPackOptions const* options, bool checkExternals) {
  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer, options);
  bool resolveExt = true;
  if (checkExternals) {
    resolveExt = hasExternals(input);
  }
  if (resolveExt) {  // resolve
    SanitizeExternals(input, builder);
  } else {
    builder.add(input);
  }
  return buffer;  // elided
}

/// @brief extract the collection id from VelocyPack
uint64_t VelocyPackHelper::extractIdValue(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return 0;
  }
  VPackSlice id = slice.get("id");
  if (id.isNone()) {
    // pre-3.1 compatibility
    id = slice.get("cid");
  }

  if (id.isString()) {
    // string cid, e.g. "9988488"
    return StringUtils::uint64(id.copyString());
  } else if (id.isNumber()) {
    // numeric cid, e.g. 9988488
    return id.getNumericValue<uint64_t>();
  } else if (!id.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid value for 'id' attribute");
  }

  TRI_ASSERT(id.isNone()); 
  return 0;
}

arangodb::LoggerStream& operator<<(arangodb::LoggerStream& logger,
                                   VPackSlice const& slice) {
  size_t const cutoff = 100;
  std::string sliceStr(slice.toJson());
  bool longer = sliceStr.size() > cutoff;
  if (longer) {
    logger << sliceStr.substr(cutoff) << "...";
  } else {
    logger << sliceStr;
  }
  return logger;
}
