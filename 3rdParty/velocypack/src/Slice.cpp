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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <ostream>

#include "velocypack/velocypack-common.h"
#include "velocypack/AttributeTranslator.h"
#include "velocypack/Builder.h"
#include "velocypack/Dumper.h"
#include "velocypack/HexDump.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/Sink.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

using namespace arangodb::velocypack;

namespace {

// maximum values for integers of different byte sizes
constexpr int64_t maxValues[] = {
  128, 32768, 8388608, 2147483648, 549755813888, 140737488355328, 36028797018963968
};

} // namespace
  
uint8_t const Slice::noneSliceData[] = { 0x00 };
uint8_t const Slice::illegalSliceData[] = { 0x17 };
uint8_t const Slice::nullSliceData[] = { 0x18 };
uint8_t const Slice::falseSliceData[] = { 0x19 };
uint8_t const Slice::trueSliceData[] = { 0x1a };
uint8_t const Slice::zeroSliceData[] = { 0x30 };
uint8_t const Slice::emptyStringSliceData[] = { 0x40 };
uint8_t const Slice::emptyArraySliceData[] = { 0x01 };
uint8_t const Slice::emptyObjectSliceData[] = { 0x0a };
uint8_t const Slice::minKeySliceData[] = { 0x1e };
uint8_t const Slice::maxKeySliceData[] = { 0x1f };

// translates an integer key into a string
Slice Slice::translate() const {
  if (VELOCYPACK_UNLIKELY(!isSmallInt() && !isUInt())) {
    throw Exception(Exception::InvalidValueType,
                    "Cannot translate key of this type");
  }
  if (Options::Defaults.attributeTranslator == nullptr) {
    throw Exception(Exception::NeedAttributeTranslator);
  }
  return translateUnchecked();
}

// return the value for a UInt object, without checks!
// returns 0 for invalid values/types
uint64_t Slice::getUIntUnchecked() const noexcept {
  uint8_t const h = head();
  if (h >= 0x28 && h <= 0x2f) {
    // UInt
    return readIntegerNonEmpty<uint64_t>(start() + 1, h - 0x27);
  }

  if (h >= 0x30 && h <= 0x39) {
    // Smallint >= 0
    return static_cast<uint64_t>(h - 0x30);
  }
  return 0;
}

// return the value for a SmallInt object
int64_t Slice::getSmallIntUnchecked() const noexcept {
  uint8_t const h = head();

  if (h >= 0x30 && h <= 0x39) {
    // Smallint >= 0
    return static_cast<int64_t>(h - 0x30);
  }

  if (h >= 0x3a && h <= 0x3f) {
    // Smallint < 0
    return static_cast<int64_t>(h - 0x3a) - 6;
  }

  if ((h >= 0x20 && h <= 0x27) || (h >= 0x28 && h <= 0x2f)) {
    // Int and UInt
    // we'll leave it to the compiler to detect the two ranges above are
    // adjacent
    return getIntUnchecked();
  }

  return 0;
}

// translates an integer key into a string, without checks
Slice Slice::translateUnchecked() const {
  uint8_t const* result = Options::Defaults.attributeTranslator->translate(getUIntUnchecked());
  if (VELOCYPACK_LIKELY(result != nullptr)) {
    return Slice(result);
  }
  return Slice();
}

std::string Slice::toHex() const {
  HexDump dump(this);
  return dump.toString(); 
}

std::string Slice::toJson(Options const* options) const {
  std::string buffer;
  StringSink sink(&buffer);
  toJson(&sink, options);
  return buffer;
}

std::string& Slice::toJson(std::string& out, Options const* options) const {
  StringSink sink(&out);
  toJson(&sink, options);
  return out;
}

void Slice::toJson(Sink* sink, Options const* options) const {
  Dumper dumper(sink, options);
  dumper.dump(this);
}

std::string Slice::toString(Options const* options) const {
  if (isString()) {
    return copyString();
  }

  // copy options and set prettyPrint in copy
  Options prettyOptions = *options;
  prettyOptions.prettyPrint = true;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper::dump(this, &sink, &prettyOptions);
  return buffer;
}

std::string Slice::hexType() const { return HexDump::toHex(head()); }
  
uint64_t Slice::normalizedHash(uint64_t seed) const {
  uint64_t value;

  if (isNumber()) {
    // upcast integer values to double
    double v = getNumericValue<double>();
    value = VELOCYPACK_HASH(&v, sizeof(v), seed);
  } else if (isArray()) {
    // normalize arrays by hashing array length and iterating
    // over all array members
    ArrayIterator it(*this);
    uint64_t const n = it.size() ^ 0xba5bedf00d;
    value = VELOCYPACK_HASH(&n, sizeof(n), seed);
    while (it.valid()) {
      value ^= it.value().normalizedHash(value);
      it.next();
    }
  } else if (isObject()) {
    // normalize objects by hashing object length and iterating
    // over all object members
    ObjectIterator it(*this, true);
    uint64_t const n = it.size() ^ 0xf00ba44ba5;
    uint64_t seed2 = VELOCYPACK_HASH(&n, sizeof(n), seed);
    value = seed2;
    while (it.valid()) {
      auto current = (*it);
      uint64_t seed3 = current.key.normalizedHash(seed2);
      value ^= seed3;
      value ^= current.value.normalizedHash(seed3);
      it.next();
    }
  } else {
    // fall back to regular hash function
    value = hash(seed);
  }

  return value;
}

uint32_t Slice::normalizedHash32(uint32_t seed) const {
  uint32_t value;

  if (isNumber()) {
    // upcast integer values to double
    double v = getNumericValue<double>();
    value = VELOCYPACK_HASH32(&v, sizeof(v), seed);
  } else if (isArray()) {
    // normalize arrays by hashing array length and iterating
    // over all array members
    ArrayIterator it(*this);
    uint64_t const n = it.size() ^ 0xba5bedf00d;
    value = VELOCYPACK_HASH32(&n, sizeof(n), seed);
    while (it.valid()) {
      value ^= it.value().normalizedHash32(value);
      it.next();
    }
  } else if (isObject()) {
    // normalize objects by hashing object length and iterating
    // over all object members
    ObjectIterator it(*this, true);
    uint32_t const n = static_cast<uint32_t>(it.size() ^ 0xf00ba44ba5);
    uint32_t seed2 = VELOCYPACK_HASH32(&n, sizeof(n), seed);
    value = seed2;
    while (it.valid()) {
      auto current = (*it);
      uint32_t seed3 = current.key.normalizedHash32(seed2);
      value ^= seed3;
      value ^= current.value.normalizedHash32(seed3);
      it.next();
    }
  } else {
    // fall back to regular hash function
    value = hash32(seed);
  }

  return value;
}

// look for the specified attribute inside an Object
// returns a Slice(ValueType::None) if not found
Slice Slice::get(StringRef const& attribute) const {
  if (VELOCYPACK_UNLIKELY(!isObject())) {
    throw Exception(Exception::InvalidValueType, "Expecting Object");
  }

  auto const h = head();
  if (h == 0x0a) {
    // special case, empty object
    return Slice();
  }

  if (h == 0x14) {
    // compact Object
    return getFromCompactObject(attribute);
  }

  ValueLength const offsetSize = indexEntrySize(h);
  VELOCYPACK_ASSERT(offsetSize > 0);
  ValueLength end = readIntegerNonEmpty<ValueLength>(start() + 1, offsetSize);

  // read number of items
  ValueLength n;
  ValueLength ieBase;
  if (offsetSize < 8) {
    n = readIntegerNonEmpty<ValueLength>(start() + 1 + offsetSize, offsetSize);
    ieBase = end - n * offsetSize;
  } else {
    n = readIntegerNonEmpty<ValueLength>(start() + end - offsetSize, offsetSize);
    ieBase = end - n * offsetSize - offsetSize;
  }

  if (n == 1) {
    // Just one attribute, there is no index table!
    Slice key(start() + findDataOffset(h));

    if (key.isString()) {
      if (key.isEqualStringUnchecked(attribute)) {
        return Slice(key.start() + key.byteSize());
      }
      // fall through to returning None Slice below
    } else if (key.isSmallInt() || key.isUInt()) {
      // translate key
      if (Options::Defaults.attributeTranslator == nullptr) {
        throw Exception(Exception::NeedAttributeTranslator);
      }
      if (key.translateUnchecked().isEqualString(attribute)) {
        return Slice(key.start() + key.byteSize());
      }
    }

    // no match or invalid key type
    return Slice();
  }

  // only use binary search for attributes if we have at least this many entries
  // otherwise we'll always use the linear search
  constexpr ValueLength SortedSearchEntriesThreshold = 4;

  if (n >= SortedSearchEntriesThreshold && (h >= 0x0b && h <= 0x0e)) {
    switch (offsetSize) {
      case 1:
        return searchObjectKeyBinary<1>(attribute, ieBase, n);
      case 2:
        return searchObjectKeyBinary<2>(attribute, ieBase, n);
      case 4:
        return searchObjectKeyBinary<4>(attribute, ieBase, n);
      case 8:
        return searchObjectKeyBinary<8>(attribute, ieBase, n);
      default: {}
    }
  }

  return searchObjectKeyLinear(attribute, ieBase, offsetSize, n);
}

// return the value for an Int object
int64_t Slice::getIntUnchecked() const noexcept {
  uint8_t const h = head();

  if (h >= 0x20 && h <= 0x27) {
    // Int  T
    uint64_t v = readIntegerNonEmpty<uint64_t>(start() + 1, h - 0x1f);
    if (h == 0x27) {
      return toInt64(v);
    } else {
      int64_t vv = static_cast<int64_t>(v);
      int64_t shift = ::maxValues[h - 0x20];
      return vv < shift ? vv : vv - (shift << 1);
    }
  }

  // SmallInt
  VELOCYPACK_ASSERT(h >= 0x30 && h <= 0x3f);
  return getSmallIntUnchecked();
}

// return the value for an Int object
int64_t Slice::getInt() const {
  uint8_t const h = head();

  if (h >= 0x20 && h <= 0x27) {
    // Int  T
    uint64_t v = readIntegerNonEmpty<uint64_t>(start() + 1, h - 0x1f);
    if (h == 0x27) {
      return toInt64(v);
    } else {
      int64_t vv = static_cast<int64_t>(v);
      int64_t shift = ::maxValues[h - 0x20];
      return vv < shift ? vv : vv - (shift << 1);
    }
  }

  if (h >= 0x28 && h <= 0x2f) {
    // UInt
    uint64_t v = getUIntUnchecked();
    if (v > static_cast<uint64_t>(INT64_MAX)) {
      throw Exception(Exception::NumberOutOfRange);
    }
    return static_cast<int64_t>(v);
  }

  if (h >= 0x30 && h <= 0x3f) {
    // SmallInt
    return getSmallIntUnchecked();
  }

  throw Exception(Exception::InvalidValueType, "Expecting type Int");
}

// return the value for a UInt object
uint64_t Slice::getUInt() const {
  uint8_t const h = head();
  if (h == 0x28) {
    // single byte integer
    return readIntegerFixed<uint64_t, 1>(start() + 1);
  }

  if (h >= 0x29 && h <= 0x2f) {
    // UInt
    return readIntegerNonEmpty<uint64_t>(start() + 1, h - 0x27);
  }

  if (h >= 0x20 && h <= 0x27) {
    // Int
    int64_t v = getInt();
    if (v < 0) {
      throw Exception(Exception::NumberOutOfRange);
    }
    return static_cast<int64_t>(v);
  }

  if (h >= 0x30 && h <= 0x39) {
    // Smallint >= 0
    return static_cast<uint64_t>(h - 0x30);
  }

  if (h >= 0x3a && h <= 0x3f) {
    // Smallint < 0
    throw Exception(Exception::NumberOutOfRange);
  }

  throw Exception(Exception::InvalidValueType, "Expecting type UInt");
}

// return the value for a SmallInt object
int64_t Slice::getSmallInt() const {
  uint8_t const h = head();

  if (h >= 0x30 && h <= 0x39) {
    // Smallint >= 0
    return static_cast<int64_t>(h - 0x30);
  }

  if (h >= 0x3a && h <= 0x3f) {
    // Smallint < 0
    return static_cast<int64_t>(h - 0x3a) - 6;
  }

  if ((h >= 0x20 && h <= 0x27) || (h >= 0x28 && h <= 0x2f)) {
    // Int and UInt
    // we'll leave it to the compiler to detect the two ranges above are
    // adjacent
    return getInt();
  }

  throw Exception(Exception::InvalidValueType, "Expecting type SmallInt");
}

int Slice::compareString(StringRef const& value) const {
  std::size_t const length = value.size();
  ValueLength keyLength;
  char const* k = getString(keyLength);
  std::size_t const compareLength =
      (std::min)(static_cast<std::size_t>(keyLength), length);
  int res = std::memcmp(k, value.data(), compareLength);

  if (res == 0) {
    return static_cast<int>(keyLength - length);
  }
  return res;
}

int Slice::compareStringUnchecked(StringRef const& value) const noexcept {
  std::size_t const length = value.size();
  ValueLength keyLength;
  char const* k = getStringUnchecked(keyLength);
  std::size_t const compareLength =
      (std::min)(static_cast<std::size_t>(keyLength), length);
  int res = std::memcmp(k, value.data(), compareLength);

  if (res == 0) {
    return static_cast<int>(keyLength - length);
  }
  return res;
}

bool Slice::isEqualString(StringRef const& attribute) const {
  ValueLength keyLength;
  char const* k = getString(keyLength);
  return (static_cast<std::size_t>(keyLength) == attribute.size()) &&
          (std::memcmp(k, attribute.data(), attribute.size()) == 0);
}

bool Slice::isEqualStringUnchecked(StringRef const& attribute) const noexcept {
  ValueLength keyLength;
  char const* k = getStringUnchecked(keyLength);
  return (static_cast<std::size_t>(keyLength) == attribute.size()) &&
          (std::memcmp(k, attribute.data(), attribute.size()) == 0);
}

Slice Slice::getFromCompactObject(StringRef const& attribute) const {
  ObjectIterator it(*this);
  while (it.valid()) {
    Slice key = it.key(false);
    if (key.makeKey().isEqualString(attribute)) {
      return Slice(key.start() + key.byteSize());
    }

    it.next();
  }
  // not found
  return Slice();
}

// get the offset for the nth member from an Array or Object type
ValueLength Slice::getNthOffset(ValueLength index) const {
  VELOCYPACK_ASSERT(isArray() || isObject());

  auto const h = head();

  if (h == 0x13 || h == 0x14) {
    // compact Array or Object
    return getNthOffsetFromCompact(index);
  }
  
  if (VELOCYPACK_UNLIKELY(h == 0x01 || h == 0x0a)) {
    // special case: empty Array or empty Object
    throw Exception(Exception::IndexOutOfBounds);
  }

  ValueLength const offsetSize = indexEntrySize(h);
  ValueLength end = readIntegerNonEmpty<ValueLength>(start() + 1, offsetSize);

  ValueLength dataOffset = 0;

  // find the number of items
  ValueLength n;
  if (h <= 0x05) {  // No offset table or length, need to compute:
    VELOCYPACK_ASSERT(h != 0x00 && h != 0x01);
    dataOffset = findDataOffset(h);
    Slice first(start() + dataOffset);
    ValueLength s = first.byteSize();
    if (VELOCYPACK_UNLIKELY(s == 0)) {
      throw Exception(Exception::InternalError, "Invalid data for compact object");
    }
    n = (end - dataOffset) / s;
  } else if (offsetSize < 8) {
    n = readIntegerNonEmpty<ValueLength>(start() + 1 + offsetSize, offsetSize);
  } else {
    n = readIntegerNonEmpty<ValueLength>(start() + end - offsetSize, offsetSize);
  }

  if (index >= n) {
    throw Exception(Exception::IndexOutOfBounds);
  }

  // empty array case was already covered
  VELOCYPACK_ASSERT(n > 0);

  if (h <= 0x05 || n == 1) {
    // no index table, but all array items have the same length
    // now fetch first item and determine its length
    if (dataOffset == 0) {
      VELOCYPACK_ASSERT(h != 0x00 && h != 0x01);
      dataOffset = findDataOffset(h);
    }
    return dataOffset + index * Slice(start() + dataOffset).byteSize();
  }

  ValueLength const ieBase =
      end - n * offsetSize + index * offsetSize - (offsetSize == 8 ? 8 : 0);
  return readIntegerNonEmpty<ValueLength>(start() + ieBase, offsetSize);
}

// extract the nth member from an Array
Slice Slice::getNth(ValueLength index) const {
  VELOCYPACK_ASSERT(isArray());

  return Slice(start() + getNthOffset(index));
}

// extract the nth member from an Object
Slice Slice::getNthKey(ValueLength index, bool translate) const {
  VELOCYPACK_ASSERT(type() == ValueType::Object);

  Slice s(start() + getNthOffset(index));

  if (translate) {
    return s.makeKey();
  }

  return s;
}

Slice Slice::makeKey() const {
  if (isString()) {
    return *this;
  }
  if (isSmallInt() || isUInt()) {
    if (VELOCYPACK_UNLIKELY(Options::Defaults.attributeTranslator == nullptr)) {
      throw Exception(Exception::NeedAttributeTranslator);
    }
    return translateUnchecked();
  }

  throw Exception(Exception::InvalidValueType,
                  "Cannot translate key of this type");
}

// get the offset for the nth member from a compact Array or Object type
ValueLength Slice::getNthOffsetFromCompact(ValueLength index) const {
  auto const h = head();
  VELOCYPACK_ASSERT(h == 0x13 || h == 0x14);

  ValueLength end = readVariableValueLength<false>(start() + 1);
  ValueLength n = readVariableValueLength<true>(start() + end - 1);
  if (VELOCYPACK_UNLIKELY(index >= n)) {
    throw Exception(Exception::IndexOutOfBounds);
  }

  ValueLength offset = 1 + getVariableValueLength(end);
  ValueLength current = 0;
  while (current != index) {
    uint8_t const* s = start() + offset;
    offset += Slice(s).byteSize();
    if (h == 0x14) {
      offset += Slice(start() + offset).byteSize();
    }
    ++current;
  }
  return offset;
}

// perform a linear search for the specified attribute inside an Object
Slice Slice::searchObjectKeyLinear(StringRef const& attribute,
                                   ValueLength ieBase, ValueLength offsetSize,
                                   ValueLength n) const {
  bool const useTranslator = (Options::Defaults.attributeTranslator != nullptr);

  for (ValueLength index = 0; index < n; ++index) {
    ValueLength offset = ieBase + index * offsetSize;
    Slice key(start() + readIntegerNonEmpty<ValueLength>(start() + offset, offsetSize));

    if (key.isString()) {
      if (!key.isEqualStringUnchecked(attribute)) {
        continue;
      } 
    } else if (key.isSmallInt() || key.isUInt()) {
      // translate key
      if (VELOCYPACK_UNLIKELY(!useTranslator)) {
        // no attribute translator
        throw Exception(Exception::NeedAttributeTranslator);
      }
      if (!key.translateUnchecked().isEqualString(attribute)) {
        continue;
      }
    } else {
      // invalid key type
      return Slice();
    }

    // key is identical. now return value
    return Slice(key.start() + key.byteSize());
  }

  // nothing found
  return Slice();
}

// perform a binary search for the specified attribute inside an Object
template<ValueLength offsetSize>
Slice Slice::searchObjectKeyBinary(StringRef const& attribute,
                                   ValueLength ieBase,
                                   ValueLength n) const {
  bool const useTranslator = (Options::Defaults.attributeTranslator != nullptr);
  VELOCYPACK_ASSERT(n > 0);

  int64_t l = 0;
  int64_t r = static_cast<int64_t>(n) - 1;
  int64_t index = r / 2;

  do {
    ValueLength offset = ieBase + index * offsetSize;
    Slice key(start() + readIntegerFixed<ValueLength, offsetSize>(start() + offset));

    int res;
    if (key.isString()) {
      res = key.compareStringUnchecked(attribute.data(), attribute.size());
    } else {
      VELOCYPACK_ASSERT(key.isSmallInt() || key.isUInt());
      // translate key
      if (VELOCYPACK_UNLIKELY(!useTranslator)) {
        // no attribute translator
        throw Exception(Exception::NeedAttributeTranslator);
      }
      res = key.translateUnchecked().compareString(attribute);
    }

    if (res > 0) {
      r = index - 1;
    } else if (res == 0) {
      // found. now return a Slice pointing at the value
      return Slice(key.start() + key.byteSize());
    } else {
      l = index + 1;
    }
    
    // determine new midpoint
    index = l + ((r - l) / 2);
  } while (r >= l);
  
  // not found
  return Slice();
}

// template instanciations for searchObjectKeyBinary
template Slice Slice::searchObjectKeyBinary<1>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;
template Slice Slice::searchObjectKeyBinary<2>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;
template Slice Slice::searchObjectKeyBinary<4>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;
template Slice Slice::searchObjectKeyBinary<8>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;

std::ostream& operator<<(std::ostream& stream, Slice const* slice) {
  stream << "[Slice " << valueTypeName(slice->type()) << " ("
         << slice->hexType() << "), byteSize: " << slice->byteSize() << "]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, Slice const& slice) {
  return operator<<(stream, &slice);
}

static_assert(sizeof(arangodb::velocypack::Slice) ==
              sizeof(void*), "Slice has an unexpected size");
