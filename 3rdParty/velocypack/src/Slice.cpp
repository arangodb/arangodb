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
using VT = arangodb::velocypack::ValueType;

VT const Slice::TypeMap[256] = {
    /* 0x00 */ VT::None,     /* 0x01 */ VT::Array,
    /* 0x02 */ VT::Array,    /* 0x03 */ VT::Array,
    /* 0x04 */ VT::Array,    /* 0x05 */ VT::Array,
    /* 0x06 */ VT::Array,    /* 0x07 */ VT::Array,
    /* 0x08 */ VT::Array,    /* 0x09 */ VT::Array,
    /* 0x0a */ VT::Object,   /* 0x0b */ VT::Object,
    /* 0x0c */ VT::Object,   /* 0x0d */ VT::Object,
    /* 0x0e */ VT::Object,   /* 0x0f */ VT::Object,
    /* 0x10 */ VT::Object,   /* 0x11 */ VT::Object,
    /* 0x12 */ VT::Object,   /* 0x13 */ VT::Array,
    /* 0x14 */ VT::Object,   /* 0x15 */ VT::None,
    /* 0x16 */ VT::None,     /* 0x17 */ VT::None,
    /* 0x18 */ VT::Null,     /* 0x19 */ VT::Bool,
    /* 0x1a */ VT::Bool,     /* 0x1b */ VT::Double,
    /* 0x1c */ VT::UTCDate,  /* 0x1d */ VT::External,
    /* 0x1e */ VT::MinKey,   /* 0x1f */ VT::MaxKey,
    /* 0x20 */ VT::Int,      /* 0x21 */ VT::Int,
    /* 0x22 */ VT::Int,      /* 0x23 */ VT::Int,
    /* 0x24 */ VT::Int,      /* 0x25 */ VT::Int,
    /* 0x26 */ VT::Int,      /* 0x27 */ VT::Int,
    /* 0x28 */ VT::UInt,     /* 0x29 */ VT::UInt,
    /* 0x2a */ VT::UInt,     /* 0x2b */ VT::UInt,
    /* 0x2c */ VT::UInt,     /* 0x2d */ VT::UInt,
    /* 0x2e */ VT::UInt,     /* 0x2f */ VT::UInt,
    /* 0x30 */ VT::SmallInt, /* 0x31 */ VT::SmallInt,
    /* 0x32 */ VT::SmallInt, /* 0x33 */ VT::SmallInt,
    /* 0x34 */ VT::SmallInt, /* 0x35 */ VT::SmallInt,
    /* 0x36 */ VT::SmallInt, /* 0x37 */ VT::SmallInt,
    /* 0x38 */ VT::SmallInt, /* 0x39 */ VT::SmallInt,
    /* 0x3a */ VT::SmallInt, /* 0x3b */ VT::SmallInt,
    /* 0x3c */ VT::SmallInt, /* 0x3d */ VT::SmallInt,
    /* 0x3e */ VT::SmallInt, /* 0x3f */ VT::SmallInt,
    /* 0x40 */ VT::String,   /* 0x41 */ VT::String,
    /* 0x42 */ VT::String,   /* 0x43 */ VT::String,
    /* 0x44 */ VT::String,   /* 0x45 */ VT::String,
    /* 0x46 */ VT::String,   /* 0x47 */ VT::String,
    /* 0x48 */ VT::String,   /* 0x49 */ VT::String,
    /* 0x4a */ VT::String,   /* 0x4b */ VT::String,
    /* 0x4c */ VT::String,   /* 0x4d */ VT::String,
    /* 0x4e */ VT::String,   /* 0x4f */ VT::String,
    /* 0x50 */ VT::String,   /* 0x51 */ VT::String,
    /* 0x52 */ VT::String,   /* 0x53 */ VT::String,
    /* 0x54 */ VT::String,   /* 0x55 */ VT::String,
    /* 0x56 */ VT::String,   /* 0x57 */ VT::String,
    /* 0x58 */ VT::String,   /* 0x59 */ VT::String,
    /* 0x5a */ VT::String,   /* 0x5b */ VT::String,
    /* 0x5c */ VT::String,   /* 0x5d */ VT::String,
    /* 0x5e */ VT::String,   /* 0x5f */ VT::String,
    /* 0x60 */ VT::String,   /* 0x61 */ VT::String,
    /* 0x62 */ VT::String,   /* 0x63 */ VT::String,
    /* 0x64 */ VT::String,   /* 0x65 */ VT::String,
    /* 0x66 */ VT::String,   /* 0x67 */ VT::String,
    /* 0x68 */ VT::String,   /* 0x69 */ VT::String,
    /* 0x6a */ VT::String,   /* 0x6b */ VT::String,
    /* 0x6c */ VT::String,   /* 0x6d */ VT::String,
    /* 0x6e */ VT::String,   /* 0x6f */ VT::String,
    /* 0x70 */ VT::String,   /* 0x71 */ VT::String,
    /* 0x72 */ VT::String,   /* 0x73 */ VT::String,
    /* 0x74 */ VT::String,   /* 0x75 */ VT::String,
    /* 0x76 */ VT::String,   /* 0x77 */ VT::String,
    /* 0x78 */ VT::String,   /* 0x79 */ VT::String,
    /* 0x7a */ VT::String,   /* 0x7b */ VT::String,
    /* 0x7c */ VT::String,   /* 0x7d */ VT::String,
    /* 0x7e */ VT::String,   /* 0x7f */ VT::String,
    /* 0x80 */ VT::String,   /* 0x81 */ VT::String,
    /* 0x82 */ VT::String,   /* 0x83 */ VT::String,
    /* 0x84 */ VT::String,   /* 0x85 */ VT::String,
    /* 0x86 */ VT::String,   /* 0x87 */ VT::String,
    /* 0x88 */ VT::String,   /* 0x89 */ VT::String,
    /* 0x8a */ VT::String,   /* 0x8b */ VT::String,
    /* 0x8c */ VT::String,   /* 0x8d */ VT::String,
    /* 0x8e */ VT::String,   /* 0x8f */ VT::String,
    /* 0x90 */ VT::String,   /* 0x91 */ VT::String,
    /* 0x92 */ VT::String,   /* 0x93 */ VT::String,
    /* 0x94 */ VT::String,   /* 0x95 */ VT::String,
    /* 0x96 */ VT::String,   /* 0x97 */ VT::String,
    /* 0x98 */ VT::String,   /* 0x99 */ VT::String,
    /* 0x9a */ VT::String,   /* 0x9b */ VT::String,
    /* 0x9c */ VT::String,   /* 0x9d */ VT::String,
    /* 0x9e */ VT::String,   /* 0x9f */ VT::String,
    /* 0xa0 */ VT::String,   /* 0xa1 */ VT::String,
    /* 0xa2 */ VT::String,   /* 0xa3 */ VT::String,
    /* 0xa4 */ VT::String,   /* 0xa5 */ VT::String,
    /* 0xa6 */ VT::String,   /* 0xa7 */ VT::String,
    /* 0xa8 */ VT::String,   /* 0xa9 */ VT::String,
    /* 0xaa */ VT::String,   /* 0xab */ VT::String,
    /* 0xac */ VT::String,   /* 0xad */ VT::String,
    /* 0xae */ VT::String,   /* 0xaf */ VT::String,
    /* 0xb0 */ VT::String,   /* 0xb1 */ VT::String,
    /* 0xb2 */ VT::String,   /* 0xb3 */ VT::String,
    /* 0xb4 */ VT::String,   /* 0xb5 */ VT::String,
    /* 0xb6 */ VT::String,   /* 0xb7 */ VT::String,
    /* 0xb8 */ VT::String,   /* 0xb9 */ VT::String,
    /* 0xba */ VT::String,   /* 0xbb */ VT::String,
    /* 0xbc */ VT::String,   /* 0xbd */ VT::String,
    /* 0xbe */ VT::String,   /* 0xbf */ VT::String,
    /* 0xc0 */ VT::Binary,   /* 0xc1 */ VT::Binary,
    /* 0xc2 */ VT::Binary,   /* 0xc3 */ VT::Binary,
    /* 0xc4 */ VT::Binary,   /* 0xc5 */ VT::Binary,
    /* 0xc6 */ VT::Binary,   /* 0xc7 */ VT::Binary,
    /* 0xc8 */ VT::BCD,      /* 0xc9 */ VT::BCD,
    /* 0xca */ VT::BCD,      /* 0xcb */ VT::BCD,
    /* 0xcc */ VT::BCD,      /* 0xcd */ VT::BCD,
    /* 0xce */ VT::BCD,      /* 0xcf */ VT::BCD,
    /* 0xd0 */ VT::BCD,      /* 0xd1 */ VT::BCD,
    /* 0xd2 */ VT::BCD,      /* 0xd3 */ VT::BCD,
    /* 0xd4 */ VT::BCD,      /* 0xd5 */ VT::BCD,
    /* 0xd6 */ VT::BCD,      /* 0xd7 */ VT::BCD,
    /* 0xd8 */ VT::None,     /* 0xd9 */ VT::None,
    /* 0xda */ VT::None,     /* 0xdb */ VT::None,
    /* 0xdc */ VT::None,     /* 0xdd */ VT::None,
    /* 0xde */ VT::None,     /* 0xdf */ VT::None,
    /* 0xe0 */ VT::None,     /* 0xe1 */ VT::None,
    /* 0xe2 */ VT::None,     /* 0xe3 */ VT::None,
    /* 0xe4 */ VT::None,     /* 0xe5 */ VT::None,
    /* 0xe6 */ VT::None,     /* 0xe7 */ VT::None,
    /* 0xe8 */ VT::None,     /* 0xe9 */ VT::None,
    /* 0xea */ VT::None,     /* 0xeb */ VT::None,
    /* 0xec */ VT::None,     /* 0xed */ VT::None,
    /* 0xee */ VT::None,     /* 0xef */ VT::None,
    /* 0xf0 */ VT::Custom,   /* 0xf1 */ VT::Custom,
    /* 0xf2 */ VT::Custom,   /* 0xf3 */ VT::Custom,
    /* 0xf4 */ VT::Custom,   /* 0xf5 */ VT::Custom,
    /* 0xf6 */ VT::Custom,   /* 0xf7 */ VT::Custom,
    /* 0xf8 */ VT::Custom,   /* 0xf9 */ VT::Custom,
    /* 0xfa */ VT::Custom,   /* 0xfb */ VT::Custom,
    /* 0xfc */ VT::Custom,   /* 0xfd */ VT::Custom,
    /* 0xfe */ VT::Custom,   /* 0xff */ VT::Custom};

unsigned int const Slice::WidthMap[32] = {
    0,  // 0x00, None
    1,  // 0x01, empty array
    1,  // 0x02, array without index table
    2,  // 0x03, array without index table
    4,  // 0x04, array without index table
    8,  // 0x05, array without index table
    1,  // 0x06, array with index table
    2,  // 0x07, array with index table
    4,  // 0x08, array with index table
    8,  // 0x09, array with index table
    1,  // 0x0a, empty object
    1,  // 0x0b, object with sorted index table
    2,  // 0x0c, object with sorted index table
    4,  // 0x0d, object with sorted index table
    8,  // 0x0e, object with sorted index table
    1,  // 0x0f, object with unsorted index table
    2,  // 0x10, object with unsorted index table
    4,  // 0x11, object with unsorted index table
    8,  // 0x12, object with unsorted index table
    0};

unsigned int const Slice::FirstSubMap[32] = {
    0,  // 0x00, None
    1,  // 0x01, empty array
    2,  // 0x02, array without index table
    3,  // 0x03, array without index table
    5,  // 0x04, array without index table
    9,  // 0x05, array without index table
    3,  // 0x06, array with index table
    5,  // 0x07, array with index table
    8,  // 0x08, array with index table
    8,  // 0x09, array with index table
    1,  // 0x0a, empty object
    3,  // 0x0b, object with sorted index table
    5,  // 0x0c, object with sorted index table
    8,  // 0x0d, object with sorted index table
    8,  // 0x0e, object with sorted index table
    3,  // 0x0f, object with unsorted index table
    5,  // 0x10, object with unsorted index table
    8,  // 0x11, object with unsorted index table
    8,  // 0x12, object with unsorted index table
    0};

// creates a Slice from Json and adds it to a scope
Slice Slice::fromJson(SliceScope& scope, std::string const& json,
                      Options const* options) {
  Parser parser(options);
  parser.parse(json);

  Builder& b = parser.builder();  // don't copy Builder contents here
  return scope.add(b.start(), b.size(), options);
}

// translates an integer key into a string
Slice Slice::translate() const {
  if (!isSmallInt() && !isUInt()) {
    throw Exception(Exception::InvalidValueType,
                    "Cannot translate key of this type");
  }
  uint64_t id = getUInt();

  if (options->attributeTranslator == nullptr) {
    throw Exception(Exception::NeedAttributeTranslator);
  }

  return Slice(options->attributeTranslator->translate(id), options);
}

// check if two Slices are equal on the binary level
bool Slice::equals(Slice const& other) const {
  if (head() != other.head()) {
    return false;
  }

  ValueLength const size = byteSize();

  if (size != other.byteSize()) {
    return false;
  }

  return (memcmp(start(), other.start(),
                 arangodb::velocypack::checkOverflow(size)) == 0);
}

std::string Slice::toJson() const {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, options);
  dumper.dump(this);
  return std::move(buffer);
}

std::string Slice::toString() const {
  // copy options and set prettyPrint in copy
  Options prettyOptions = *options;
  prettyOptions.prettyPrint = true;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper::dump(this, &sink, &prettyOptions);
  return std::move(buffer);
}

std::string Slice::hexType() const { return std::move(HexDump::toHex(head())); }

// look for the specified attribute inside an Object
// returns a Slice(ValueType::None) if not found
Slice Slice::get(std::string const& attribute) const {
  if (!isType(ValueType::Object)) {
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
  ValueLength end = readInteger<ValueLength>(_start + 1, offsetSize);
  ValueLength dataOffset = 0;

  // read number of items
  ValueLength n;
  if (offsetSize < 8) {
    n = readInteger<ValueLength>(_start + 1 + offsetSize, offsetSize);
  } else {
    n = readInteger<ValueLength>(_start + end - offsetSize, offsetSize);
  }

  if (n == 1) {
    // Just one attribute, there is no index table!
    if (dataOffset == 0) {
      dataOffset = findDataOffset(h);
    }

    Slice key = Slice(_start + dataOffset, options);

    if (key.isString() && key.isEqualString(attribute)) {
      return Slice(key.start() + key.byteSize(), options);
    }

    if (key.isSmallInt() || key.isUInt()) {
      // translate key
      if (key.translate().isEqualString(attribute)) {
        return Slice(key.start() + key.byteSize(), options);
      }
    }

    // no match or invalid key type
    return Slice();
  }

  ValueLength const ieBase =
      end - n * offsetSize - (offsetSize == 8 ? offsetSize : 0);

  // only use binary search for attributes if we have at least this many entries
  // otherwise we'll always use the linear search
  static ValueLength const SortedSearchEntriesThreshold = 4;

  if (isSorted() && n >= SortedSearchEntriesThreshold) {
    // This means, we have to handle the special case n == 1 only
    // in the linear search!
    return searchObjectKeyBinary(attribute, ieBase, offsetSize, n);
  }

  return searchObjectKeyLinear(attribute, ieBase, offsetSize, n);
}

// return the value for an Int object
int64_t Slice::getInt() const {
  uint8_t const h = head();
  if (h >= 0x20 && h <= 0x27) {
    // Int  T
    uint64_t v = readInteger<uint64_t>(_start + 1, h - 0x1f);
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
    uint64_t v = getUInt();
    if (v > static_cast<uint64_t>(INT64_MAX)) {
      throw Exception(Exception::NumberOutOfRange);
    }
    return static_cast<int64_t>(v);
  }

  if (h >= 0x30 && h <= 0x3f) {
    // SmallInt
    return getSmallInt();
  }

  throw Exception(Exception::InvalidValueType, "Expecting type Int");
}

// return the value for a UInt object
uint64_t Slice::getUInt() const {
  uint8_t const h = head();
  if (h >= 0x28 && h <= 0x2f) {
    // UInt
    return readInteger<uint64_t>(_start + 1, h - 0x27);
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

int Slice::compareString(std::string const& attribute) const {
  ValueLength keyLength;
  char const* k = getString(keyLength);
  size_t const attributeLength = attribute.size();
  size_t const compareLength =
      (std::min)(static_cast<size_t>(keyLength), attributeLength);
  int res = memcmp(k, attribute.c_str(), compareLength);

  if (res == 0) {
    if (keyLength != attributeLength) {
      return (keyLength > attributeLength) ? 1 : -1;
    }
  }
  return res;
}

bool Slice::isEqualString(std::string const& attribute) const {
  ValueLength keyLength;
  char const* k = getString(keyLength);
  if (static_cast<size_t>(keyLength) != attribute.size()) {
    return false;
  }
  return (memcmp(k, attribute.c_str(), attribute.size()) == 0);
}

Slice Slice::getFromCompactObject(std::string const& attribute) const {
  ObjectIterator it(*this);
  while (it.valid()) {
    Slice key = it.key();
    if (key.makeKey().isEqualString(attribute)) {
      return Slice(key.start() + key.byteSize(), options);
    }

    it.next();
  }
  // not found
  return Slice();
}

// get the offset for the nth member from an Array or Object type
ValueLength Slice::getNthOffset(ValueLength index) const {
  VELOCYPACK_ASSERT(type() == ValueType::Array || type() == ValueType::Object);

  auto const h = head();

  if (h == 0x01 || h == 0x0a) {
    // special case: empty Array or empty Object
    throw Exception(Exception::IndexOutOfBounds);
  }

  if (h == 0x13 || h == 0x14) {
    // compact Array or Object
    return getNthOffsetFromCompact(index);
  }

  ValueLength const offsetSize = indexEntrySize(h);
  ValueLength end = readInteger<ValueLength>(_start + 1, offsetSize);

  ValueLength dataOffset = findDataOffset(h);

  // find the number of items
  ValueLength n;
  if (h <= 0x05) {  // No offset table or length, need to compute:
    Slice first(_start + dataOffset, options);
    n = (end - dataOffset) / first.byteSize();
  } else if (offsetSize < 8) {
    n = readInteger<ValueLength>(_start + 1 + offsetSize, offsetSize);
  } else {
    n = readInteger<ValueLength>(_start + end - offsetSize, offsetSize);
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
      dataOffset = findDataOffset(h);
    }
    Slice firstItem(_start + dataOffset, options);
    return dataOffset + index * firstItem.byteSize();
  }

  ValueLength const ieBase =
      end - n * offsetSize + index * offsetSize - (offsetSize == 8 ? 8 : 0);
  return readInteger<ValueLength>(_start + ieBase, offsetSize);
}

// extract the nth member from an Array
Slice Slice::getNth(ValueLength index) const {
  VELOCYPACK_ASSERT(type() == ValueType::Array);

  return Slice(_start + getNthOffset(index), options);
}

// extract the nth member from an Object
Slice Slice::getNthKey(ValueLength index, bool translate) const {
  VELOCYPACK_ASSERT(type() == ValueType::Object);

  Slice s(_start + getNthOffset(index), options);

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
    return translate();
  }

  throw Exception(Exception::InvalidValueType,
                  "Cannot translate key of this type");
}

// get the offset for the nth member from a compact Array or Object type
ValueLength Slice::getNthOffsetFromCompact(ValueLength index) const {
  ValueLength end = readVariableValueLength<false>(_start + 1);
  ValueLength n = readVariableValueLength<true>(_start + end - 1);
  if (index >= n) {
    throw Exception(Exception::IndexOutOfBounds);
  }

  auto const h = head();
  ValueLength offset = 1 + getVariableValueLength(end);
  ValueLength current = 0;
  while (current != index) {
    uint8_t const* s = _start + offset;
    Slice key = Slice(s, options);
    offset += key.byteSize();
    if (h == 0x14) {
      Slice value = Slice(_start + offset, options);
      offset += value.byteSize();
    }
    ++current;
  }
  return offset;
}

// perform a linear search for the specified attribute inside an Object
Slice Slice::searchObjectKeyLinear(std::string const& attribute,
                                   ValueLength ieBase, ValueLength offsetSize,
                                   ValueLength n) const {
  for (ValueLength index = 0; index < n; ++index) {
    ValueLength offset = ieBase + index * offsetSize;
    Slice key(_start + readInteger<ValueLength>(_start + offset, offsetSize),
              options);

    if (key.isString()) {
      if (!key.isEqualString(attribute)) {
        continue;
      }
    } else if (key.isSmallInt() || key.isUInt()) {
      // translate key
      if (!key.translate().isEqualString(attribute)) {
        continue;
      }
    } else {
      // invalid key type
      return Slice();
    }

    // key is identical. now return value
    return Slice(key.start() + key.byteSize(), options);
  }

  // nothing found
  return Slice();
}

// perform a binary search for the specified attribute inside an Object
Slice Slice::searchObjectKeyBinary(std::string const& attribute,
                                   ValueLength ieBase, ValueLength offsetSize,
                                   ValueLength n) const {
  VELOCYPACK_ASSERT(n > 0);

  ValueLength l = 0;
  ValueLength r = n - 1;

  while (true) {
    // midpoint
    ValueLength index = l + ((r - l) / 2);

    ValueLength offset = ieBase + index * offsetSize;
    Slice key(_start + readInteger<ValueLength>(_start + offset, offsetSize),
              options);

    int res;
    if (key.isString()) {
      res = key.compareString(attribute);
    } else if (key.isSmallInt() || key.isUInt()) {
      // translate key
      res = key.translate().compareString(attribute);
    } else {
      // invalid key
      return Slice();
    }

    if (res == 0) {
      // found
      return Slice(key.start() + key.byteSize(), options);
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
  }
}

SliceScope::SliceScope() : _allocations() {}

SliceScope::~SliceScope() {
  for (auto& it : _allocations) {
    delete[] it;
  }
}

Slice SliceScope::add(uint8_t const* data, ValueLength size,
                      Options const* options) {
  size_t const s = checkOverflow(size);
  std::unique_ptr<uint8_t[]> copy(new uint8_t[s]);
  memcpy(copy.get(), data, s);
  _allocations.push_back(copy.get());
  uint8_t const* start = copy.release();

  return Slice(start, options);
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
                  sizeof(void*) + sizeof(void*),
              "Slice has an unexpected size");
