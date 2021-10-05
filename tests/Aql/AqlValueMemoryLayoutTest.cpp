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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Basics/Endian.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
    
constexpr uint8_t UNINITIALIZED = 0xa5;

void runChecksForNumber(AqlValue const& value, uint8_t const* expected) {
  EXPECT_FALSE(value.requiresDestruction());
  EXPECT_FALSE(value.isEmpty());
  EXPECT_FALSE(value.isPointer());
  EXPECT_FALSE(value.isManagedDocument());
  EXPECT_FALSE(value.isRange());
  EXPECT_FALSE(value.isNone());
  EXPECT_FALSE(value.isNull(false));
  EXPECT_FALSE(value.isNull(true));
  EXPECT_FALSE(value.isBoolean());
  EXPECT_TRUE(value.isNumber());
  EXPECT_FALSE(value.isString());
  EXPECT_FALSE(value.isObject());
  EXPECT_FALSE(value.isArray());

  if constexpr (arangodb::basics::isLittleEndian()) {
    uint8_t const* data = reinterpret_cast<uint8_t const*>(&value);

    for (uint8_t i = 0; i < sizeof(AqlValue); ++i) {
      // note: in all tests, the value UNINITIALIZED (0xa5) has a special 
      // meaning in a byte and will lead to it not being compared
      if (expected[i] == UNINITIALIZED) {
        continue;
      }
      EXPECT_EQ(data[i], expected[i]);
    }
  }
}

void runChecksForUInt64(uint64_t value, uint8_t const* expected) {
  struct alignas(16) AqlValueMemory {
    AqlValueMemory() {
      // poison memory with some garbage values
      memset(&buffer[0], 0x99, sizeof(buffer));
    }
    uint8_t buffer[16];
  
    static_assert(sizeof(buffer) == 16, "invalid size of AqlValueMemory buffer");
  };
  static_assert(sizeof(AqlValueMemory) == 16, "invalid size of AqlValueMemory");

  // poison some memory with 0x99 values
  AqlValueMemory memory;
  void* p = reinterpret_cast<void*>(&memory);

  // put uint64_t value into an AqlValue directly using the AqlValueHintUInt ctor.
  // note: we are using placement new here, so that the AqlValue uses the
  // poisoned memory region
  new (p) AqlValue((AqlValueHintUInt(value)));
  // although we are using placement new here, we don't need to call the
  // destructor here, as the AqlValue with the payloads we use here won't
  // do anything in its destructor

  AqlValue& aqlValue = *reinterpret_cast<AqlValue*>(p);
  runChecksForNumber(aqlValue, expected);

  EXPECT_EQ(value != 0, aqlValue.toBoolean());
  
  EXPECT_EQ(value, aqlValue.slice().getNumber<uint64_t>());

  if (value <= static_cast<uint64_t>(INT64_MAX)) {
    EXPECT_EQ(static_cast<int64_t>(value), aqlValue.toInt64());
    EXPECT_EQ(static_cast<int64_t>(value), aqlValue.slice().getNumber<int64_t>());
  }
}

// note: in all following tests, the value UNINITIALIZED (0xa5) has a special meaning.
// if it used, it means that we don't compare the byte, because it is not initialized
// in certain AqlValue configurations.
// if you add tests, make sure that none of the test data includes 0xa5!
TEST(AqlValueMemoryLayoutTest, UnsignedSmallValues48Bit_0) {
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x30, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(0ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedSmallValues48Bit_1) {
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x31, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(1ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedSmallValues48Bit_2) {
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x32, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(2ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedSmallValues48Bit_5) {
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x35, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(5ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedSmallValues48Bit_9) {
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x39, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 
    0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(9ULL), expected);
}
 
// note: in all following tests, the value UNINITIALIZED (0xa5) has a special meaning.
// if it used, it means that we don't compare the byte, because it is not initialized
// in certain AqlValue configurations.
// if you add tests, make sure that none of the test data includes 0xa5!
TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_10) {
  // 0a
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x28, 0x0a, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(10ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_11) {
  // 0b
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x28, 0x0b, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(11ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_255) {
  // ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x29, 0xff, 0x00, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(255ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_256) {
  // 01 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x29, 0x00, 0x01, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(256ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_511) {
  // 01 ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x29, 0xff, 0x01, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(511ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_32767) {
  // 7f ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x29, 0xff, 0x7f, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(32767ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_32768) {
  // 80 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2a, 0x00, 0x80, 0x00, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(32768ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_65534) {
  // ff fe
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2a, 0xfe, 0xff, 0x00, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(65534ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_65535) {
  // ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2a, 0xff, 0xff, 0x00, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(65535ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_65536) {
  // 01 00 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2a, 0x00, 0x00, 0x01, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(65536ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_65537) {
  // 01 00 01
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2a, 0x01, 0x00, 0x01, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(65537ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_1073741824) {
  // 40 00 00 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2b, 0x00, 0x00, 0x00, 0x40, UNINITIALIZED, UNINITIALIZED,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(1073741824ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_197374285) {
  // 75 a4 ec e9
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2b, 0xe9, 0xec, 0xa4, 0x75, UNINITIALIZED, UNINITIALIZED,
    0xe9, 0xec, 0xa4, 0x75, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(1973742825ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_2147483647) {
  // 7f ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2b, 0xff, 0xff, 0xff, 0x7f, UNINITIALIZED, UNINITIALIZED,
    0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(2147483647ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_2147483648) {
  // 7f ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2c, 0x00, 0x00, 0x00, 0x80, 0x00, UNINITIALIZED,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(2147483648ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_4294967294) {
  // ff ff ff fe
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2c, 0xfe, 0xff, 0xff, 0xff, 0x00, UNINITIALIZED,
    0xfe, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(4294967294ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_4294967295) {
  // ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2c, 0xff, 0xff, 0xff, 0xff, 0x00, UNINITIALIZED,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(4294967295ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_4294967296) {
  // 01 00 00 00 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, UNINITIALIZED,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(4294967296ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_549755813887) {
  // 7f ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2c, 0xff, 0xff, 0xff, 0xff, 0x7f, UNINITIALIZED,
    0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(549755813887ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_549755813888) {
  // 7f ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(549755813888ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_1099511627776) {
  // 01 00 00 00 00 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(1099511627776ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues48Bit_140737488355327) {
  // 7f ff ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_INT48, 0x2d, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(140737488355327ULL), expected);
}

// note: in all following tests, the value UNINITIALIZED (0xa5) has a special meaning.
// if it used, it means that we don't compare the byte, because it is not initialized
// in certain AqlValue configurations.
// if you add tests, make sure that none of the test data includes 0xa5!
TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues64Bit_281474976710654) {
  // ff ff ff ff ff fe
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_UINT64, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 0x2f,
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(281474976710654ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues64Bit_281474976710655) {
  // ff ff ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_UINT64, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 0x2f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00
  };
  runChecksForUInt64(uint64_t(281474976710655ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues64Bit_72057594037927935) {
  // ff ff ff ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_UINT64, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 0x2f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00
  };
  runChecksForUInt64(uint64_t(72057594037927935ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues64Bit_72057594037927936) {
  // 01 00 00 00 00 00 00 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_UINT64, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 0x2f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
  };
  runChecksForUInt64(uint64_t(72057594037927936ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues64Bit_9223372036854775807) {
  // 7f ff ff ff ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_UINT64, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 0x2f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
  };
  runChecksForUInt64(uint64_t(9223372036854775807ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues64Bit_9223372036854775808) {
  // 80 00 00 00 00 00 00 00
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_UINT64, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 0x2f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  };
  runChecksForUInt64(uint64_t(9223372036854775808ULL), expected);
}

TEST(AqlValueMemoryLayoutTest, UnsignedLargerValues64Bit_18446744073709551615) {
  // ff ff ff ff ff ff ff ff
  uint8_t const expected[] = { 
    AqlValue::AqlValueType::VPACK_INLINE_UINT64, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED, 0x2f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };
  runChecksForUInt64(uint64_t(18446744073709551615ULL), expected);
}

} // namespace
