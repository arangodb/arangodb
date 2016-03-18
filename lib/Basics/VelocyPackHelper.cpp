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
#include "Basics/conversions.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Basics/files.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VPackStringBufferAdapter.h"

#include <velocypack/AttributeTranslator.h>
#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

static std::unique_ptr<VPackAttributeTranslator> Translator;
static std::unique_ptr<VPackAttributeExcludeHandler> ExcludeHandler;

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for NULL which can be shared by all
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

static VPackBuilder NullBuilder; 

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for TRUE which can be shared by all
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

static VPackBuilder TrueBuilder;

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for FALSE which can be shared by all
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

static VPackBuilder FalseBuilder;

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for empty ARRAYs which can be shared by all
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

static VPackBuilder EmptyArrayBuilder;


// attribute exclude handler for skipping over system attributes
struct SystemAttributeExcludeHandler : public VPackAttributeExcludeHandler {
  bool shouldExclude(VPackSlice const& key, int nesting) override final {
    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);

    if (p == nullptr || *p != '_' || keyLength < 3 || keyLength > 5 || nesting > 0) {
      // keep attribute
      return true;
    }

    // exclude these attributes (but not _key!)
    if ((keyLength == 3 && memcmp(p, "_id", keyLength) == 0) ||
        (keyLength == 4 && memcmp(p, "_rev", keyLength) == 0) ||
        (keyLength == 3 && memcmp(p, "_to", keyLength) == 0) ||
        (keyLength == 5 && memcmp(p, "_from", keyLength) == 0)) {
      return true;
    }

    // keep attribute
    return false;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief static initializer for all VPack values
////////////////////////////////////////////////////////////////////////////////
  
void VelocyPackHelper::initialize() {
  LOG(TRACE) << "initializing vpack";

  // initialize attribute translator
  Translator.reset(new VPackAttributeTranslator);

  // these attribute names will be translated into short integer values
  Translator->add("_key", 1);
  Translator->add("_rev", 2);
  Translator->add("_id", 3);
  Translator->add("_from", 4);
  Translator->add("_to", 5);

  Translator->seal();

  // set the attribute translator in the global options
  VPackOptions::Defaults.attributeTranslator = Translator.get();
  VPackSlice::attributeTranslator = Translator.get();
  
  // initialize exclude handler for system attributes
  ExcludeHandler.reset(new SystemAttributeExcludeHandler);

  // Null value
  NullBuilder.add(VPackValue(VPackValueType::Null));
  // True value
  TrueBuilder.add(VPackValue(true));
  // False value
  FalseBuilder.add(VPackValue(false));
  // Array value (empty)
  EmptyArrayBuilder.openArray();
  EmptyArrayBuilder.close();
}
  
void VelocyPackHelper::disableAssemblerFunctions() {
  arangodb::velocypack::disableAssemblerFunctions();
}

arangodb::velocypack::Slice VelocyPackHelper::NullValue() {
  return NullBuilder.slice(); 
}

arangodb::velocypack::Slice VelocyPackHelper::TrueValue() {
  return TrueBuilder.slice(); 
}

arangodb::velocypack::Slice VelocyPackHelper::FalseValue() {
  return FalseBuilder.slice(); 
}

arangodb::velocypack::Slice VelocyPackHelper::BooleanValue(bool value) {
  if (value) {
    return TrueBuilder.slice();
  }
  return FalseBuilder.slice(); 
}

arangodb::velocypack::Slice VelocyPackHelper::EmptyArrayValue() {
  return EmptyArrayBuilder.slice(); 
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the (global) attribute exclude handler instance
////////////////////////////////////////////////////////////////////////////////

arangodb::velocypack::AttributeExcludeHandler* VelocyPackHelper::getExcludeHandler() {
  return ExcludeHandler.get();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the (global) attribute translator instance
////////////////////////////////////////////////////////////////////////////////

arangodb::velocypack::AttributeTranslator* VelocyPackHelper::getTranslator() {
  return Translator.get();
}


bool VelocyPackHelper::AttributeSorter::operator()(std::string const& l,
                                                   std::string const& r) const {
  return TRI_compare_utf8(l.c_str(), l.size(), r.c_str(), r.size()) < 0;
}

size_t VelocyPackHelper::VPackHash::operator()(VPackSlice const& slice) const {
  return slice.hash();
};

bool VelocyPackHelper::VPackEqual::operator()(VPackSlice const& lhs, VPackSlice const& rhs) const {
  return VelocyPackHelper::compare(lhs, rhs, false) == 0;
};

static int TypeWeight(VPackSlice const& slice) {
  switch (slice.type()) {
    case VPackValueType::Bool:
      return 1;
    case VPackValueType::Double:
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt:
      return 2;
    case VPackValueType::String:
    case VPackValueType::Custom:
      // custom type is used for id
      return 3;
    case VPackValueType::Array:
      return 4;
    case VPackValueType::Object:
      return 5;
    default:
      // All other values have equal weight
      return 0;
  }
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

std::string VelocyPackHelper::getStringValue(VPackSlice const& slice,
                                             char const* name,
                                             std::string const& defaultValue) {
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

TRI_json_t* VelocyPackHelper::velocyPackToJson(VPackSlice const& slice, VPackOptions const* options) {
  return JsonHelper::fromString(slice.toJson(options));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json file to VelocyPack
//////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> VelocyPackHelper::velocyPackFromFile(
    std::string const& path) {
  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, path.c_str(), &length);
  if (content != nullptr) {
    // The Parser might THROW
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

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  arangodb::basics::VPackStringBufferAdapter bufferAdapter(&buffer);
  try {
    VPackDumper dumper(&bufferAdapter);
    dumper.dump(slice);
  } catch (...) {
    // Writing failed
    TRI_AnnihilateStringBuffer(&buffer);
    return false;
  }

  if (TRI_LengthStringBuffer(&buffer) == 0) {
    // should not happen
    return false;
  }

  if (appendNewline) {
    // add the newline here so we only need one write operation in the ideal
    // case
    TRI_AppendCharStringBuffer(&buffer, '\n');
  }

  char const* p = TRI_BeginStringBuffer(&buffer);
  size_t n = TRI_LengthStringBuffer(&buffer);

  while (0 < n) {
    ssize_t m = TRI_WRITE(fd, p, (TRI_write_t)n);

    if (m <= 0) {
      TRI_AnnihilateStringBuffer(&buffer);
      return false;
    }

    n -= m;
    p += m;
  }

  TRI_AnnihilateStringBuffer(&buffer);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief writes a VelocyPack to a file
//////////////////////////////////////////////////////////////////////////////

bool VelocyPackHelper::velocyPackToFile(char const* filename,
                                        VPackSlice const& slice,
                                        bool syncFile) {
  std::string const tmp = std::string(filename) + ".tmp";

  // remove a potentially existing temporary file
  if (TRI_ExistsFile(tmp.c_str())) {
    TRI_UnlinkFile(tmp.c_str());
  }

  int fd = TRI_CREATE(tmp.c_str(), O_CREAT | O_TRUNC | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot create json file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    return false;
  }

  if (!PrintVelocyPack(fd, slice, true)) {
    TRI_CLOSE(fd);
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot write to json file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());
    return false;
  }

  if (syncFile) {
    LOG(TRACE) << "syncing tmp file '" << tmp << "'";

    if (!TRI_fsync(fd)) {
      TRI_CLOSE(fd);
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG(ERR) << "cannot sync saved json '" << tmp << "': " << TRI_LAST_ERROR_STR;
      TRI_UnlinkFile(tmp.c_str());
      return false;
    }
  }

  int res = TRI_CLOSE(fd);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "cannot close saved file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());
    return false;
  }

  res = TRI_RenameFile(tmp.c_str(), filename);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG(ERR) << "cannot rename saved file '" << tmp << "' to '" << filename << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());

    return false;
  }

  return true;
}

int VelocyPackHelper::compare(VPackSlice const& lhs, VPackSlice const& rhs,
                              bool useUTF8, VPackOptions const* options,
                              VPackSlice const* lhsBase, VPackSlice const* rhsBase) {
  {
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

  // lhs and rhs have equal weights
  if (lhs.isNone() || rhs.isNone()) {
    // either lhs or rhs is none. we cannot be sure here that both are
    // nones.
    // there can also exist the situation that lhs is a none and rhs is a
    // null value
    // (or vice versa). Anyway, the compare value is the same for both,
    return 0;
  }

  switch (lhs.type()) {
    case VPackValueType::None:
    case VPackValueType::Null:
      return 0;  // null == null;
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
      double left = lhs.getNumericValue<double>();
      double right = rhs.getNumericValue<double>();
      if (left == right) {
        return 0;
      }
      if (left < right) {
        return -1;
      }
      return 1;
    }
    case VPackValueType::Custom: 
    case VPackValueType::String: {
      std::string lhsString;
      VPackValueLength nl;
      char const* left;
      if (lhs.isCustom()) {
        if (lhsBase == nullptr || options == nullptr || options->customTypeHandler == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "Could not extract custom attribute.");
        }
        lhsString.assign(options->customTypeHandler->toString(lhs, options, *lhsBase));
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
        if (rhsBase == nullptr || options == nullptr || options->customTypeHandler == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "Could not extract custom attribute.");
        }
        rhsString.assign(options->customTypeHandler->toString(rhs, options, *rhsBase));
        right = rhsString.c_str();
        nr = rhsString.size();
      } else {
        right = rhs.getString(nr);
      }
      TRI_ASSERT(right != nullptr);

      int res;
      if (useUTF8) {
        res = TRI_compare_utf8(left, nl, right, nr);
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
          lhsValue = lhs.at(i);
        }
        VPackSlice rhsValue;
        if (i < nr) {
          rhsValue = rhs.at(i);
        }

        int result = compare(lhsValue, rhsValue, useUTF8, options, &lhs, &rhs);
        if (result != 0) {
          return result;
        }
      }
      return 0;
    }
    case VPackValueType::Object: {
      std::set<std::string, AttributeSorter> keys;
      VPackCollection::keys(lhs, keys);
      VPackCollection::keys(rhs, keys);
      for (auto const& key : keys) {
        VPackSlice lhsValue;
        if (lhs.hasKey(key)) {
          lhsValue = lhs.get(key);
        }
        VPackSlice rhsValue;
        if (rhs.hasKey(key)) {
          rhsValue = rhs.get(key);
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
        return VelocyPackHelper::toDouble(slice.at(0), failed);
      }
      break;
    }
    case VPackValueType::Object:
    case VPackValueType::UTCDate:
    case VPackValueType::External:
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

arangodb::LoggerStream& operator<< (arangodb::LoggerStream& logger,
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
