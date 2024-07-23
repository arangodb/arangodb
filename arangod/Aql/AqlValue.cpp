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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "AqlValue.h"

#include "Aql/Arithmetic.h"
#include "Aql/Range.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#ifdef USE_V8
#include "V8/v8-vpack.h"
#endif

#include <absl/strings/numbers.h>

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

#include <bit>
#include <type_traits>

#ifndef velocypack_malloc
#error velocypack_malloc must be defined
#endif

using namespace arangodb;
using namespace aql;

void AqlValue::setPointer(uint8_t const* pointer) noexcept {
  setType(AqlValueType::VPACK_SLICE_POINTER);
  _data.slicePointerMeta.pointer = pointer;
}

uint64_t AqlValue::hash(uint64_t seed) const {
  auto t = type();
  if (ADB_UNLIKELY(t == RANGE)) {
    uint64_t const n = _data.rangeMeta.range->size();

    // simon: copied from VPackSlice::normalizedHash()
    // normalize arrays by hashing array length and iterating
    // over all array members
    uint64_t const tmp = n ^ 0xba5bedf00d;
    uint64_t value = VELOCYPACK_HASH(&tmp, sizeof(tmp), seed);

    for (uint64_t i = 0; i < n; ++i) {
      // upcast integer values to double
      double v = static_cast<double>(_data.rangeMeta.range->at(i));
      value ^= VELOCYPACK_HASH(&v, sizeof(v), value);
    }

    return value;
  }
  // we must use the slow hash function here, because a value may have
  // different representations in case it's an array/object/number
  return slice(t).normalizedHash(seed);
}

bool AqlValue::isNone() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_INLINE:
      return VPackSlice(_data.inlineSliceMeta.slice).isNone();
    case VPACK_SLICE_POINTER:
      return VPackSlice(_data.slicePointerMeta.pointer).isNone();
    case VPACK_MANAGED_SLICE:
      return VPackSlice(_data.managedSliceMeta.pointer).isNone();
    default:
      return false;
  }
}

bool AqlValue::isNull(bool emptyIsNull) const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_INLINE: {
      VPackSlice s{_data.inlineSliceMeta.slice};
      return s.isNull() || (emptyIsNull && s.isNone());
    }
    case VPACK_SLICE_POINTER: {
      VPackSlice s{_data.slicePointerMeta.pointer};
      return s.isNull() || (emptyIsNull && s.isNone());
    }
    case VPACK_MANAGED_SLICE: {
      VPackSlice s{_data.managedSliceMeta.pointer};
      return s.isNull() || (emptyIsNull && s.isNone());
    }
    default:
      return false;
  }
}

bool AqlValue::isBoolean() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_INLINE:
      return VPackSlice{_data.inlineSliceMeta.slice}.isBoolean();
    case VPACK_SLICE_POINTER:
      return VPackSlice{_data.slicePointerMeta.pointer}.isBoolean();
    case VPACK_MANAGED_SLICE:
      return VPackSlice{_data.managedSliceMeta.pointer}.isBoolean();
    default:
      return false;
  }
}

bool AqlValue::isNumber() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_INLINE_INT64:
    case VPACK_INLINE_UINT64:
    case VPACK_INLINE_DOUBLE:
      return true;
    case VPACK_INLINE:
      return VPackSlice{_data.inlineSliceMeta.slice}.isNumber();
    case VPACK_SLICE_POINTER:
      return VPackSlice{_data.slicePointerMeta.pointer}.isNumber();
    case VPACK_MANAGED_SLICE:
      return VPackSlice{_data.managedSliceMeta.pointer}.isNumber();
    default:
      return false;
  }
}

bool AqlValue::isString() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_INLINE:
      return VPackSlice{_data.inlineSliceMeta.slice}.isString();
    case VPACK_SLICE_POINTER:
      return VPackSlice{_data.slicePointerMeta.pointer}.isString();
    case VPACK_MANAGED_SLICE:
      return VPackSlice{_data.managedSliceMeta.pointer}.isString();
    case VPACK_MANAGED_STRING:
      return _data.managedStringMeta.toSlice().isString();
    default:
      return false;
  }
}

bool AqlValue::isObject() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_INLINE:
      return VPackSlice{_data.inlineSliceMeta.slice}.isObject();
    case VPACK_SLICE_POINTER:
      return VPackSlice{_data.slicePointerMeta.pointer}.isObject();
    case VPACK_MANAGED_SLICE:
      return VPackSlice{_data.managedSliceMeta.pointer}.isObject();
    case VPACK_MANAGED_STRING:
      return _data.managedStringMeta.toSlice().isObject();
    default:
      return false;
  }
}

bool AqlValue::isArray() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_INLINE:
      return VPackSlice{_data.inlineSliceMeta.slice}.isArray();
    case VPACK_SLICE_POINTER:
      return VPackSlice{_data.slicePointerMeta.pointer}.isArray();
    case VPACK_MANAGED_SLICE:
      return VPackSlice{_data.managedSliceMeta.pointer}.isArray();
    case VPACK_MANAGED_STRING:
      return _data.managedStringMeta.toSlice().isArray();
    case RANGE:
      return true;
    default:
      return false;
  }
}

std::string_view AqlValue::getTypeString() const noexcept {
  VPackSlice s;
  switch (type()) {
    case VPACK_INLINE_INT64:
    case VPACK_INLINE_UINT64:
    case VPACK_INLINE_DOUBLE:
      return "number";
    case VPACK_INLINE:
      s = VPackSlice{_data.inlineSliceMeta.slice};
      break;
    case VPACK_SLICE_POINTER:
      s = VPackSlice{_data.slicePointerMeta.pointer};
      break;
    case VPACK_MANAGED_SLICE:
      s = VPackSlice{_data.managedSliceMeta.pointer};
      break;
    case VPACK_MANAGED_STRING:
      s = _data.managedStringMeta.toSlice();
      break;
    case RANGE:
      return "array";
  }
  switch (s.type()) {
    case velocypack::ValueType::Null:
      return "null";
    case velocypack::ValueType::Bool:
      return "bool";
    case velocypack::ValueType::Array:
      return "array";
    case velocypack::ValueType::Object:
      return "object";
    case velocypack::ValueType::Double:
    case velocypack::ValueType::Int:
    case velocypack::ValueType::UInt:
    case velocypack::ValueType::SmallInt:
      return "number";
    case velocypack::ValueType::String:
      return "string";
    default:
      return "none";
  }
}

size_t AqlValue::length() const {
  auto t = type();
  switch (t) {
    case RANGE:
      return range()->size();
    default:
      return slice(t).length();
  }
}

AqlValue AqlValue::at(int64_t position, bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isArray()) {
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
      break;
    case RANGE: {
      size_t const n = range()->size();
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }

      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        return AqlValue(AqlValueHintInt(
            _data.rangeMeta.range->at(static_cast<size_t>(position))));
      }
    } break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

AqlValue AqlValue::at(int64_t position, size_t n, bool& mustDestroy,
                      bool doCopy) const {
  mustDestroy = false;
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isArray()) {
        if (position < 0) {
          // a negative position is allowed
          position = static_cast<int64_t>(n) + position;
        }
        if (position >= 0 && position < static_cast<int64_t>(n)) {
          s = s.at(position);
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(s);
          }
          // return a reference to an existing slice
          return AqlValue{s.begin()};
        }
      }
      break;
    case RANGE:
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }

      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        return AqlValue(AqlValueHintInt(
            _data.rangeMeta.range->at(static_cast<size_t>(position))));
      }
      break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

AqlValue AqlValue::getKeyAttribute(bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isObject()) {
        auto const found = transaction::helpers::extractKeyFromDocument(s);
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue{found};
          }
          // return a reference to an existing slice
          return AqlValue{found.begin()};
        }
      }
      break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

AqlValue AqlValue::getIdAttribute(CollectionNameResolver const& resolver,
                                  bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isObject()) {
        auto const found = transaction::helpers::extractIdFromDocument(s);
        if (found.isCustom()) {
          // _id as a custom type needs special treatment
          mustDestroy = true;
          return AqlValue{
              transaction::helpers::extractIdString(&resolver, found, s)};
        }
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue{found};
          }
          // return a reference to an existing slice
          return AqlValue{found.begin()};
        }
      }
      break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

AqlValue AqlValue::getFromAttribute(bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isObject()) {
        auto const found = transaction::helpers::extractFromFromDocument(s);
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue{found};
          }
          // return a reference to an existing slice
          return AqlValue{found.begin()};
        }
      }
      break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

AqlValue AqlValue::getToAttribute(bool& mustDestroy, bool doCopy) const {
  mustDestroy = false;
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isObject()) {
        auto const found = transaction::helpers::extractToFromDocument(s);
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue{found};
          }
          // return a reference to an existing slice
          return AqlValue{found.begin()};
        }
      }
      break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

AqlValue AqlValue::get(CollectionNameResolver const& resolver,
                       std::string_view name, bool& mustDestroy,
                       bool doCopy) const {
  mustDestroy = false;
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isObject()) {
        auto const found = s.get(name);
        if (found.isCustom()) {
          // _id needs special treatment
          mustDestroy = true;
          return AqlValue{
              transaction::helpers::extractIdString(&resolver, s, {})};
        }
        if (!found.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue(found);
          }
          // return a reference to an existing slice
          return AqlValue{found.begin()};
        }
      }
      break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

AqlValue AqlValue::get(CollectionNameResolver const& resolver,
                       std::vector<std::string> const& names, bool& mustDestroy,
                       bool doCopy) const {
  mustDestroy = false;
  if (names.empty()) {
    return AqlValue{AqlValueHintNull{}};
  }
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
      doCopy = false;
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      if (auto s = slice(t); s.isObject()) {
        VPackSlice prev;
        size_t const n = names.size();
        for (size_t i = 0; i < n; ++i) {
          // fetch subattribute
          prev = s;
          s = s.get(names[i]);
          TRI_ASSERT(!s.isExternal());

          if (s.isNone()) {
            // not found
            return AqlValue{AqlValueHintNull{}};
          } else if (s.isCustom()) {
            // _id needs special treatment
            if (i + 1 == n) {
              // x.y._id
              mustDestroy = true;
              return AqlValue(
                  transaction::helpers::extractIdString(&resolver, s, prev));
            }
            // x._id.y
            return AqlValue{AqlValueHintNull{}};
          } else if (i + 1 < n && !s.isObject()) {
            return AqlValue{AqlValueHintNull{}};
          }
        }

        if (!s.isNone()) {
          if (doCopy) {
            mustDestroy = true;
            return AqlValue{s};
          }
          // return a reference to an existing slice
          return AqlValue{s.begin()};
        }
      }
      break;
    default:
      break;
  }
  return AqlValue{AqlValueHintNull{}};
}

bool AqlValue::hasKey(std::string_view name) const {
  auto t = type();
  switch (t) {
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING: {
      auto s = slice(t);
      return s.isObject() && s.hasKey(name);
    }
    default:
      return false;
  }
}

double AqlValue::toDouble() const {
  bool failed;
  return toDouble(failed);
}

double AqlValue::toDouble(bool& failed) const {
  failed = false;
  auto t = type();
  switch (t) {
    case VPACK_INLINE_INT64:
      // TODO(MBkkt) Not all int64_t fit to double without precision lost
      return static_cast<double>(absl::little_endian::ToHost(
          _data.longNumberMeta.data.intLittleEndian.val));
    case VPACK_INLINE_UINT64:
      // TODO(MBkkt) Not all uint64_t fit to double without precision lost
      return static_cast<double>(absl::little_endian::ToHost(
          _data.longNumberMeta.data.uintLittleEndian.val));
    case VPACK_INLINE_DOUBLE: {
      auto h = absl::little_endian::ToHost(
          _data.longNumberMeta.data.uintLittleEndian.val);
      return std::bit_cast<double>(h);
    }
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING: {
      auto s = slice(t);
      if (s.isNull()) {
        return 0.0;
      }
      if (s.isNumber()) {
        return s.getNumber<double>();
      }
      if (s.isBool()) {
        return s.getBool() ? 1.0 : 0.0;
      }
      if (s.isString()) {
        // TODO(MBkkt) it's really strange way to make conversation
        return stringToNumber(s.copyString(), failed);
      }
      if (s.isArray()) {
        auto const length = s.length();
        if (length == 0) {
          return 0.0;
        }
        if (length == 1) {
          // We can ignore destruction here, because doCopy is false
          bool mustDestroy;
          return at(0, mustDestroy, false).toDouble(failed);
        }
      }
    } break;
    case RANGE:
      if (range()->size() == 1) {
        // We can ignore destruction here, because doCopy is false
        bool mustDestroy;
        return at(0, mustDestroy, false).toDouble(failed);
      }
      return 0.0;
  }
  failed = true;
  return 0.0;
}

int64_t AqlValue::toInt64() const {
  auto checkOverflow = [](auto v) {
    if (ADB_UNLIKELY(v > static_cast<decltype(v)>(
                             std::numeric_limits<int64_t>::max()))) {
      throw VPackException{VPackException::NumberOutOfRange};
    }
    return static_cast<int64_t>(v);
  };
  auto t = type();
  switch (t) {
    case VPACK_INLINE_INT64:
      return absl::little_endian::ToHost(
          _data.longNumberMeta.data.intLittleEndian.val);
    case VPACK_INLINE_UINT64: {
      auto v = absl::little_endian::ToHost(
          _data.longNumberMeta.data.uintLittleEndian.val);
      return checkOverflow(v);
    }
    case VPACK_INLINE_DOUBLE: {
      auto h = absl::little_endian::ToHost(
          _data.longNumberMeta.data.uintLittleEndian.val);
      return checkOverflow(std::bit_cast<double>(h));
    }
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING: {
      auto s = slice(t);
      if (s.isNumber()) {
        return s.getNumber<int64_t>();
      }
      if (s.isBool()) {
        return s.getBool() ? 1 : 0;
      }
      if (s.isString()) {
        try {
          auto v = s.copyString();
          return static_cast<int64_t>(std::stoll(s.copyString()));
        } catch (...) {
        }
      } else if (s.isArray()) {
        if (auto const length = s.length(); length == 1) {
          // We can ignore destruction here, because doCopy is false
          bool mustDestroy;
          return at(0, mustDestroy, false).toInt64();
        }
      }
    } break;
    case RANGE: {
      if (range()->size() == 1) {
        // We can ignore destruction here, because doCopy is false
        bool mustDestroy;
        return at(0, mustDestroy, false).toInt64();
      }
    } break;
    default:
      break;
  }
  return 0;
}

bool AqlValue::toBoolean() const {
  auto t = type();
  switch (t) {
    case VPACK_INLINE_INT64:  // intentinally ignore endianess. 0 is always 0
      return _data.longNumberMeta.data.intLittleEndian.val != 0;
    case VPACK_INLINE_UINT64:  // intentinally ignore endianess. 0 is always 0
      return _data.longNumberMeta.data.uintLittleEndian.val != 0;
    case VPACK_INLINE_DOUBLE: {
      auto h = absl::little_endian::ToHost(
          _data.longNumberMeta.data.uintLittleEndian.val);
      double v = std::bit_cast<double>(h);
      return v != 0.0;
    }
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING: {
      auto s = slice(t);
      if (s.isBoolean()) {
        return s.getBoolean();
      }
      if (s.isNumber()) {
        return s.getNumber<double>() != 0.0;
      }
      if (s.isString()) {
        return s.getStringLength() > 0;
      }
      if (s.isArray() || s.isObject() || s.isCustom()) {
        // custom _id type is also true
        return true;
      }
      // all other cases, including Null and None
      return false;
    }
    case RANGE: {
      return true;
    }
  }
  return false;
}

AqlValue::MemoryOriginType AqlValue::memoryOriginType() const noexcept {
  TRI_ASSERT(type() == VPACK_MANAGED_SLICE);
  MemoryOriginType mot =
      static_cast<MemoryOriginType>(_data.managedSliceMeta.getOrigin());
  TRI_ASSERT(mot == MemoryOriginType::New || mot == MemoryOriginType::Malloc);
  return mot;
}

void AqlValue::setManagedSliceData(MemoryOriginType mot,
                                   velocypack::ValueLength length) {
  TRI_ASSERT(length > 0);
  TRI_ASSERT(mot == MemoryOriginType::New || mot == MemoryOriginType::Malloc);
  if (ADB_UNLIKELY(length > 0x0000ffffffffffffULL)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_OUT_OF_MEMORY,
                                   "invalid AqlValue length");
  }
  // assemble a 64 bit value with meta information for this AqlValue:
  // the first 6 bytes contain the byteSize
  // the next byte contains the memoryOriginType (0 = new[], 1 = malloc)
  // the last byte contains the AqlValueType (always VPACK_MANAGED_SLICE)
  _data.managedSliceMeta.lengthOrigin = length;
  if constexpr (basics::isLittleEndian()) {
    _data.managedSliceMeta.lengthOrigin <<= 16;
    _data.managedSliceMeta.lengthOrigin |= (static_cast<uint64_t>(mot) << 8);
    _data.managedSliceMeta.lengthOrigin |= AqlValueType::VPACK_MANAGED_SLICE;
  } else {
    _data.managedSliceMeta.lengthOrigin |= (static_cast<uint64_t>(mot) << 48);
    _data.managedSliceMeta.lengthOrigin |=
        (static_cast<uint64_t>(AqlValueType::VPACK_MANAGED_SLICE) << 56);
  }
  TRI_ASSERT(type() == VPACK_MANAGED_SLICE);
  TRI_ASSERT(memoryOriginType() == mot);
  TRI_ASSERT(memoryUsage() == length);
}

#ifdef USE_V8
v8::Handle<v8::Value> AqlValue::toV8(v8::Isolate* isolate,
                                     velocypack::Options const* options) const {
  auto context = TRI_IGETC;
  auto t = type();
  switch (t) {
    case RANGE: {
      auto const n = _data.rangeMeta.range->size();
      Range::throwIfTooBigForMaterialization(n);
      v8::Handle<v8::Array> result =
          v8::Array::New(isolate, static_cast<int>(n));

      for (uint32_t i = 0; i < n; ++i) {
        // is it safe to use a double here (precision loss)? No, it's not safe
        result
            ->Set(context, i,
                  v8::Number::New(isolate, static_cast<double>(
                                               _data.rangeMeta.range->at(i))))
            .FromMaybe(true);
        if (i % 1024 == 0) {
          if (V8PlatformFeature::isOutOfMemory(isolate)) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
      }
      return result;
    }
    default:
      return TRI_VPackToV8(isolate, slice(t), options);
  }
}
#endif

void AqlValue::toVelocyPack(velocypack::Options const* options,
                            velocypack::Builder& builder,
                            bool allowUnindexed) const {
  auto t = type();
  switch (t) {
    case VPACK_INLINE_INT64:
      // We handle it separatly because all small int/uint converted to int64
      // But we want to use vpack compaction for them
      builder.add(VPackValue{absl::little_endian::ToHost(
          _data.longNumberMeta.data.intLittleEndian.val)});
      break;
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_INLINE_UINT64:
    case VPACK_INLINE_DOUBLE:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING: {
      auto s = slice(t);
      builder.add(s);
    } break;
    case RANGE: {
      builder.openArray(/*unindexed*/ allowUnindexed);
      size_t const n = _data.rangeMeta.range->size();
      Range::throwIfTooBigForMaterialization(n);
      for (size_t i = 0; i < n; ++i) {
        builder.add(VPackValue(_data.rangeMeta.range->at(i)));
      }
      builder.close();
    } break;
  }
}

AqlValue AqlValue::materialize(velocypack::Options const* options,
                               bool& hasCopied) const {
  auto t = type();
  switch (t) {
    case RANGE: {
      VPackBuffer<uint8_t> buffer;
      VPackBuilder builder{buffer};
      toVelocyPack(options, builder, /*allowUnindexed*/ true);
      hasCopied = true;
      return AqlValue{std::move(buffer)};
    }
    default:
      hasCopied = false;
      return *this;
  }
}

AqlValue AqlValue::clone() const {
  auto t = type();
  switch (t) {
    case VPACK_MANAGED_SLICE:
      return AqlValue{VPackSlice{_data.managedSliceMeta.pointer},
                      _data.managedSliceMeta.getLength()};
    case VPACK_MANAGED_STRING:
      return AqlValue{_data.managedStringMeta.toSlice(),
                      _data.managedStringMeta.getLength()};
    case RANGE:
      return AqlValue{range()->_low, range()->_high};
    default:
      break;
  }
  return AqlValue{*this};
}

void AqlValue::destroy() noexcept {
  auto t = type();
  switch (t) {
    case VPACK_MANAGED_SLICE:
      if (auto const t = memoryOriginType(); t == MemoryOriginType::New) {
        delete[] _data.managedSliceMeta.pointer;
      } else {
        TRI_ASSERT(t == MemoryOriginType::Malloc);
        free(_data.managedSliceMeta.pointer);
      }
      break;
    case VPACK_MANAGED_STRING:
      delete _data.managedStringMeta.pointer;
      break;
    case RANGE:
      delete _data.rangeMeta.range;
      break;
    default:
      return;
  }

  erase();  // to prevent duplicate deletion
}

VPackSlice AqlValue::slice() const { return this->slice(type()); }

VPackSlice AqlValue::slice(AqlValueType type) const {
  switch (type) {
    case VPACK_INLINE_INT64:
    case VPACK_INLINE_UINT64:
    case VPACK_INLINE_DOUBLE:
      return VPackSlice{_data.longNumberMeta.data.slice.slice};
    case VPACK_INLINE:
      return VPackSlice{_data.inlineSliceMeta.slice};
    case VPACK_SLICE_POINTER:
      return VPackSlice{_data.slicePointerMeta.pointer};
    case VPACK_MANAGED_SLICE:
      return VPackSlice{_data.managedSliceMeta.pointer};
    case VPACK_MANAGED_STRING:
      return _data.managedStringMeta.toSlice();
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
}

int AqlValue::Compare(velocypack::Options const* options, AqlValue const& left,
                      AqlValue const& right, bool compareUtf8) {
  auto const leftType = left.type();
  auto const rightType = right.type();

  if ((leftType == RANGE) != (rightType == RANGE)) {
    // TODO implement this case more efficiently
    VPackBuilder leftBuilder;
    left.toVelocyPack(options, leftBuilder, /*allowUnindexed*/ true);

    VPackBuilder rightBuilder;
    right.toVelocyPack(options, rightBuilder, /*allowUnindexed*/ true);

    return basics::VelocyPackHelper::compare(
        leftBuilder.slice(), rightBuilder.slice(), compareUtf8, options);
  }
  // if we get here, types are equal or can be treated as being equal

  switch (leftType) {
    case VPACK_INLINE_INT64:
    case VPACK_INLINE_UINT64:
      // if right value type is also optimized inline we can optimize comparison
      if (leftType == rightType) {
        if (leftType == VPACK_INLINE_UINT64) {
          uint64_t l = static_cast<uint64_t>(left.toInt64());
          uint64_t r = static_cast<uint64_t>(right.toInt64());
          if (l == r) {
            return 0;
          }
          return (l < r ? -1 : 1);
        } else {
          int64_t l = left.toInt64();
          int64_t r = right.toInt64();
          if (l == r) {
            return 0;
          }
          return (l < r ? -1 : 1);
        }
        // intentional fallthrough to double comparison
      }
      [[fallthrough]];
    case VPACK_INLINE_DOUBLE:
      // here we could only compare doubles in case of right is also inlined
      // the same is done in VelocyPackHelper::compare for numbers. Equal types
      // are compared directly - unequal (or doubles) as doubles
      if (VPACK_INLINE_INT64 <= rightType && rightType <= VPACK_INLINE_DOUBLE) {
        double l = left.toDouble();
        double r = right.toDouble();
        if (l == r) {
          return 0;
        }
        return (l < r ? -1 : 1);
      }
      [[fallthrough]];
    case VPACK_INLINE:
    case VPACK_SLICE_POINTER:
    case VPACK_MANAGED_SLICE:
    case VPACK_MANAGED_STRING:
      return basics::VelocyPackHelper::compare(
          left.slice(leftType), right.slice(rightType), compareUtf8, options);
    case RANGE:
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
  return 0;
}

AqlValue::AqlValue() noexcept { erase(); }

AqlValue::AqlValue(DocumentData& data) noexcept {
  TRI_ASSERT(data);
  auto size = data->size();
  TRI_ASSERT(size >= 1);
  VPackSlice slice{reinterpret_cast<uint8_t const*>(data->data())};
  TRI_ASSERT(size == slice.byteSize());
  TRI_ASSERT(!slice.isExternal());
  if (size < sizeof(AqlValue)) {
    initFromSlice(slice, size);
  } else {
    setType(AqlValueType::VPACK_MANAGED_STRING);
    _data.managedStringMeta.pointer = data.release();
  }
}

AqlValue::AqlValue(uint8_t const* pointer) noexcept {
  TRI_ASSERT(pointer != nullptr);
  TRI_ASSERT(!VPackSlice{pointer}.isExternal());
  setPointer(pointer);
}

AqlValue::AqlValue(AqlValue const& other, void const* data) noexcept {
  TRI_ASSERT(data != nullptr);
  auto t = other.type();
  setType(t);
  switch (t) {
    case VPACK_MANAGED_SLICE:
      _data.managedSliceMeta.lengthOrigin =
          other._data.managedSliceMeta.lengthOrigin;
      _data.managedSliceMeta.pointer =
          const_cast<uint8_t*>(static_cast<uint8_t const*>(data));
      break;
    case VPACK_MANAGED_STRING:
      _data.managedStringMeta.pointer = static_cast<std::string const*>(data);
      break;
    case RANGE:
      _data.rangeMeta.range = static_cast<Range const*>(data);
      break;
    default:
      TRI_ASSERT(false);
      break;
  }
}

AqlValue::AqlValue(AqlValueHintNone) noexcept {
  setType(AqlValueType::VPACK_INLINE);
  _data.inlineSliceMeta.slice[0] = 0x00;  // none in VPack
}

AqlValue::AqlValue(AqlValueHintNull) noexcept {
  setType(AqlValueType::VPACK_INLINE);
  _data.inlineSliceMeta.slice[0] = 0x18;  // null in VPack
}

AqlValue::AqlValue(AqlValueHintBool v) noexcept {
  setType(AqlValueType::VPACK_INLINE);
  _data.inlineSliceMeta.slice[0] =
      v.value ? 0x1a : 0x19;  // true/false in VPack
}

AqlValue::AqlValue(AqlValueHintDouble v) noexcept {
  double value = v.value;
  if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL ||
      value == -HUGE_VAL) {
    setType(AqlValueType::VPACK_INLINE);
    _data.inlineSliceMeta.slice[0] = 0x18;  // null
  } else {
    // a "real" double
    setType(AqlValueType::VPACK_INLINE_DOUBLE);
    _data.longNumberMeta.data.slice.slice[0] = 0x1b;
    if (ADB_UNLIKELY(value == -0.0)) {  // unify +0.0 and -0.0 to +0.0
      value = 0.0;
    }
    uint64_t h = std::bit_cast<uint64_t>(value);
    _data.longNumberMeta.data.uintLittleEndian.val =
        absl::little_endian::FromHost(h);
  }
}

AqlValue::AqlValue(AqlValueHintInt v) noexcept { initFromInt(v.value); }

AqlValue::AqlValue(AqlValueHintUInt v) noexcept { initFromUint(v.value); }

AqlValue::AqlValue(std::string_view s) {
  TRI_ASSERT(s.data() != nullptr);  // not necessary, can be removed
  if (s.size() == 0) {
    // empty string
    _data.inlineSliceMeta.slice[0] = 0x40;
    setType(AqlValueType::VPACK_INLINE);
  } else if (s.size() < sizeof(AqlValue) - 1) {
    // short string... can store it inline
    _data.inlineSliceMeta.slice[0] = static_cast<uint8_t>(0x40 + s.size());
    memcpy(_data.inlineSliceMeta.slice + 1, s.data(), s.size());
    setType(AqlValueType::VPACK_INLINE);
  } else if (s.size() <= 126) {
    // short string... cannot store inline, but we don't need to
    // create a full-featured Builder object here
    setManagedSliceData(MemoryOriginType::New, s.size() + 1);
    _data.managedSliceMeta.pointer = new uint8_t[s.size() + 1];
    _data.managedSliceMeta.pointer[0] = static_cast<uint8_t>(0x40U + s.size());
    memcpy(_data.managedSliceMeta.pointer + 1, s.data(), s.size());
  } else {
    // long string
    // create a big enough uint8_t buffer
    size_t byteSize = s.size() + 9;
    setManagedSliceData(MemoryOriginType::New, byteSize);
    _data.managedSliceMeta.pointer = new uint8_t[byteSize];
    _data.managedSliceMeta.pointer[0] = static_cast<uint8_t>(0xbfU);
    auto v = absl::little_endian::FromHost64(s.size());
    memcpy(&_data.managedSliceMeta.pointer[1], &v, sizeof(v));
    memcpy(&_data.managedSliceMeta.pointer[9], s.data(), s.size());
  }
}

AqlValue::AqlValue(AqlValueHintEmptyArray) noexcept {
  setType(AqlValueType::VPACK_INLINE);
  _data.inlineSliceMeta.slice[0] = 0x01;  // empty array in VPack
}

AqlValue::AqlValue(AqlValueHintEmptyObject) noexcept {
  setType(AqlValueType::VPACK_INLINE);
  _data.inlineSliceMeta.slice[0] = 0x0a;  // empty object in VPack
}

AqlValue::AqlValue(velocypack::Buffer<uint8_t>&& buffer) {
  auto size = buffer.size();
  TRI_ASSERT(size >= 1);
  VPackSlice slice{buffer.data()};
  TRI_ASSERT(size == slice.byteSize());
  TRI_ASSERT(!slice.isExternal());
  if (size < sizeof(AqlValue)) {
    // Use inline value
    initFromSlice(slice, size);
    buffer.clear();  // for move semantics
  } else {
    // Use managed slice
    if (buffer.usesLocalMemory()) {
      setManagedSliceData(MemoryOriginType::New, size);
      _data.managedSliceMeta.pointer = new uint8_t[size];
      memcpy(_data.managedSliceMeta.pointer, buffer.data(), size);
      buffer.clear();  // for move semantics
    } else {
      // steal dynamic memory from the Buffer
      setManagedSliceData(MemoryOriginType::Malloc, size);
      _data.managedSliceMeta.pointer = buffer.steal();
    }
  }
}

AqlValue::AqlValue(AqlValueHintSliceNoCopy v) noexcept
    : AqlValue{v.slice.start()} {}

AqlValue::AqlValue(AqlValueHintSliceCopy v) : AqlValue{v.slice} {}

AqlValue::AqlValue(VPackSlice slice) { initFromSlice(slice, slice.byteSize()); }

AqlValue::AqlValue(VPackSlice slice, velocypack::ValueLength length) {
  initFromSlice(slice, length);
}

AqlValue::AqlValue(int64_t low, int64_t high) {
  _data.rangeMeta.range = new Range(low, high);
  setType(AqlValueType::RANGE);
}

bool AqlValue::requiresDestruction() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_SLICE_POINTER:
    case VPACK_INLINE:
    case VPACK_INLINE_INT64:
    case VPACK_INLINE_UINT64:
    case VPACK_INLINE_DOUBLE:
      return false;
    default:
      return true;
  }
}

bool AqlValue::isEmpty() const noexcept {
  return _data.aqlValueType == VPACK_INLINE &&
         _data.inlineSliceMeta.slice[0] == '\x00';
}

bool AqlValue::isPointer() const noexcept {
  return type() == VPACK_SLICE_POINTER;
}

bool AqlValue::isRange() const noexcept { return type() == RANGE; }

Range const* AqlValue::range() const {
  TRI_ASSERT(isRange());
  return _data.rangeMeta.range;
}

void AqlValue::erase() noexcept {
  // VPACK_INLINE must have a value of 0, and VPackSlice::None must be equal
  // to a NUL byte too
  static_assert(AqlValueType::VPACK_INLINE == 0,
                "invalid value for VPACK_INLINE");

  // construct a slice of type None
  // we will simply zero-initialize the two 64 bit words
  _data.words[0] = 0;
  _data.words[1] = 0;

  TRI_ASSERT(isEmpty());
}

size_t AqlValue::memoryUsage() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_MANAGED_SLICE:
      return _data.managedSliceMeta.getLength();
    case VPACK_MANAGED_STRING:
      // It should be length, because in case of clone
      // VPACK_MANAGED_SLICE will be created
      return _data.managedStringMeta.getLength();
    case RANGE:
      return sizeof(Range);
    default:
      return 0;
  }
}

void AqlValue::initFromSlice(VPackSlice slice, VPackValueLength length) {
  TRI_ASSERT(!slice.isExternal());
  TRI_ASSERT(length > 0);
  TRI_ASSERT(slice.byteSize() == length);
  if (length > sizeof(_data.inlineSliceMeta.slice)) {
    // Use managed slice
    setManagedSliceData(MemoryOriginType::New, length);
    _data.managedSliceMeta.pointer = new uint8_t[length];
    memcpy(_data.managedSliceMeta.pointer, slice.begin(), length);
    return;
  }
  if (length <= 9) {
    if (slice.isDouble()) {
      TRI_ASSERT(length == 9);
      setType(AqlValueType::VPACK_INLINE_DOUBLE);
      memcpy(_data.longNumberMeta.data.slice.slice, slice.begin(), 9);
      return;
    }
    if (slice.isUInt()) {
      return initFromUint(slice.getUIntUnchecked());
    }
    if (slice.isInt() || slice.isSmallInt()) {
      return initFromInt(slice.getIntUnchecked());
    }
  }
  // Use inline value
  memcpy(_data.inlineSliceMeta.slice, slice.begin(),
         static_cast<size_t>(length));
  setType(AqlValueType::VPACK_INLINE);
}

void AqlValue::initFromUint(uint64_t v) {
  // We want int instead of uint because it allows us to use it without checks
  if (v < (std::uint64_t{1} << std::uint64_t{63})) {
    return initFromInt(static_cast<int64_t>(v));
  }
  setType(AqlValueType::VPACK_INLINE_UINT64);
  // always store as 8 byte slice as we need fully aligned value
  _data.longNumberMeta.data.slice.slice[0] = 0x27U + 8;
  auto x = absl::little_endian::FromHost(v);
  _data.longNumberMeta.data.uintLittleEndian.val = x;
}

void AqlValue::initFromInt(int64_t v) {
  setType(AqlValueType::VPACK_INLINE_INT64);
  // always store as 8 byte slice as we need fully aligned value
  _data.longNumberMeta.data.slice.slice[0] = 0x1fU + 8;
  auto x = absl::little_endian::FromHost(v);
  _data.longNumberMeta.data.intLittleEndian.val = x;
}

void AqlValue::setType(AqlValue::AqlValueType type) noexcept {
  _data.aqlValueType = type;
}

void const* AqlValue::data() const noexcept {
  auto t = type();
  switch (t) {
    case VPACK_MANAGED_SLICE:
      return _data.managedSliceMeta.pointer;
    case VPACK_MANAGED_STRING:
      return _data.managedStringMeta.pointer;
    case RANGE:
      return _data.rangeMeta.range;
    default:
      TRI_ASSERT(false);
      return nullptr;
  }
}

size_t std::hash<AqlValue>::operator()(AqlValue const& x) const noexcept {
  auto t = x.type();
  switch (t) {
    case AqlValue::VPACK_INLINE:
      return static_cast<size_t>(
          VPackSlice(x._data.inlineSliceMeta.slice).volatileHash());
    case AqlValue::VPACK_INLINE_INT64:
    case AqlValue::VPACK_INLINE_UINT64:
    case AqlValue::VPACK_INLINE_DOUBLE:
      return static_cast<size_t>(
          VPackSlice(x._data.longNumberMeta.data.slice.slice).volatileHash());
      // TODO(MBkkt) these hashes have bad distribution
    case AqlValue::VPACK_SLICE_POINTER:
      return std::hash<void const*>()(x._data.slicePointerMeta.pointer);
    case AqlValue::VPACK_MANAGED_SLICE:
      return std::hash<void const*>()(x._data.managedSliceMeta.pointer);
    case AqlValue::VPACK_MANAGED_STRING:
      return std::hash<void const*>()(x._data.managedStringMeta.pointer);
    case AqlValue::RANGE:
      return std::hash<void const*>()(x._data.rangeMeta.range);
  }
  return 0;
}

bool std::equal_to<AqlValue>::operator()(AqlValue const& a,
                                         AqlValue const& b) const noexcept {
  // TODO(MBkkt) can be just compare two uint64_t?
  auto t = a.type();
  if (t != b.type()) {
    return false;
  }
  switch (t) {
    case AqlValue::VPACK_INLINE:
      return VPackSlice(a._data.inlineSliceMeta.slice)
          .binaryEquals(VPackSlice(b._data.inlineSliceMeta.slice));
    case AqlValue::VPACK_INLINE_INT64:
    case AqlValue::VPACK_INLINE_UINT64:
    case AqlValue::VPACK_INLINE_DOUBLE:
      // equal is equal. sign/endianess does not matter
      return a._data.longNumberMeta.data.intLittleEndian.val ==
             b._data.longNumberMeta.data.intLittleEndian.val;
    case AqlValue::VPACK_SLICE_POINTER:
      return a._data.slicePointerMeta.pointer ==
             b._data.slicePointerMeta.pointer;
    case AqlValue::VPACK_MANAGED_SLICE:
      return a._data.managedSliceMeta.pointer ==
             b._data.managedSliceMeta.pointer;
    case AqlValue::VPACK_MANAGED_STRING:
      return a._data.managedStringMeta.pointer ==
             b._data.managedStringMeta.pointer;
    case AqlValue::RANGE:
      return a._data.rangeMeta.range == b._data.rangeMeta.range;
  }
  return false;
}
