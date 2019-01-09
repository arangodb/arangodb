////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <ostream>

#include "velocypack/velocypack-common.h"
#include "velocypack/AttributeTranslator.h"
#include "velocypack/Builder.h"
#include "velocypack/Dumper.h"
#include "velocypack/HexDump.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

using namespace arangodb::velocypack;

// creates a Slice from Json and adds it to a scope
Slice Slice::fromJson(SliceScope& scope, std::string const& json,
                      Options const* options) {
  Parser parser(options);
  parser.parse(json);

  Builder const& b = parser.builder();  // don't copy Builder contents here
  return scope.add(b.start(), b.size());
}

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
    return readIntegerNonEmpty<uint64_t>(_start + 1, h - 0x27);
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
  if (result != nullptr) {
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
  Dumper dumper(&sink, options);
  dumper.dump(this);
  return buffer;
}

std::string Slice::toString(Options const* options) const {
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
  ValueLength end = readIntegerNonEmpty<ValueLength>(_start + 1, offsetSize);

  // read number of items
  ValueLength n;
  ValueLength ieBase;
  if (offsetSize < 8) {
    n = readIntegerNonEmpty<ValueLength>(_start + 1 + offsetSize, offsetSize);
    ieBase = end - n * offsetSize;
  } else {
    n = readIntegerNonEmpty<ValueLength>(_start + end - offsetSize, offsetSize);
    ieBase = end - n * offsetSize - offsetSize;
  }

  if (n == 1) {
    // Just one attribute, there is no index table!
    Slice key = Slice(_start + findDataOffset(h));

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
    uint64_t v = readIntegerNonEmpty<uint64_t>(_start + 1, h - 0x1f);
    if (h == 0x27) {
      return toInt64(v);
    } else {
      int64_t vv = static_cast<int64_t>(v);
      int64_t shift = 1LL << ((h - 0x1f) * 8 - 1);
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
    uint64_t v = readIntegerNonEmpty<uint64_t>(_start + 1, h - 0x1f);
    if (h == 0x27) {
      return toInt64(v);
    } else {
      int64_t vv = static_cast<int64_t>(v);
      int64_t shift = 1LL << ((h - 0x1f) * 8 - 1);
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
    return readIntegerFixed<uint64_t, 1>(_start + 1);
  }

  if (h >= 0x29 && h <= 0x2f) {
    // UInt
    return readIntegerNonEmpty<uint64_t>(_start + 1, h - 0x27);
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
  size_t const length = value.size();
  ValueLength keyLength;
  char const* k = getString(keyLength);
  size_t const compareLength =
      (std::min)(static_cast<size_t>(keyLength), length);
  int res = memcmp(k, value.data(), compareLength);

  if (res == 0) {
    if (keyLength != length) {
      return (keyLength > length) ? 1 : -1;
    }
  }
  return res;
}

int Slice::compareStringUnchecked(StringRef const& value) const noexcept {
  size_t const length = value.size();
  ValueLength keyLength;
  char const* k = getStringUnchecked(keyLength);
  size_t const compareLength =
      (std::min)(static_cast<size_t>(keyLength), length);
  int res = memcmp(k, value.data(), compareLength);

  if (res == 0) {
    if (keyLength != length) {
      return (keyLength > length) ? 1 : -1;
    }
  }
  return res;
}

bool Slice::isEqualString(StringRef const& attribute) const {
  ValueLength keyLength;
  char const* k = getString(keyLength);
  if (static_cast<size_t>(keyLength) != attribute.size()) {
    return false;
  }
  return (memcmp(k, attribute.data(), attribute.size()) == 0);
}

bool Slice::isEqualStringUnchecked(StringRef const& attribute) const noexcept {
  ValueLength keyLength;
  char const* k = getStringUnchecked(keyLength);
  if (static_cast<size_t>(keyLength) != attribute.size()) {
    return false;
  }
  return (memcmp(k, attribute.data(), attribute.size()) == 0);
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
  
  if (h == 0x01 || h == 0x0a) {
    // special case: empty Array or empty Object
    throw Exception(Exception::IndexOutOfBounds);
  }

  ValueLength const offsetSize = indexEntrySize(h);
  ValueLength end = readIntegerNonEmpty<ValueLength>(_start + 1, offsetSize);

  ValueLength dataOffset = 0;

  // find the number of items
  ValueLength n;
  if (h <= 0x05) {  // No offset table or length, need to compute:
    VELOCYPACK_ASSERT(h != 0x00 && h != 0x01);
    dataOffset = findDataOffset(h);
    Slice first(_start + dataOffset);
    ValueLength s = first.byteSize();
    if (s == 0) {
      throw Exception(Exception::InternalError, "Invalid data for compact object");
    }
    n = (end - dataOffset) / s;
  } else if (offsetSize < 8) {
    n = readIntegerNonEmpty<ValueLength>(_start + 1 + offsetSize, offsetSize);
  } else {
    n = readIntegerNonEmpty<ValueLength>(_start + end - offsetSize, offsetSize);
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
    return dataOffset + index * Slice(_start + dataOffset).byteSize();
  }

  ValueLength const ieBase =
      end - n * offsetSize + index * offsetSize - (offsetSize == 8 ? 8 : 0);
  return readIntegerNonEmpty<ValueLength>(_start + ieBase, offsetSize);
}

// extract the nth member from an Array
Slice Slice::getNth(ValueLength index) const {
  VELOCYPACK_ASSERT(isArray());

  return Slice(_start + getNthOffset(index));
}

// extract the nth member from an Object
Slice Slice::getNthKey(ValueLength index, bool translate) const {
  VELOCYPACK_ASSERT(type() == ValueType::Object);

  Slice s(_start + getNthOffset(index));

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
    if (Options::Defaults.attributeTranslator == nullptr) {
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

  ValueLength end = readVariableValueLength<false>(_start + 1);
  ValueLength n = readVariableValueLength<true>(_start + end - 1);
  if (VELOCYPACK_UNLIKELY(index >= n)) {
    throw Exception(Exception::IndexOutOfBounds);
  }

  ValueLength offset = 1 + getVariableValueLength(end);
  ValueLength current = 0;
  while (current != index) {
    uint8_t const* s = _start + offset;
    offset += Slice(s).byteSize();
    if (h == 0x14) {
      offset += Slice(_start + offset).byteSize();
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
    Slice key(_start + readIntegerNonEmpty<ValueLength>(_start + offset, offsetSize));

    if (key.isString()) {
      if (!key.isEqualStringUnchecked(attribute)) {
        continue;
      } 
    } else if (key.isSmallInt() || key.isUInt()) {
      // translate key
      if (!useTranslator) {
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

  ValueLength l = 0;
  ValueLength r = n - 1;
  ValueLength index = r / 2;

  while (true) {
    ValueLength offset = ieBase + index * offsetSize;
    Slice key(_start + readIntegerFixed<ValueLength, offsetSize>(_start + offset));

    int res;
    if (key.isString()) {
      res = key.compareStringUnchecked(attribute.data(), attribute.size());
    } else if (key.isSmallInt() || key.isUInt()) {
      // translate key
      if (!useTranslator) {
        // no attribute translator
        throw Exception(Exception::NeedAttributeTranslator);
      }
      res = key.translateUnchecked().compareString(attribute);
    } else {
      // invalid key
      return Slice();
    }

    if (res == 0) {
      // found. now return a Slice pointing at the value
      return Slice(key.start() + key.byteSize());
    }

    if (res > 0) {
      if (index == 0) {
        return Slice();
      }
      r = index - 1;
    } else {
      l = index + 1;
    }
    if (r < l) {
      return Slice();
    }
    
    // determine new midpoint
    index = l + ((r - l) / 2);
  }
}

// template instanciations for searchObjectKeyBinary
template Slice Slice::searchObjectKeyBinary<1>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;
template Slice Slice::searchObjectKeyBinary<2>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;
template Slice Slice::searchObjectKeyBinary<4>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;
template Slice Slice::searchObjectKeyBinary<8>(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;

SliceScope::SliceScope() : _allocations() {}

SliceScope::~SliceScope() {
  for (auto& it : _allocations) {
    delete[] it;
  }
}

Slice SliceScope::add(uint8_t const* data, ValueLength size) {
  size_t const s = checkOverflow(size);
  std::unique_ptr<uint8_t[]> copy(new uint8_t[s]);
  memcpy(copy.get(), data, s);
  _allocations.push_back(copy.get());
  return Slice(copy.release());
}

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
