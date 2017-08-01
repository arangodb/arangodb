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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "AqlValue.h"
#include "Aql/AqlItemBlock.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Context.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <array>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief hashes the value
uint64_t AqlValue::hash(transaction::Methods* trx, uint64_t seed) const {
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      // we must use the slow hash function here, because a value may have
      // different representations in case it's an array/object/number
      return slice().normalizedHash(seed);
    }
    case DOCVEC:
    case RANGE: {
      VPackBuilder builder;
      toVelocyPack(trx, builder, false);
      // we must use the slow hash function here, because a value may have
      // different representations in case its an array/object/number
      return builder.slice().normalizedHash(seed);
    }
  }

  return 0;
}

/// @brief whether or not the value contains a none value
bool AqlValue::isNone() const noexcept {
  AqlValueType t = type();
  if (t == DOCVEC || t == RANGE) {
    return false;
  }

  try {
    return slice().isNone();
  } catch (...) {
    return false;
  }
}

/// @brief whether or not the value is a null value
bool AqlValue::isNull(bool emptyIsNull) const noexcept {
  AqlValueType t = type();
  if (t == DOCVEC || t == RANGE) {
    return false;
  }

  try {
    VPackSlice s(slice());
    return (s.isNull() || (emptyIsNull && s.isNone()));
  } catch (...) {
    return false;
  }
}

/// @brief whether or not the value is a boolean value
bool AqlValue::isBoolean() const noexcept {
  AqlValueType t = type();
  if (t == DOCVEC || t == RANGE) {
    return false;
  }
  try {
    return slice().isBoolean();
  } catch (...) {
    return false;
  }
}

/// @brief whether or not the value is a number
bool AqlValue::isNumber() const noexcept {
  AqlValueType t = type();
  if (t == DOCVEC || t == RANGE) {
    return false;
  }
  try {
    return slice().isNumber();
  } catch (...) {
    return false;
  }
}

/// @brief whether or not the value is a string
bool AqlValue::isString() const noexcept {
  AqlValueType t = type();
  if (t == DOCVEC || t == RANGE) {
    return false;
  }
  try {
    return slice().isString();
  } catch (...) {
    return false;
  }
}

/// @brief whether or not the value is an object
bool AqlValue::isObject() const noexcept {
  AqlValueType t = type();
  if (t == RANGE || t == DOCVEC) {
    return false;
  }
  try {
    return slice().isObject();
  } catch (...) {
    return false;
  }
}

/// @brief whether or not the value is an array (note: this treats ranges
/// as arrays, too!)
bool AqlValue::isArray() const noexcept {
  AqlValueType t = type();
  if (t == RANGE || t == DOCVEC) {
    return true;
  }
  try {
    return slice().isArray();
  } catch (...) {
    return false;
  }
}

std::array<std::string const, 8> AqlValue::typeStrings = { {
  "none",
  "null",
  "bool",
  "number",
  "string",
  "object",
  "array",
  "unknown"} };

std::string const & AqlValue::getTypeString() const noexcept {
  if(isNone()) {
    return typeStrings[0];
  } else if(isNull(true)) {
     return typeStrings[1];
  } else if(isBoolean()) {
     return typeStrings[2];
  } else if(isNumber()) {
     return typeStrings[3];
  } else if(isString()) {
     return typeStrings[4];
  } else if(isObject()) {
     return typeStrings[5];
  } else if(isArray()){
     return typeStrings[6];
  }
  return typeStrings[7];
}

/// @brief get the (array) length (note: this treats ranges as arrays, too!)
size_t AqlValue::length() const {
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      return static_cast<size_t>(slice().length());
    }
    case DOCVEC: {
      return docvecSize();
    }
    case RANGE: {
      return range()->size();
    }
  }
  TRI_ASSERT(false);
  return 0;
}
  
/// @brief get the (array) element at position 
AqlValue AqlValue::at(transaction::Methods* trx,
                      int64_t position, bool& mustDestroy, 
                      bool doCopy) const {
  mustDestroy = false;
  switch (type()) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    // fall-through intentional
    case VPACK_INLINE:
    // fall-through intentional
    case VPACK_MANAGED_SLICE:
    // fall-through intentional
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
      // fall-through intentional
      break;
    }
    case DOCVEC: {
      size_t const n = docvecSize();
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }
      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        size_t total = 0;
        for (auto const& it : *_data.docvec) {
          if (position < static_cast<int64_t>(total + it->size())) {
            // found the correct vector
            if (doCopy) {
              mustDestroy = true;
              return it
                  ->getValueReference(static_cast<size_t>(position - total), 0)
                  .clone();
            }
            return it->getValue(static_cast<size_t>(position - total), 0);
          }
          total += it->size();
        }
      }
      // fall-through intentional
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
        return AqlValue(_data.range->at(static_cast<size_t>(position)));
      }
      // fall-through intentional
      break;
    }
  }

  // default is to return null
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief get the _key attribute from an object/document
AqlValue AqlValue::getKeyAttribute(transaction::Methods* trx,
                                   bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  switch (type()) {
    case VPACK_SLICE_POINTER:
    // fall-through intentional
      doCopy = false;
    case VPACK_INLINE:
    // fall-through intentional
    case VPACK_MANAGED_SLICE:
    // fall-through intentional
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief get the _id attribute from an object/document
AqlValue AqlValue::getIdAttribute(transaction::Methods* trx,
                                  bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  switch (type()) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    // fall-through intentional
    case VPACK_INLINE:
    // fall-through intentional
    case VPACK_MANAGED_SLICE:
    // fall-through intentional
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
      if (s.isObject()) {
        VPackSlice found = transaction::helpers::extractIdFromDocument(s);
        if (found.isCustom()) {
          // _id as a custom type needs special treatment
          mustDestroy = true;
          return AqlValue(transaction::helpers::extractIdString(trx->resolver(), found, s));
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
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief get the _from attribute from an object/document
AqlValue AqlValue::getFromAttribute(transaction::Methods* trx,
                                    bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  switch (type()) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    // fall-through intentional
    case VPACK_INLINE:
    // fall-through intentional
    case VPACK_MANAGED_SLICE:
    // fall-through intentional
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief get the _to attribute from an object/document
AqlValue AqlValue::getToAttribute(transaction::Methods* trx,
                                  bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  switch (type()) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    // fall-through intentional
    case VPACK_INLINE:
    // fall-through intentional
    case VPACK_MANAGED_SLICE:
    // fall-through intentional
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief get the (object) element by name
AqlValue AqlValue::get(transaction::Methods* trx,
                       std::string const& name, bool& mustDestroy,
                       bool doCopy) const {
  mustDestroy = false;
  switch (type()) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    // fall-through intentional
    case VPACK_INLINE:
    // fall-through intentional
    case VPACK_MANAGED_SLICE:
    // fall-through intentional
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
      if (s.isObject()) {
        VPackSlice found(s.get(name));
        if (found.isCustom()) {
          // _id needs special treatment
          mustDestroy = true;
          return AqlValue(trx->extractIdString(s));
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
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief get the (object) element(s) by name
AqlValue AqlValue::get(transaction::Methods* trx,
                       std::vector<std::string> const& names, 
                       bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  if (names.empty()) {
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }

  switch (type()) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
    // fall-through intentional
    case VPACK_INLINE:
    // fall-through intentional
    case VPACK_MANAGED_SLICE:
    // fall-through intentional
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
      if (s.isObject()) {
        if (s.isExternal()) {
          s = s.resolveExternal();
        }
        VPackSlice prev;
        size_t const n = names.size();
        for (size_t i = 0; i < n; ++i) {
          // fetch subattribute
          prev = s;
          s = s.get(names[i]);
          if (s.isExternal()) {
            s = s.resolveExternal();
          }

          if (s.isNone()) {
            // not found
            return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
          } else if (s.isCustom()) {
            // _id needs special treatment
            if (i + 1 == n) {
              // x.y._id
              mustDestroy = true;
              return AqlValue(transaction::helpers::extractIdString(trx->resolver(), s, prev));
            }
            // x._id.y
            return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
          } else if (i + 1 < n && !s.isObject()) {
            return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
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
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      // will return null
      break;
    }
  }

  // default is to return null
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

/// @brief check whether an object has a specific key
bool AqlValue::hasKey(transaction::Methods* trx,
                      std::string const& name) const {
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
double AqlValue::toDouble(transaction::Methods* trx) const {
  bool failed; // will be ignored
  return toDouble(trx, failed);
}

double AqlValue::toDouble(transaction::Methods* trx, bool& failed) const {
  failed = false;
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
        std::string v(s.copyString());
        try {
          size_t behind = 0;
          double value = std::stod(v, &behind);
          while (behind < v.size()) {
            char c = v[behind];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
              failed = true;
              return 0.0;
            }
            ++behind;
          }
          TRI_ASSERT(!failed);
          return value;
        } catch (...) {
          if (v.empty()) {
            return 0.0;
          }
          // conversion failed
          break;
        }
      } else if (s.isArray()) {
        auto length = s.length();
        if (length == 0) {
          return 0.0;
        }
        if (length == 1) {
          bool mustDestroy;  // we can ignore destruction here
          return at(trx, 0, mustDestroy, false).toDouble(trx, failed);
        }
      }
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      if (length() == 1) {
        bool mustDestroy;  // we can ignore destruction here
        return at(trx, 0, mustDestroy, false).toDouble(trx, failed);
      }
      // will return 0
      return 0.0;
    }
  }

  failed = true;
  return 0.0;
}

/// @brief get the numeric value of an AqlValue
int64_t AqlValue::toInt64(transaction::Methods* trx) const {
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
          return at(trx, 0, mustDestroy, false).toInt64(trx);
        }
      }
      // fall-through intentional
      break;
    }
    case DOCVEC:
    case RANGE: {
      if (length() == 1) {
        bool mustDestroy;
        return at(trx, 0, mustDestroy, false).toInt64(trx);
      }
      // will return 0
      break;
    }
  }

  return 0;
}

/// @brief whether or not the contained value evaluates to true
bool AqlValue::toBoolean() const {
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(slice());
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
size_t AqlValue::docvecSize() const {
  TRI_ASSERT(type() == DOCVEC);
  size_t s = 0;
  for (auto const& it : *_data.docvec) {
    s += it->size();
  }
  return s;
}

/// @brief construct a V8 value as input for the expression execution in V8
/// only construct those attributes that are needed in the expression
v8::Handle<v8::Value> AqlValue::toV8Partial(
    v8::Isolate* isolate, transaction::Methods* trx,
    std::unordered_set<std::string> const& attributes) const {
  AqlValueType t = type();

  if (t == DOCVEC || t == RANGE) {
    // cannot make use of these types
    return v8::Null(isolate);
  }

  VPackOptions* options = trx->transactionContext()->getVPackOptions();
  VPackSlice s(slice());

  if (s.isObject()) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    // only construct those attributes needed
    size_t left = attributes.size();

    // we can only have got here if we had attributes
    TRI_ASSERT(left > 0);

    // iterate over all the object's attributes
    for (auto const& it : VPackObjectIterator(s, false)) {
      // check if we need to render this attribute
      auto it2 = attributes.find(it.key.copyString());

      if (it2 == attributes.end()) {
        // we can skip the attribute
        continue;
      }

      result->ForceSet(TRI_V8_STD_STRING((*it2)),
                       TRI_VPackToV8(isolate, it.value, options, &s));

      if (--left == 0) {
        // we have rendered all required attributes
        break;
      }
    }

    // return partial object
    return result;
  }

  // fallback
  return TRI_VPackToV8(isolate, s, options);
}

/// @brief construct a V8 value as input for the expression execution in V8
v8::Handle<v8::Value> AqlValue::toV8(
    v8::Isolate* isolate, transaction::Methods* trx) const {
  
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      VPackOptions* options = trx->transactionContext()->getVPackOptions();
      return TRI_VPackToV8(isolate, slice(), options);
    }
    case DOCVEC: {
      // calculate the result array length
      size_t const s = docvecSize();
      // allocate the result array
      v8::Handle<v8::Array> result =
          v8::Array::New(isolate, static_cast<int>(s));
      uint32_t j = 0;  // output row count
      for (auto const& it : *_data.docvec) {
        size_t const n = it->size();
        for (size_t i = 0; i < n; ++i) {
          result->Set(j++, it->getValueReference(i, 0).toV8(isolate, trx));

          if (V8PlatformFeature::isOutOfMemory(isolate)) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
      }
      return result;
    }
    case RANGE: {
      size_t const n = _data.range->size();
      v8::Handle<v8::Array> result =
          v8::Array::New(isolate, static_cast<int>(n));

      for (uint32_t i = 0; i < n; ++i) {
        // is it safe to use a double here (precision loss)?
        result->Set(
            i, v8::Number::New(isolate, static_cast<double>(_data.range->at(
                                            static_cast<size_t>(i)))));

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
void AqlValue::toVelocyPack(transaction::Methods* trx, 
                            arangodb::velocypack::Builder& builder,
                            bool resolveExternals) const {
  switch (type()) {
    case VPACK_SLICE_POINTER:
      if (!resolveExternals && isManagedDocument()) {
        builder.addExternal(_data.pointer);
        break;
      }  // fallthrough intentional
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      if (resolveExternals) {
        bool const sanitizeExternals = true;
        bool const sanitizeCustom = true;
        arangodb::basics::VelocyPackHelper::sanitizeNonClientTypes(slice(), VPackSlice::noneSlice(), builder, trx->transactionContextPtr()->getVPackOptions(), sanitizeExternals, sanitizeCustom);
      } else {
        builder.add(slice());
      }
      break;
    }
    case DOCVEC: {
      builder.openArray();
      for (auto const& it : *_data.docvec) {
        size_t const n = it->size();
        for (size_t i = 0; i < n; ++i) {
          it->getValueReference(i, 0).toVelocyPack(trx, builder,
                                                   resolveExternals);
        }
      }
      builder.close();
      break;
    }
    case RANGE: {
      builder.openArray();
      size_t const n = _data.range->size();
      for (size_t i = 0; i < n; ++i) {
        builder.add(VPackValue(_data.range->at(i)));
      }
      builder.close();
      break;
    }
  }
}

/// @brief materializes a value into the builder
AqlValue AqlValue::materialize(transaction::Methods* trx, bool& hasCopied,
                               bool resolveExternals) const {
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_BUFFER: {
      hasCopied = false;
      return *this;
    }
    case DOCVEC:
    case RANGE: {
      bool shouldDelete = true;
      ConditionalDeleter<VPackBuffer<uint8_t>> deleter(shouldDelete);
      std::shared_ptr<VPackBuffer<uint8_t>> buffer(new VPackBuffer<uint8_t>,
                                                   deleter);
      VPackBuilder builder(buffer);
      toVelocyPack(trx, builder, resolveExternals);
      hasCopied = true;
      return AqlValue(buffer.get(), shouldDelete);
    }
  }

  // we shouldn't get here
  hasCopied = false;
  return AqlValue();
}

/// @brief clone a value
AqlValue AqlValue::clone() const {
  switch (type()) {
    case VPACK_SLICE_POINTER: {
      if (isManagedDocument()) {
        // copy from externally managed document. this will not copy the data
        return AqlValue(AqlValueHintNoCopy(_data.pointer));
      }
      // copy from regular pointer. this may copy the data
      return AqlValue(_data.pointer);
    }
    case VPACK_INLINE: {
      // copy internal data
      return AqlValue(slice());
    }
    case VPACK_MANAGED_SLICE: {
      return AqlValue(AqlValueHintCopy(_data.slice));
    }
    case VPACK_MANAGED_BUFFER: {
      // copy buffer
      return AqlValue(VPackSlice(_data.buffer->data()));
    }
    case DOCVEC: {
      auto c = std::make_unique<std::vector<AqlItemBlock*>>();
      c->reserve(docvecSize());
      try {
        for (auto const& it : *_data.docvec) {
          c->emplace_back(it->slice(0, it->size()));
        }
      } catch (...) {
        for (auto& it : *c) {
          delete it;
        }
        throw;
      }
      return AqlValue(c.release());
    }
    case RANGE: {
      // create a new value with a new range
      return AqlValue(range()->_low, range()->_high);
    }
  }

  TRI_ASSERT(false);
  return AqlValue();
}

/// @brief destroy the value's internals
void AqlValue::destroy() {
  switch (type()) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE: {
      // nothing to do
      return;
    }
    case VPACK_MANAGED_SLICE: {
      delete[] _data.slice;
      break;
    }
    case VPACK_MANAGED_BUFFER: {
      delete _data.buffer;
      break;
    }
    case DOCVEC: {
      for (auto& it : *_data.docvec) {
        delete it;
      }
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
  switch (type()) {
    case VPACK_SLICE_POINTER: {
      return VPackSlice(_data.pointer);
    }
    case VPACK_INLINE: {
      VPackSlice s(&_data.internal[0]);
      if (s.isExternal()) {
        s = s.resolveExternal();
      }
      return s;
    }
    case VPACK_MANAGED_SLICE: {
      VPackSlice s(_data.slice);
      if (s.isExternal()) {
        s = s.resolveExternal();
      }
      return s;
    }
    case VPACK_MANAGED_BUFFER: {
      VPackSlice s(_data.buffer->data());
      if (s.isExternal()) {
        s = s.resolveExternal();
      }
      return s;
    }
    case DOCVEC:
    case RANGE: {
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
}

/// @brief create an AqlValue from a vector of AqlItemBlock*s
AqlValue AqlValue::CreateFromBlocks(
    transaction::Methods* trx, std::vector<AqlItemBlock*> const& src,
    std::vector<std::string> const& variableNames) {
  bool shouldDelete = true;
  ConditionalDeleter<VPackBuffer<uint8_t>> deleter(shouldDelete);
  std::shared_ptr<VPackBuffer<uint8_t>> buffer(new VPackBuffer<uint8_t>,
                                               deleter);
  VPackBuilder builder(buffer);
  builder.openArray();

  for (auto const& current : src) {
    RegisterId const n = current->getNrRegs();

    std::vector<RegisterId> registers;
    for (RegisterId j = 0; j < n; ++j) {
      // temporaries don't have a name and won't be included
      if (!variableNames[j].empty()) {
        registers.emplace_back(j);
      }
    }

    for (size_t i = 0; i < current->size(); ++i) {
      builder.openObject();

      // only enumerate the registers that are left
      for (auto const& reg : registers) {
        builder.add(VPackValue(variableNames[reg]));
        current->getValueReference(i, reg).toVelocyPack(trx, builder, false);
      }

      builder.close();
    }
  }

  builder.close();
  return AqlValue(buffer.get(), shouldDelete);
}

/// @brief create an AqlValue from a vector of AqlItemBlock*s
AqlValue AqlValue::CreateFromBlocks(
    transaction::Methods* trx, std::vector<AqlItemBlock*> const& src,
    arangodb::aql::RegisterId expressionRegister) {
  bool shouldDelete = true;
  ConditionalDeleter<VPackBuffer<uint8_t>> deleter(shouldDelete);
  std::shared_ptr<VPackBuffer<uint8_t>> buffer(new VPackBuffer<uint8_t>,
                                               deleter);
  VPackBuilder builder(buffer);

  builder.openArray();

  for (auto const& current : src) {
    for (size_t i = 0; i < current->size(); ++i) {
      current->getValueReference(i, expressionRegister)
          .toVelocyPack(trx, builder, false);
    }
  }

  builder.close();
  return AqlValue(buffer.get(), shouldDelete);
}

/// @brief 3-way comparison for AqlValue objects
int AqlValue::Compare(transaction::Methods* trx, AqlValue const& left,
                      AqlValue const& right, bool compareUtf8) {
  AqlValue::AqlValueType const leftType = left.type();
  AqlValue::AqlValueType const rightType = right.type();

  if (leftType != rightType) {
    if (leftType == RANGE || rightType == RANGE || leftType == DOCVEC ||
        rightType == DOCVEC) {
      // range|docvec against x
      VPackBuilder leftBuilder;
      left.toVelocyPack(trx, leftBuilder, false);

      VPackBuilder rightBuilder;
      right.toVelocyPack(trx, rightBuilder, false);

      return arangodb::basics::VelocyPackHelper::compare(
          leftBuilder.slice(), rightBuilder.slice(), compareUtf8, trx->transactionContextPtr()->getVPackOptions());
    }
    // fall-through to other types intentional
  }

  // if we get here, types are equal or can be treated as being equal

  switch (leftType) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE: 
    case VPACK_MANAGED_BUFFER: {
      return arangodb::basics::VelocyPackHelper::compare(
          left.slice(), right.slice(), compareUtf8, trx->transactionContextPtr()->getVPackOptions());
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

      size_t lrows = left._data.docvec->at(0)->size();
      size_t rrows = right._data.docvec->at(0)->size();

      while (lblock < lsize && rblock < rsize) {
        AqlValue const& lval =
            left._data.docvec->at(lblock)->getValueReference(litem, 0);
        AqlValue const& rval =
            right._data.docvec->at(rblock)->getValueReference(ritem, 0);

        int cmp = Compare(trx, lval, rval, compareUtf8);

        if (cmp != 0) {
          return cmp;
        }
        if (++litem == lrows) {
          litem = 0;
          lblock++;
          if (lblock < lsize) {
            lrows = left._data.docvec->at(lblock)->size();
          }
        }
        if (++ritem == rrows) {
          ritem = 0;
          rblock++;
          if (rblock < rsize) {
            rrows = right._data.docvec->at(rblock)->size();
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
