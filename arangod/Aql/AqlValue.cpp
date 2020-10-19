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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "AqlValue.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Arithmetic.h"
#include "Aql/Range.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/Endian.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "V8/v8-vpack.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>
#include <type_traits>

#ifndef velocypack_malloc
#error velocypack_malloc must be defined
#endif

using namespace arangodb;
using namespace arangodb::aql;

// some functionality borrowed from 3rdParty/velocypack/include/velocypack
// this is a copy of that functionality, because the functions in velocypack
// are not accessible from here
namespace {
static inline uint64_t toUInt64(int64_t v) noexcept {
  // If v is negative, we need to add 2^63 to make it positive,
  // before we can cast it to an uint64_t:
  uint64_t shift2 = 1ULL << 63;
  int64_t shift = static_cast<int64_t>(shift2 - 1);
  return v >= 0 ? static_cast<uint64_t>(v) : static_cast<uint64_t>((v + shift) + 1) + shift2;
  // Note that g++ and clang++ with -O3 compile this away to
  // nothing. Further note that a plain cast from int64_t to
  // uint64_t is not guaranteed to work for negative values!
}

// returns number of bytes required to store the value in 2s-complement
static inline uint8_t intLength(int64_t value) noexcept {
  if (value >= -0x80 && value <= 0x7f) {
    // shortcut for the common case
    return 1;
  }
  uint64_t x = value >= 0 ? static_cast<uint64_t>(value)
                          : static_cast<uint64_t>(-(value + 1));
  uint8_t xSize = 0;
  do {
    xSize++;
    x >>= 8;
  } while (x >= 0x80);
  return xSize + 1;
}
}  // namespace

/// @brief hashes the value, normalizes the values
uint64_t AqlValue::hash(uint64_t seed) const {
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      // we must use the slow hash function here, because a value may have
      // different representations in case it's an array/object/number
      return slice(t).normalizedHash(seed);
    }
    case DOCVEC: {
      uint64_t const tmp = docvecLength() ^ 0xba5bedf00d;
      uint64_t value = VELOCYPACK_HASH(&tmp, sizeof(tmp), seed);
      for (auto const& it : *_data.docvec) {
        size_t const n = it->numRows();
        for (size_t i = 0; i < n; ++i) {
          value = it->getValueReference(i, 0).hash(value);
        }
      }
      return value;
    }
    case RANGE: {
      uint64_t const n = _data.range->size();
      
      // simon: copied from VPackSlice::normalizedHash()
      // normalize arrays by hashing array length and iterating
      // over all array members
      uint64_t const tmp = n ^ 0xba5bedf00d;
      uint64_t value = VELOCYPACK_HASH(&tmp, sizeof(tmp), seed);

      for (uint64_t i = 0; i < n; ++i) {
        // upcast integer values to double
        double v = static_cast<double>(_data.range->at(i));
        value ^= VELOCYPACK_HASH(&v, sizeof(v), value);
      }
      
      return value;
    }
  }

  return 0;
}

/// @brief whether or not the value contains a none value
bool AqlValue::isNone() const noexcept {
  switch (type()) {
    case VPACK_INLINE: {
      return VPackSlice(&_data.internal[0]).resolveExternal().isNone();
    }
    case VPACK_SLICE_POINTER: {
      // not resolving externals here
      return VPackSlice(_data.pointer).isNone();
    }
    case VPACK_MANAGED_SLICE: {
      return VPackSlice(_data.slice).resolveExternal().isNone();
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }

  return false;
}

/// @brief whether or not the value is a null value
bool AqlValue::isNull(bool emptyIsNull) const noexcept {
  switch (type()) {
    case VPACK_INLINE: {
      VPackSlice s(&_data.internal[0]);
      s = s.resolveExternal();
      return (s.isNull() || (emptyIsNull && s.isNone()));
    }
    case VPACK_SLICE_POINTER: {
      // not resolving externals here
      VPackSlice s(_data.pointer);
      return (s.isNull() || (emptyIsNull && s.isNone()));
    }
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(_data.slice);
      s = s.resolveExternal();
      return (s.isNull() || (emptyIsNull && s.isNone()));
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }

  return false;
}

/// @brief whether or not the value is a boolean value
bool AqlValue::isBoolean() const noexcept {
  switch (type()) {
    case VPACK_INLINE: {
      return VPackSlice(&_data.internal[0]).resolveExternal().isBoolean();
    }
    case VPACK_SLICE_POINTER: {
      // not resolving externals here
      return VPackSlice(_data.pointer).isBoolean();
    }
    case VPACK_MANAGED_SLICE: {
      return VPackSlice(_data.slice).resolveExternal().isBoolean();
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }

  return false;
}

/// @brief whether or not the value is a number
bool AqlValue::isNumber() const noexcept {
  switch (type()) {
    case VPACK_INLINE: {
      return VPackSlice(&_data.internal[0]).resolveExternal().isNumber();
    }
    case VPACK_SLICE_POINTER: {
      // not resolving externals here
      return VPackSlice(_data.pointer).isNumber();
    }
    case VPACK_MANAGED_SLICE: {
      return VPackSlice(_data.slice).resolveExternal().isNumber();
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }

  return false;
}

/// @brief whether or not the value is a string
bool AqlValue::isString() const noexcept {
  switch (type()) {
    case VPACK_INLINE: {
      return VPackSlice(&_data.internal[0]).resolveExternal().isString();
    }
    case VPACK_SLICE_POINTER: {
      // not resolving externals here
      return VPackSlice(_data.pointer).isString();
    }
    case VPACK_MANAGED_SLICE: {
      return VPackSlice(_data.slice).resolveExternal().isString();
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }
  
  return false;
}

/// @brief whether or not the value is an object
bool AqlValue::isObject() const noexcept {
  switch (type()) {
    case VPACK_INLINE: {
      return VPackSlice(&_data.internal[0]).resolveExternal().isObject();
    }
    case VPACK_SLICE_POINTER: {
      // not resolving externals here
      return VPackSlice(_data.pointer).isObject();
    }
    case VPACK_MANAGED_SLICE: {
      return VPackSlice(_data.slice).resolveExternal().isObject();
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }
  
  return false;
}

/// @brief whether or not the value is an array (note: this treats ranges
/// as arrays, too!)
bool AqlValue::isArray() const noexcept {
  switch (type()) {
    case VPACK_INLINE: {
      return VPackSlice(&_data.internal[0]).resolveExternal().isArray();
    }
    case VPACK_SLICE_POINTER: {
      // not resolving externals here
      return VPackSlice(_data.pointer).isArray();
    }
    case VPACK_MANAGED_SLICE: {
      return VPackSlice(_data.slice).resolveExternal().isArray();
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }
  
  return true;
}

char const* AqlValue::getTypeString() const noexcept {
  if (isNone()) {
    return "none";
  } else if (isNull(true)) {
    return "null";
  } else if (isBoolean()) {
    return "bool";
  } else if (isNumber()) {
    return "number";
  } else if (isString()) {
    return "string";
  } else if (isObject()) {
    return "object";
  } else if (isArray()) {
    return "array";
  }
  return "none";
}

/// @brief get the (array) length (note: this treats ranges as arrays, too!)
size_t AqlValue::length() const {
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      return static_cast<size_t>(slice(t).length());
    }
    case DOCVEC: {
      return docvecLength();
    }
    case RANGE: {
      return range()->size();
    }
  }
  TRI_ASSERT(false);
  return 0;
}

/// @brief get the (array) element at position
AqlValue AqlValue::at(int64_t position, bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isArray()) {
        int64_t const n = static_cast<int64_t>(s.length());
        if (position < 0) {
          // a negative position is allowed
          position = n + position;
        }
        if (position >= 0 && position < n) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(s.at(position));
          }
          // return a reference to an existing slice
          return AqlValue(s.at(position).begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC: {
      size_t const n = docvecLength();
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }
      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        size_t total = 0;
        for (auto const& it : *_data.docvec) {
          if (position < static_cast<int64_t>(total + it->numRows())) {
            // found the correct vector
            if (doCopy) {
              mustDestroy = true;
              return it
                  ->getValueReference(static_cast<size_t>(position - total), 0)
                  .clone();
            }
            return it->getValue(static_cast<size_t>(position - total), 0);
          }
          total += it->numRows();
        }
      }
      // intentionally falls through
      break;
    }
    case RANGE: {
      size_t const n = range()->size();
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }

      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        return AqlValue(AqlValueHintInt(_data.range->at(static_cast<size_t>(position))));
      }
      // intentionally falls through
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the (array) element at position
AqlValue AqlValue::at(int64_t position, size_t n, bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isArray()) {
        if (position < 0) {
          // a negative position is allowed
          position = static_cast<int64_t>(n) + position;
        }
        if (position >= 0 && position < static_cast<int64_t>(n)) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(s.at(position));
          }
          // return a reference to an existing slice
          return AqlValue(s.at(position).begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC: {
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }
      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        size_t total = 0;
        for (auto const& it : *_data.docvec) {
          if (position < static_cast<int64_t>(total + it->numRows())) {
            // found the correct vector
            if (doCopy) {
              mustDestroy = true;
              return it
                  ->getValueReference(static_cast<size_t>(position - total), 0)
                  .clone();
            }
            return it->getValue(static_cast<size_t>(position - total), 0);
          }
          total += it->numRows();
        }
      }
      // intentionally falls through
      break;
    }
    case RANGE: {
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }

      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        return AqlValue(AqlValueHintInt(_data.range->at(static_cast<size_t>(position))));
      }
      // intentionally falls through
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the _key attribute from an object/document
AqlValue AqlValue::getKeyAttribute(bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isObject()) {
        VPackSlice found = transaction::helpers::extractKeyFromDocument(s);
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(found);
          }
          // return a reference to an existing slice
          return AqlValue(found.begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the _id attribute from an object/document
AqlValue AqlValue::getIdAttribute(CollectionNameResolver const& resolver,
                                  bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isObject()) {
        VPackSlice found = transaction::helpers::extractIdFromDocument(s);
        if (found.isCustom()) {
          // _id as a custom type needs special treatment
          mustDestroy = true;
          return AqlValue(transaction::helpers::extractIdString(&resolver, found, s));
        }
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(found);
          }
          // return a reference to an existing slice
          return AqlValue(found.begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the _from attribute from an object/document
AqlValue AqlValue::getFromAttribute(bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isObject()) {
        VPackSlice found = transaction::helpers::extractFromFromDocument(s);
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(found);
          }
          // return a reference to an existing slice
          return AqlValue(found.begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the _to attribute from an object/document
AqlValue AqlValue::getToAttribute(bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isObject()) {
        VPackSlice found = transaction::helpers::extractToFromDocument(s);
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(found);
          }
          // return a reference to an existing slice
          return AqlValue(found.begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the (object) element by name
AqlValue AqlValue::get(CollectionNameResolver const& resolver,
                       std::string const& name, bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isObject()) {
        VPackSlice found(s.get(name));
        if (found.isCustom()) {
          // _id needs special treatment
          mustDestroy = true;
          return AqlValue(transaction::helpers::extractIdString(&resolver, s, VPackSlice()));
        }
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(found);
          }
          // return a reference to an existing slice
          return AqlValue(found.begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the (object) element by name
AqlValue AqlValue::get(CollectionNameResolver const& resolver,
                       arangodb::velocypack::StringRef const& name,
                       bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isObject()) {
        VPackSlice found(s.get(name));
        if (found.isCustom()) {
          // _id needs special treatment
          mustDestroy = true;
          return AqlValue(transaction::helpers::extractIdString(&resolver, s, VPackSlice()));
        }
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(found);
          }
          // return a reference to an existing slice
          return AqlValue(found.begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief get the (object) element(s) by name
AqlValue AqlValue::get(CollectionNameResolver const& resolver,
                       std::vector<std::string> const& names, bool& mustDestroy,
                       bool doCopy) const {
  mustDestroy = false;
  if (names.empty()) {
    return AqlValue(AqlValueHintNull());
  }

  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    [[fallthrough]];
    case VPACK_INLINE:
    [[fallthrough]];
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isObject()) {
        s = s.resolveExternal();
        VPackSlice prev;
        size_t const n = names.size();
        for (size_t i = 0; i < n; ++i) {
          // fetch subattribute
          prev = s;
          s = s.get(names[i]);
          s = s.resolveExternal();

          if (s.isNone()) {
            // not found
            return AqlValue(AqlValueHintNull());
          } else if (s.isCustom()) {
            // _id needs special treatment
            if (i + 1 == n) {
              // x.y._id
              mustDestroy = true;
              return AqlValue(transaction::helpers::extractIdString(&resolver, s, prev));
            }
            // x._id.y
            return AqlValue(AqlValueHintNull());
          } else if (i + 1 < n && !s.isObject()) {
            return AqlValue(AqlValueHintNull());
          }
        }

        if (!s.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(s);
          }
          // return a reference to an existing slice
          return AqlValue(s.begin());
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(AqlValueHintNull());
}

/// @brief check whether an object has a specific key
bool AqlValue::hasKey(std::string const& name) const {
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      return (s.isObject() && s.hasKey(name));
    }
    case DOCVEC:
    case RANGE: {
      break;
    }
  }

  // default is to return false
  return false;
}

/// @brief get the numeric value of an AqlValue
double AqlValue::toDouble() const {
  bool failed;  // will be ignored
  return toDouble(failed);
}

double AqlValue::toDouble(bool& failed) const {
  failed = false;
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isNull()) {
        return 0.0;
      }
      if (s.isNumber()) {
        return s.getNumber<double>();
      }
      if (s.isBoolean()) {
        return s.getBoolean() ? 1.0 : 0.0;
      }
      if (s.isString()) {
        return arangodb::aql::stringToNumber(s.copyString(), failed);
      }
      if (s.isArray()) {
        auto length = s.length();
        if (length == 0) {
          return 0.0;
        }
        if (length == 1) {
          bool mustDestroy;  // we can ignore destruction here
          return at(0, mustDestroy, false).toDouble(failed);
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      if (length() == 1) {
        bool mustDestroy;  // we can ignore destruction here
        return at(0, mustDestroy, false).toDouble(failed);
      }
      // will return 0
      return 0.0;
    }
  }

  failed = true;
  return 0.0;
}

/// @brief get the numeric value of an AqlValue
int64_t AqlValue::toInt64() const {
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isNumber()) {
        return s.getNumber<int64_t>();
      }
      if (s.isBoolean()) {
        return s.getBoolean() ? 1 : 0;
      }
      if (s.isString()) {
        std::string v(s.copyString());
        try {
          return static_cast<int64_t>(std::stoll(v));
        } catch (...) {
          if (v.empty()) {
            return 0;
          }
          // conversion failed
        }
      } else if (s.isArray()) {
        auto length = s.length();
        if (length == 0) {
          return 0;
        }
        if (length == 1) {
          // we can ignore destruction here
          bool mustDestroy;
          return at(0, mustDestroy, false).toInt64();
        }
      }
      // intentionally falls through
      break;
    }
    case DOCVEC:
    case RANGE: {
      if (length() == 1) {
        bool mustDestroy;
        return at(0, mustDestroy, false).toInt64();
      }
      // will return 0
      break;
    }
  }

  return 0;
}

/// @brief whether or not the contained value evaluates to true
bool AqlValue::toBoolean() const {
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(slice(t));
      if (s.isBoolean()) {
        return s.getBoolean();
      }
      if (s.isNumber()) {
        return (s.getNumber<double>() != 0.0);
      }
      if (s.isString()) {
        return (s.getStringLength() > 0);
      }
      if (s.isArray() || s.isObject() || s.isCustom()) {
        // custom _id type is also true
        return true;
      }
      // all other cases, including Null and None
      return false;
    }
    case DOCVEC:
    case RANGE: {
      return true;
    }
  }
  return false;
}

/// @brief return the total size of the docvecs
size_t AqlValue::docvecLength() const {
  TRI_ASSERT(type() == DOCVEC);
  TRI_ASSERT(_data.docvec != nullptr);
  size_t s = 0;
  for (auto const& it : *_data.docvec) {
    TRI_ASSERT(it != nullptr);
    s += it->numRows();
  }
  return s;
}

/// @brief return the memory origin type for values of type VPACK_MANAGED_SLICE
AqlValue::MemoryOriginType AqlValue::memoryOriginType() const noexcept {
  TRI_ASSERT(type() == VPACK_MANAGED_SLICE);
  MemoryOriginType mot = static_cast<MemoryOriginType>(_data.internal[sizeof(_data.internal) - 2]);
  TRI_ASSERT(mot == MemoryOriginType::New || mot == MemoryOriginType::Malloc);
  return mot;
}
  
/// @brief store meta information for values of type VPACK_MANAGED_SLICE
void AqlValue::setManagedSliceData(MemoryOriginType mot, arangodb::velocypack::ValueLength length) {
  TRI_ASSERT(length > 0);
  TRI_ASSERT(mot == MemoryOriginType::New || mot == MemoryOriginType::Malloc);
  if (ADB_UNLIKELY(length > 0x0000ffffffffffffULL)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_OUT_OF_MEMORY, "invalid AqlValue length");
  }
  // assemble a 64 bit value with meta information for this AqlValue:
  // the first 6 bytes contain the byteSize
  // the next byte contains the memoryOriginType (0 = new[], 1 = malloc)
  // the last byte contains the AqlValueType (always VPACK_MANAGED_SLICE)
  _data.words[1] = basics::hostToBig<uint64_t>(
      ((length & 0x0000ffffffffffffULL) << 16U) | 
      (static_cast<uint8_t>(mot) << 8) |
      static_cast<uint8_t>(AqlValueType::VPACK_MANAGED_SLICE)
  );
  TRI_ASSERT(type() == VPACK_MANAGED_SLICE);
  TRI_ASSERT(memoryOriginType() == mot);
  TRI_ASSERT(memoryUsage() == length);
}
  
/// @brief construct a V8 value as input for the expression execution in V8
v8::Handle<v8::Value> AqlValue::toV8(v8::Isolate* isolate, velocypack::Options const* options) const {
  auto context = TRI_IGETC;
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      return TRI_VPackToV8(isolate, slice(t), options);
    }
    case DOCVEC: {
      // calculate the result array length
      size_t const s = docvecLength();
      // allocate the result array
      v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(s));
      uint32_t j = 0;  // output row count
      for (auto const& it : *_data.docvec) {
        size_t const n = it->numRows();
        for (size_t i = 0; i < n; ++i) {
          result->Set(context, j++, it->getValueReference(i, 0).toV8(isolate, options)).FromMaybe(false);

          if (V8PlatformFeature::isOutOfMemory(isolate)) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
      }
      return result;
    }
    case RANGE: {
      size_t const n = _data.range->size();
      Range::throwIfTooBigForMaterialization(n);
      v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));

      for (uint32_t i = 0; i < n; ++i) {
        // is it safe to use a double here (precision loss)?
        result->Set(context,
                    i,
                    v8::Number::New(isolate,
                                    static_cast<double>(_data.range->at(static_cast<size_t>(i)))
                                    )
                    ).FromMaybe(true);

        if (i % 1000 == 0) {
          if (V8PlatformFeature::isOutOfMemory(isolate)) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
      }
      return result;
    }
  }

  // we shouldn't get here
  return v8::Null(isolate);
}

/// @brief materializes a value into the builder
void AqlValue::toVelocyPack(VPackOptions const* options, VPackBuilder& builder,
                            bool resolveExternals, bool allowUnindexed) const {
  AqlValueType t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      if (!resolveExternals && isManagedDocument()) {
        builder.addExternal(_data.pointer);
        break;
      }  
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE: {
      if (resolveExternals) {
        bool const sanitizeExternals = true;
        bool const sanitizeCustom = true;
        arangodb::basics::VelocyPackHelper::sanitizeNonClientTypes(
            slice(t), VPackSlice::noneSlice(), builder,
            options, sanitizeExternals,
            sanitizeCustom, allowUnindexed);
      } else {
        builder.add(slice(t));
      }
      break;
    }
    case DOCVEC: {
      builder.openArray(/*unindexed*/allowUnindexed);
      for (auto const& it : *_data.docvec) {
        size_t const n = it->numRows();
        for (size_t i = 0; i < n; ++i) {
          it->getValueReference(i, 0).toVelocyPack(options, builder,
                                                   resolveExternals, allowUnindexed);
        }
      }
      builder.close();
      break;
    }
    case RANGE: {
      builder.openArray(/*unindexed*/allowUnindexed);
      size_t const n = _data.range->size();
      Range::throwIfTooBigForMaterialization(n);
      for (size_t i = 0; i < n; ++i) {
        builder.add(VPackValue(_data.range->at(i)));
      }
      builder.close();
      break;
    }
  }
}

/// @brief materializes a value into the builder
AqlValue AqlValue::materialize(VPackOptions const* options, bool& hasCopied,
                               bool resolveExternals) const {
  switch (type()) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      hasCopied = false;
      return *this;
    }
    case DOCVEC:
    case RANGE: {
      VPackBuffer<uint8_t> buffer;
      VPackBuilder builder(buffer);
      toVelocyPack(options, builder, resolveExternals, /*allowUnindexed*/true);
      hasCopied = true;
      return AqlValue(std::move(buffer));
    }
  }

  // we shouldn't get here
  hasCopied = false;
  return AqlValue();
}

/// @brief clone a value
AqlValue AqlValue::clone() const {
  AqlValueType t = type();
  switch (t) {
    case VPACK_INLINE: {
      // copy internal data
      VPackSlice s(&_data.internal[0]);
      if (!s.isExternal()) {
        return AqlValue(*this);
      }
      return AqlValue(s.resolveExternal());
    }
    case VPACK_SLICE_POINTER: {
      if (isManagedDocument()) {
        // copy from externally managed document. this will not copy the data
        return AqlValue(AqlValueHintDocumentNoCopy(_data.pointer));
      }
      // copy from regular pointer. this may copy the data
      return AqlValue(_data.pointer);
    }
    case VPACK_MANAGED_SLICE: {
      // byte size is stored in the first 6 bytes of the second uint64_t value
      VPackValueLength length = static_cast<VPackValueLength>((basics::bigToHost<uint64_t>(_data.words[1]) & 0xffffffffffff0000ULL) >> 16);
      return AqlValue(VPackSlice(_data.slice), length);
    }
    case DOCVEC: {
      auto c = std::make_unique<std::vector<SharedAqlItemBlockPtr>>();
      c->reserve(docvecLength());
      for (auto const& it : *_data.docvec) {
        c->emplace_back(it->slice(0, it->numRows()));
        TRI_ASSERT(c->back() != nullptr);
      }
      return AqlValue(c.release());
    }
    case RANGE: {
      // create a new value with a new range
      return AqlValue(range()->_low, range()->_high);
    }
  }

  TRI_ASSERT(false);
  return {};
}

/// @brief destroy the value's internals
void AqlValue::destroy() noexcept {
  switch (type()) {
    case VPACK_INLINE: 
    case VPACK_SLICE_POINTER: {
      // nothing to do
      return;
    }
    case VPACK_MANAGED_SLICE: {
      MemoryOriginType const memoryType = memoryOriginType();
      if (memoryType == MemoryOriginType::New) {
        delete[] _data.slice;
      } else {
        TRI_ASSERT(memoryType == MemoryOriginType::Malloc);
        free(_data.slice);
      }
      break;
    }
    case DOCVEC: {
      // Will delete all ItemBlocks
      delete _data.docvec;
      break;
    }
    case RANGE: {
      delete _data.range;
      break;
    }
  }

  erase();  // to prevent duplicate deletion
}

/// @brief return the slice from the value
VPackSlice AqlValue::slice() const {
  return this->slice(type());
}

/// @brief return the slice from the value
VPackSlice AqlValue::slice(AqlValueType type) const {
  switch (type) {
    case VPACK_INLINE: {
      return VPackSlice(&_data.internal[0]).resolveExternal();
    }
    case VPACK_SLICE_POINTER: {
      return VPackSlice(_data.pointer);
    }
    case VPACK_MANAGED_SLICE: {
      return VPackSlice(_data.slice).resolveExternal();
    }
    case DOCVEC:
    case RANGE: {
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
}

/// @brief comparison for AqlValue objects
int AqlValue::Compare(velocypack::Options const* options, AqlValue const& left,
                      AqlValue const& right, bool compareUtf8) {
  AqlValue::AqlValueType const leftType = left.type();
  AqlValue::AqlValueType const rightType = right.type();

  if (leftType != rightType) {
    // TODO implement this case more efficiently
    if (leftType == RANGE || rightType == RANGE || leftType == DOCVEC || rightType == DOCVEC) {
      // range|docvec against x
      VPackBuilder leftBuilder;
      left.toVelocyPack(options, leftBuilder, /*resolveExternal*/false, /*allowUnindexed*/true);

      VPackBuilder rightBuilder;
      right.toVelocyPack(options, rightBuilder, /*resolveExternal*/false, /*allowUnindexed*/true);

      return arangodb::basics::VelocyPackHelper::compare(leftBuilder.slice(),
                                                         rightBuilder.slice(),
                                                         compareUtf8, options);
    }
    // fall-through to other types intentional
  }

  // if we get here, types are equal or can be treated as being equal

  switch (leftType) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE: {
      return arangodb::basics::VelocyPackHelper::compare(left.slice(leftType), right.slice(rightType),
                                                         compareUtf8, options);
    }
    case DOCVEC: {
      // use lexicographic ordering of AqlValues regardless of block,
      // DOCVECs have a single register coming from ReturnNode.
      size_t lblock = 0;
      size_t litem = 0;
      size_t rblock = 0;
      size_t ritem = 0;
      size_t const lsize = left._data.docvec->size();
      size_t const rsize = right._data.docvec->size();

      if (lsize == 0 || rsize == 0) {
        if (lsize == rsize) {
          // both empty
          return 0;
        }
        return (lsize < rsize ? -1 : 1);
      }

      size_t lrows = left._data.docvec->at(0)->numRows();
      size_t rrows = right._data.docvec->at(0)->numRows();

      while (lblock < lsize && rblock < rsize) {
        AqlValue const& lval =
            left._data.docvec->at(lblock)->getValueReference(litem, 0);
        AqlValue const& rval =
            right._data.docvec->at(rblock)->getValueReference(ritem, 0);

        int cmp = Compare(options, lval, rval, compareUtf8);

        if (cmp != 0) {
          return cmp;
        }
        if (++litem == lrows) {
          litem = 0;
          lblock++;
          if (lblock < lsize) {
            lrows = left._data.docvec->at(lblock)->numRows();
          }
        }
        if (++ritem == rrows) {
          ritem = 0;
          rblock++;
          if (rblock < rsize) {
            rrows = right._data.docvec->at(rblock)->numRows();
          }
        }
      }

      if (lblock == lsize && rblock == rsize) {
        // both blocks exhausted
        return 0;
      }

      return (lblock < lsize ? -1 : 1);
    }
    case RANGE: {
      if (left.range()->_low < right.range()->_low) {
        return -1;
      }
      if (left.range()->_low > right.range()->_low) {
        return 1;
      }
      if (left.range()->_high < right.range()->_high) {
        return -1;
      }
      if (left.range()->_high > right.range()->_high) {
        return 1;
      }
      return 0;
    }
  }

  return 0;
}


AqlValue::AqlValue(std::vector<arangodb::aql::SharedAqlItemBlockPtr>* docvec) noexcept {
  TRI_ASSERT(docvec != nullptr);
  _data.docvec = docvec;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (auto const& it : *_data.docvec) {
    TRI_ASSERT(it != nullptr);
  }
#endif
  setType(AqlValueType::DOCVEC);
}

AqlValue::AqlValue() noexcept {
  // construct a slice of type None
  // we will simply zero-initialize the two 64 bit words
  _data.words[0] = 0;
  _data.words[1] = 0;

  // VPACK_INLINE must have a value of 0, and VPackSlice::None must be equal
  // to a NUL byte too
  static_assert(AqlValueType::VPACK_INLINE == 0,
                "invalid value for VPACK_INLINE");
}

AqlValue::AqlValue(uint8_t const* pointer) {
  // we must get rid of Externals first here, because all
  // methods that use VPACK_SLICE_POINTER expect its contents
  // to be non-Externals
  if (*pointer == '\x1d') {
    // an external
    setPointer<false>(VPackSlice(pointer).resolveExternals().begin());
  } else {
    setPointer<false>(pointer);
  }
  TRI_ASSERT(!VPackSlice(_data.pointer).isExternal());
}

AqlValue::AqlValue(AqlValue const& other, void* data) noexcept {
  TRI_ASSERT(data != nullptr);
  TRI_ASSERT(other.type() != VPACK_INLINE);
  _data.data = data;
  // copy meta data
  _data.words[1] = other._data.words[1];
}

AqlValue::AqlValue(AqlValueHintNone const&) noexcept {
  _data.internal[0] = 0x00;  // none in VPack
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(AqlValueHintNull const&) noexcept {
  _data.internal[0] = 0x18;  // null in VPack
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(AqlValueHintBool const& v) noexcept {
  _data.internal[0] = v.value ? 0x1a : 0x19;  // true/false in VPack
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(AqlValueHintZero const&) noexcept {
  _data.internal[0] = 0x30;  // 0 in VPack
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(AqlValueHintDouble const& v) noexcept {
  double value = v.value;
  if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL || value == -HUGE_VAL) {
    // null
    _data.internal[0] = 0x18;
  } else {
    // a "real" double
    _data.internal[0] = 0x1b;
    uint64_t dv;
    memcpy(&dv, &value, sizeof(double));
    VPackValueLength vSize = sizeof(double);
    int i = 1;
    for (uint64_t x = dv; vSize > 0; vSize--) {
      _data.internal[i] = x & 0xff;
      x >>= 8;
      ++i;
    }
  }
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(AqlValueHintInt const& v) noexcept {
  int64_t value = v.value;
  if (value >= 0 && value <= 9) {
    // a smallint
    _data.internal[0] = static_cast<uint8_t>(0x30U + value);
  } else if (value < 0 && value >= -6) {
    // a negative smallint
    _data.internal[0] = static_cast<uint8_t>(0x40U + value);
  } else {
    uint8_t vSize = intLength(value);
    uint64_t x;
    if (vSize == 8) {
      x = toUInt64(value);
    } else {
      int64_t shift = 1LL << (vSize * 8 - 1);  // will never overflow!
      x = value >= 0 ? static_cast<uint64_t>(value)
                     : static_cast<uint64_t>(value + shift) + shift;
    }
    _data.internal[0] = 0x1fU + vSize;
    int i = 1;
    while (vSize-- > 0 && i < 16) {
      _data.internal[i] = x & 0xffU;  // GCC-10: complains about possible out of bounds access (i = 16)
      ++i;
      x >>= 8;
    }
  }
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(AqlValueHintUInt const& v) noexcept {
  uint64_t value = v.value;
  if (value <= 9) {
    // a Smallint, 0x30 - 0x39
    _data.internal[0] = static_cast<uint8_t>(0x30U + value);
  } else {
    // UInt, 0x28 - 0x2f
    int i = 1;
    uint8_t vSize = 0;
    do {
      vSize++;
      _data.internal[i] = static_cast<uint8_t>(value & 0xffU);
      ++i;
      value >>= 8;
    } while (value != 0);
    _data.internal[0] = 0x27U + vSize;
  }
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(char const* value, size_t length) {
  TRI_ASSERT(value != nullptr);
  if (length == 0) {
    // empty string
    _data.internal[0] = 0x40;
    setType(AqlValueType::VPACK_INLINE);
  } else if (length < sizeof(_data.internal) - 1) {
    // short string... can store it inline
    _data.internal[0] = static_cast<uint8_t>(0x40 + length);
    memcpy(_data.internal + 1, value, length);
    setType(AqlValueType::VPACK_INLINE);
  } else if (length <= 126) {
    // short string... cannot store inline, but we don't need to
    // create a full-featured Builder object here
    setManagedSliceData(MemoryOriginType::New, length + 1);
    _data.slice = new uint8_t[length + 1];
    _data.slice[0] = static_cast<uint8_t>(0x40U + length);
    memcpy(&_data.slice[1], value, length);
  } else {
    // long string
    // create a big enough uint8_t buffer
    size_t byteSize = length + 9;
    setManagedSliceData(MemoryOriginType::New, byteSize);
    _data.slice = new uint8_t[byteSize];
    _data.slice[0] = static_cast<uint8_t>(0xbfU);
    uint64_t v = length;
    for (uint64_t i = 0; i < 8; ++i) {
      _data.slice[i + 1] = v & 0xffU;
      v >>= 8;
    }
    memcpy(&_data.slice[9], value, length);
  }
}

AqlValue::AqlValue(std::string const& value)
    : AqlValue(value.data(), value.size()) {}

AqlValue::AqlValue(AqlValueHintEmptyArray const&) noexcept {
  _data.internal[0] = 0x01;  // empty array in VPack
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(AqlValueHintEmptyObject const&) noexcept {
  _data.internal[0] = 0x0a;  // empty object in VPack
  setType(AqlValueType::VPACK_INLINE);
}

AqlValue::AqlValue(arangodb::velocypack::Buffer<uint8_t>&& buffer) {
  // intentionally do not resolve externals here
  VPackValueLength length = buffer.length();
  if (length < sizeof(_data.internal)) {
    // Use inline value
    memcpy(_data.internal, buffer.data(), static_cast<size_t>(length));
    setType(AqlValueType::VPACK_INLINE);
    buffer.clear(); // for move semantics
  } else {
    // Use managed slice
    if (buffer.usesLocalMemory()) {
      setManagedSliceData(MemoryOriginType::New, length);
      _data.slice = new uint8_t[length];
      memcpy(&_data.slice[0], buffer.data(), length);
      buffer.clear(); // for move semantics
    } else {
      // steal dynamic memory from the Buffer
      setManagedSliceData(MemoryOriginType::Malloc, length);
      _data.slice = buffer.steal();
    }
  }
}

AqlValue::AqlValue(AqlValueHintDocumentNoCopy const& v) noexcept {
  setPointer<true>(v.ptr);
  TRI_ASSERT(!VPackSlice(_data.pointer).isExternal());
}

AqlValue::AqlValue(AqlValueHintCopy const& v) {
  TRI_ASSERT(v.ptr != nullptr);
  VPackSlice slice(v.ptr);
  initFromSlice(slice, slice.byteSize());
}

AqlValue::AqlValue(arangodb::velocypack::Slice slice) {
  initFromSlice(slice, slice.byteSize());
}

AqlValue::AqlValue(arangodb::velocypack::Slice slice, arangodb::velocypack::ValueLength length) {
  initFromSlice(slice, length);
}

AqlValue::AqlValue(int64_t low, int64_t high) {
  _data.range = new Range(low, high);
  setType(AqlValueType::RANGE);
}

bool AqlValue::requiresDestruction() const noexcept {
  auto t = type();
  return (t != VPACK_SLICE_POINTER && t != VPACK_INLINE);
}

bool AqlValue::isEmpty() const noexcept {
  return (_data.internal[0] == '\x00' &&
          _data.internal[sizeof(_data.internal) - 1] == VPACK_INLINE);
}

bool AqlValue::isPointer() const noexcept {
  return type() == VPACK_SLICE_POINTER;
}

bool AqlValue::isManagedDocument() const noexcept {
  return isPointer() && (_data.internal[sizeof(_data.internal) - 2] == 1);
}

bool AqlValue::isRange() const noexcept { return type() == RANGE; }

bool AqlValue::isDocvec() const noexcept { return type() == DOCVEC; }

Range const* AqlValue::range() const {
  TRI_ASSERT(isRange());
  return _data.range;
}

void AqlValue::erase() noexcept {
  _data.words[0] = 0;
  _data.words[1] = 0;
  TRI_ASSERT(isEmpty());
}

size_t AqlValue::memoryUsage() const noexcept {
  auto const t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
      return 0;
    case VPACK_MANAGED_SLICE:
      // byte size is stored in the first 6 bytes of the second uint64_t value
      return static_cast<size_t>((basics::bigToHost<uint64_t>(_data.words[1]) & 0xffffffffffff0000ULL) >> 16);
    case DOCVEC:
      // no need to count the memory usage for the item blocks in docvec.
      // these have already been counted elsewhere (in ctors of AqlItemBlock
      // and AqlItemBlock::setValue)
      return sizeof(_data.docvec) + sizeof(SharedAqlItemBlockPtr) * _data.docvec->size();
    case RANGE:
      return sizeof(Range);
  }
  return 0;
}

AqlValue::AqlValueType AqlValue::type() const noexcept {
  return static_cast<AqlValueType>(_data.internal[sizeof(_data.internal) - 1]);
}

void AqlValue::initFromSlice(arangodb::velocypack::Slice slice, arangodb::velocypack::ValueLength length) {
  // intentionally do not resolve externals here
  // if (slice.isExternal()) {
  //   // recursively resolve externals
  //   slice = slice.resolveExternals();
  // }
  TRI_ASSERT(length > 0);
  TRI_ASSERT(slice.byteSize() == length);
  if (length < sizeof(_data.internal)) {
    // Use inline value
    memcpy(_data.internal, slice.begin(), static_cast<size_t>(length));
    setType(AqlValueType::VPACK_INLINE);
  } else {
    // Use managed slice
    setManagedSliceData(MemoryOriginType::New, length);
    _data.slice = new uint8_t[length];
    memcpy(&_data.slice[0], slice.begin(), length);
  }
}

void AqlValue::setType(AqlValue::AqlValueType type) noexcept {
  _data.internal[sizeof(_data.internal) - 1] = type;
}

void* AqlValue::data() const noexcept {
  TRI_ASSERT(type() != VPACK_INLINE);
  TRI_ASSERT(_data.data != nullptr);
  return _data.data;
}

template <bool isManagedDoc>
void AqlValue::setPointer(uint8_t const* pointer) noexcept {
  _data.pointer = pointer;
  // we use the byte at (size - 2) to distinguish between data pointing to
  // database documents (size[-2] == 1) and other data(size[-2] == 0)
  _data.internal[sizeof(_data.internal) - 2] = isManagedDoc ? 1 : 0;
  _data.internal[sizeof(_data.internal) - 1] = AqlValueType::VPACK_SLICE_POINTER;
}

template void AqlValue::setPointer<true>(uint8_t const* pointer) noexcept;
template void AqlValue::setPointer<false>(uint8_t const* pointer) noexcept;

static_assert(std::is_standard_layout<AqlValue>::value, "AqlValue has an invalid type");

AqlValueHintCopy::AqlValueHintCopy(uint8_t const* ptr) noexcept : ptr(ptr) {}
AqlValueHintDocumentNoCopy::AqlValueHintDocumentNoCopy(uint8_t const* v) noexcept : ptr(v) {}
AqlValueHintBool::AqlValueHintBool(bool v) noexcept : value(v) {}
AqlValueHintDouble::AqlValueHintDouble(double v) noexcept : value(v) {}
AqlValueHintInt::AqlValueHintInt(int64_t v) noexcept : value(v) {}
AqlValueHintInt::AqlValueHintInt(int v) noexcept : value(int64_t(v)) {}
AqlValueHintUInt::AqlValueHintUInt(uint64_t v) noexcept : value(v) {}

AqlValueGuard::AqlValueGuard(AqlValue& value, bool destroy) noexcept
    : _value(value), _destroy(destroy) {}

AqlValueGuard::~AqlValueGuard() noexcept {
  if (_destroy) {
    _value.destroy();
  }
}

void AqlValueGuard::steal() noexcept { _destroy = false; }

AqlValue& AqlValueGuard::value() noexcept { return _value; }

size_t std::hash<arangodb::aql::AqlValue>::operator()(arangodb::aql::AqlValue const& x) const
    noexcept {
  if (x.type() == arangodb::aql::AqlValue::VPACK_INLINE) {
      return static_cast<size_t>(arangodb::velocypack::Slice(&x._data.internal[0]).hash());
  }
  // treat all other pointer types the same, because they will
  // have the same bit representations
  return std::hash<void const*>()(x._data.pointer);
}

bool std::equal_to<arangodb::aql::AqlValue>::operator()(arangodb::aql::AqlValue const& a,
                                                        arangodb::aql::AqlValue const& b) const
    noexcept {
  arangodb::aql::AqlValue::AqlValueType type = a.type();
  if (type != b.type()) {
    return false;
  }
  if (type == arangodb::aql::AqlValue::VPACK_INLINE) {
    return arangodb::velocypack::Slice(&a._data.internal[0])
           .binaryEquals(arangodb::velocypack::Slice(&b._data.internal[0]));
  }
  // treat all other pointer types the same, because they will
  // have the same bit representations
  return a._data.pointer == b._data.pointer;
}
