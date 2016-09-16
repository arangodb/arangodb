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

ValueLength const SliceStaticData::FixedTypeLengths[256] = {
    /* 0x00 */ 1,                    /* 0x01 */ 1,
    /* 0x02 */ 0,                    /* 0x03 */ 0,
    /* 0x04 */ 0,                    /* 0x05 */ 0,
    /* 0x06 */ 0,                    /* 0x07 */ 0,
    /* 0x08 */ 0,                    /* 0x09 */ 0,
    /* 0x0a */ 1,                    /* 0x0b */ 0,
    /* 0x0c */ 0,                    /* 0x0d */ 0,
    /* 0x0e */ 0,                    /* 0x0f */ 0,
    /* 0x10 */ 0,                    /* 0x11 */ 0,
    /* 0x12 */ 0,                    /* 0x13 */ 0,
    /* 0x14 */ 0,                    /* 0x15 */ 0,
    /* 0x16 */ 0,                    /* 0x17 */ 1,
    /* 0x18 */ 1,                    /* 0x19 */ 1,
    /* 0x1a */ 1,                    /* 0x1b */ 1 + sizeof(double),
    /* 0x1c */ 1 + sizeof(int64_t),  /* 0x1d */ 1 + sizeof(char*), 
    /* 0x1e */ 1,                    /* 0x1f */ 1,
    /* 0x20 */ 2,                    /* 0x21 */ 3,
    /* 0x22 */ 4,                    /* 0x23 */ 5,
    /* 0x24 */ 6,                    /* 0x25 */ 7,
    /* 0x26 */ 8,                    /* 0x27 */ 9,
    /* 0x28 */ 2,                    /* 0x29 */ 3,
    /* 0x2a */ 4,                    /* 0x2b */ 5,
    /* 0x2c */ 6,                    /* 0x2d */ 7,
    /* 0x2e */ 8,                    /* 0x2f */ 9,
    /* 0x30 */ 1,                    /* 0x31 */ 1,
    /* 0x32 */ 1,                    /* 0x33 */ 1,
    /* 0x34 */ 1,                    /* 0x35 */ 1,
    /* 0x36 */ 1,                    /* 0x37 */ 1,
    /* 0x38 */ 1,                    /* 0x39 */ 1,
    /* 0x3a */ 1,                    /* 0x3b */ 1,
    /* 0x3c */ 1,                    /* 0x3d */ 1,
    /* 0x3e */ 1,                    /* 0x3f */ 1,
    /* 0x40 */ 1,                    /* 0x41 */ 2,
    /* 0x42 */ 3,                    /* 0x43 */ 4,
    /* 0x44 */ 5,                    /* 0x45 */ 6,
    /* 0x46 */ 7,                    /* 0x47 */ 8,
    /* 0x48 */ 9,                    /* 0x49 */ 10,
    /* 0x4a */ 11,                   /* 0x4b */ 12,
    /* 0x4c */ 13,                   /* 0x4d */ 14,
    /* 0x4e */ 15,                   /* 0x4f */ 16,
    /* 0x50 */ 17,                   /* 0x51 */ 18,
    /* 0x52 */ 19,                   /* 0x53 */ 20,
    /* 0x54 */ 21,                   /* 0x55 */ 22,
    /* 0x56 */ 23,                   /* 0x57 */ 24,
    /* 0x58 */ 25,                   /* 0x59 */ 26,
    /* 0x5a */ 27,                   /* 0x5b */ 28,
    /* 0x5c */ 29,                   /* 0x5d */ 30,
    /* 0x5e */ 31,                   /* 0x5f */ 32,
    /* 0x60 */ 33,                   /* 0x61 */ 34,
    /* 0x62 */ 35,                   /* 0x63 */ 36,
    /* 0x64 */ 37,                   /* 0x65 */ 38,
    /* 0x66 */ 39,                   /* 0x67 */ 40,
    /* 0x68 */ 41,                   /* 0x69 */ 42,
    /* 0x6a */ 43,                   /* 0x6b */ 44,
    /* 0x6c */ 45,                   /* 0x6d */ 46,
    /* 0x6e */ 47,                   /* 0x6f */ 48,
    /* 0x70 */ 49,                   /* 0x71 */ 50,
    /* 0x72 */ 51,                   /* 0x73 */ 52,
    /* 0x74 */ 53,                   /* 0x75 */ 54,
    /* 0x76 */ 55,                   /* 0x77 */ 56,
    /* 0x78 */ 57,                   /* 0x79 */ 58,
    /* 0x7a */ 59,                   /* 0x7b */ 60,
    /* 0x7c */ 61,                   /* 0x7d */ 62,
    /* 0x7e */ 63,                   /* 0x7f */ 64,
    /* 0x80 */ 65,                   /* 0x81 */ 66,
    /* 0x82 */ 67,                   /* 0x83 */ 68,
    /* 0x84 */ 69,                   /* 0x85 */ 70,
    /* 0x86 */ 71,                   /* 0x87 */ 72,
    /* 0x88 */ 73,                   /* 0x89 */ 74,
    /* 0x8a */ 75,                   /* 0x8b */ 76,
    /* 0x8c */ 77,                   /* 0x8d */ 78,
    /* 0x8e */ 79,                   /* 0x8f */ 80,
    /* 0x90 */ 81,                   /* 0x91 */ 82,
    /* 0x92 */ 83,                   /* 0x93 */ 84,
    /* 0x94 */ 85,                   /* 0x95 */ 86,
    /* 0x96 */ 87,                   /* 0x97 */ 88,
    /* 0x98 */ 89,                   /* 0x99 */ 90,
    /* 0x9a */ 91,                   /* 0x9b */ 92,
    /* 0x9c */ 93,                   /* 0x9d */ 94,
    /* 0x9e */ 95,                   /* 0x9f */ 96,
    /* 0xa0 */ 97,                   /* 0xa1 */ 98,
    /* 0xa2 */ 99,                   /* 0xa3 */ 100,
    /* 0xa4 */ 101,                  /* 0xa5 */ 102,
    /* 0xa6 */ 103,                  /* 0xa7 */ 104,
    /* 0xa8 */ 105,                  /* 0xa9 */ 106,
    /* 0xaa */ 107,                  /* 0xab */ 108,
    /* 0xac */ 109,                  /* 0xad */ 110,
    /* 0xae */ 111,                  /* 0xaf */ 112,
    /* 0xb0 */ 113,                  /* 0xb1 */ 114,
    /* 0xb2 */ 115,                  /* 0xb3 */ 116,
    /* 0xb4 */ 117,                  /* 0xb5 */ 118,
    /* 0xb6 */ 119,                  /* 0xb7 */ 120,
    /* 0xb8 */ 121,                  /* 0xb9 */ 122,
    /* 0xba */ 123,                  /* 0xbb */ 124,
    /* 0xbc */ 125,                  /* 0xbd */ 126,
    /* 0xbe */ 127,                  /* 0xbf */ 0,
    /* 0xc0 */ 0,                    /* 0xc1 */ 0,
    /* 0xc2 */ 0,                    /* 0xc3 */ 0,
    /* 0xc4 */ 0,                    /* 0xc5 */ 0,
    /* 0xc6 */ 0,                    /* 0xc7 */ 0,
    /* 0xc8 */ 0,                    /* 0xc9 */ 0,        
    /* 0xca */ 0,                    /* 0xcb */ 0,        
    /* 0xcc */ 0,                    /* 0xcd */ 0,        
    /* 0xce */ 0,                    /* 0xcf */ 0,        
    /* 0xd0 */ 0,                    /* 0xd1 */ 0,        
    /* 0xd2 */ 0,                    /* 0xd3 */ 0,        
    /* 0xd4 */ 0,                    /* 0xd5 */ 0,        
    /* 0xd6 */ 0,                    /* 0xd7 */ 0,        
    /* 0xd8 */ 0,                    /* 0xd9 */ 0,       
    /* 0xda */ 0,                    /* 0xdb */ 0,       
    /* 0xdc */ 0,                    /* 0xdd */ 0,       
    /* 0xde */ 0,                    /* 0xdf */ 0,       
    /* 0xe0 */ 0,                    /* 0xe1 */ 0,       
    /* 0xe2 */ 0,                    /* 0xe3 */ 0,       
    /* 0xe4 */ 0,                    /* 0xe5 */ 0,       
    /* 0xe6 */ 0,                    /* 0xe7 */ 0,       
    /* 0xe8 */ 0,                    /* 0xe9 */ 0,       
    /* 0xea */ 0,                    /* 0xeb */ 0,       
    /* 0xec */ 0,                    /* 0xed */ 0,       
    /* 0xee */ 0,                    /* 0xef */ 0,       
    /* 0xf0 */ 2,                    /* 0xf1 */ 3,
    /* 0xf2 */ 5,                    /* 0xf3 */ 9,
    /* 0xf4 */ 0,                    /* 0xf5 */ 0,
    /* 0xf6 */ 0,                    /* 0xf7 */ 0,
    /* 0xf8 */ 0,                    /* 0xf9 */ 0,
    /* 0xfa */ 0,                    /* 0xfb */ 0,
    /* 0xfc */ 0,                    /* 0xfd */ 0,
    /* 0xfe */ 0,                    /* 0xff */ 0};
 
VT const SliceStaticData::TypeMap[256] = {
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
    /* 0x16 */ VT::None,     /* 0x17 */ VT::Illegal,
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

unsigned int const SliceStaticData::WidthMap[32] = {
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

unsigned int const SliceStaticData::FirstSubMap[32] = {
    0,  // 0x00, None
    1,  // 0x01, empty array
    2,  // 0x02, array without index table
    3,  // 0x03, array without index table
    5,  // 0x04, array without index table
    9,  // 0x05, array without index table
    3,  // 0x06, array with index table
    5,  // 0x07, array with index table
    9,  // 0x08, array with index table
    9,  // 0x09, array with index table
    1,  // 0x0a, empty object
    3,  // 0x0b, object with sorted index table
    5,  // 0x0c, object with sorted index table
    9,  // 0x0d, object with sorted index table
    9,  // 0x0e, object with sorted index table
    3,  // 0x0f, object with unsorted index table
    5,  // 0x10, object with unsorted index table
    9,  // 0x11, object with unsorted index table
    9,  // 0x12, object with unsorted index table
    0};

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
  if (!isSmallInt() && !isUInt()) {
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
uint64_t Slice::getUIntUnchecked() const {
  uint8_t const h = head();
  if (h >= 0x28 && h <= 0x2f) {
    // UInt
    return readInteger<uint64_t>(_start + 1, h - 0x27);
  }

  if (h >= 0x30 && h <= 0x39) {
    // Smallint >= 0
    return static_cast<uint64_t>(h - 0x30);
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
    uint64_t const n = length() ^ 0xba5bedf00d;
    value = VELOCYPACK_HASH(&n, sizeof(n), seed);
    for (auto const& it : ArrayIterator(*this)) {
      value ^= it.normalizedHash(value);
    }
  } else if (isObject()) {
    // normalize objects by hashing object length and iterating
    // over all object members
    uint64_t const n = length() ^ 0xf00ba44ba5;
    uint64_t seed2 = VELOCYPACK_HASH(&n, sizeof(n), seed);
    value = seed2;
    for (auto const& it : ObjectIterator(*this, true)) {
      uint64_t seed3 = it.key.makeKey().normalizedHash(seed2);
      value ^= seed3;
      value ^= it.value.normalizedHash(seed3);
    }
  } else {
    // fall back to regular hash function
    value = hash(seed);
  }

  return value;
}

// look for the specified attribute inside an Object
// returns a Slice(ValueType::None) if not found
Slice Slice::get(std::string const& attribute) const {
  if (!isObject()) {
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

  // read number of items
  ValueLength n;
  if (offsetSize < 8) {
    n = readInteger<ValueLength>(_start + 1 + offsetSize, offsetSize);
  } else {
    n = readInteger<ValueLength>(_start + end - offsetSize, offsetSize);
  }

  if (n == 1) {
    // Just one attribute, there is no index table!
    Slice key = Slice(_start + findDataOffset(h));

    if (key.isString()) {
      if (key.isEqualString(attribute)) {
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

  ValueLength const ieBase =
      end - n * offsetSize - (offsetSize == 8 ? offsetSize : 0);

  // only use binary search for attributes if we have at least this many entries
  // otherwise we'll always use the linear search
  static ValueLength const SortedSearchEntriesThreshold = 4;

  bool const isSorted = (h >= 0x0b && h <= 0x0e);
  if (isSorted && n >= SortedSearchEntriesThreshold) {
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

int Slice::compareString(char const* value, size_t length) const {
  ValueLength keyLength;
  char const* k = getString(keyLength);
  size_t const compareLength =
      (std::min)(static_cast<size_t>(keyLength), length);
  int res = memcmp(k, value, compareLength);

  if (res == 0) {
    if (keyLength != length) {
      return (keyLength > length) ? 1 : -1;
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
  ValueLength end = readInteger<ValueLength>(_start + 1, offsetSize);

  ValueLength dataOffset = 0;

  // find the number of items
  ValueLength n;
  if (h <= 0x05) {  // No offset table or length, need to compute:
    dataOffset = findDataOffset(h);
    Slice first(_start + dataOffset);
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
    return dataOffset + index * Slice(_start + dataOffset).byteSize();
  }

  ValueLength const ieBase =
      end - n * offsetSize + index * offsetSize - (offsetSize == 8 ? 8 : 0);
  return readInteger<ValueLength>(_start + ieBase, offsetSize);
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
    offset += Slice(s).byteSize();
    if (h == 0x14) {
      offset += Slice(_start + offset).byteSize();
    }
    ++current;
  }
  return offset;
}

// perform a linear search for the specified attribute inside an Object
Slice Slice::searchObjectKeyLinear(std::string const& attribute,
                                   ValueLength ieBase, ValueLength offsetSize,
                                   ValueLength n) const {
  bool const useTranslator = (Options::Defaults.attributeTranslator != nullptr);

  for (ValueLength index = 0; index < n; ++index) {
    ValueLength offset = ieBase + index * offsetSize;
    Slice key(_start + readInteger<ValueLength>(_start + offset, offsetSize));

    if (key.isString()) {
      if (!key.isEqualString(attribute)) {
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
Slice Slice::searchObjectKeyBinary(std::string const& attribute,
                                   ValueLength ieBase, ValueLength offsetSize,
                                   ValueLength n) const {
  bool const useTranslator = (Options::Defaults.attributeTranslator != nullptr);
  VELOCYPACK_ASSERT(n > 0);

  ValueLength l = 0;
  ValueLength r = n - 1;

  while (true) {
    // midpoint
    ValueLength index = l + ((r - l) / 2);

    ValueLength offset = ieBase + index * offsetSize;
    Slice key(_start + readInteger<ValueLength>(_start + offset, offsetSize));

    int res;
    if (key.isString()) {
      res = key.compareString(attribute);
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
      // found
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
  }
}

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
