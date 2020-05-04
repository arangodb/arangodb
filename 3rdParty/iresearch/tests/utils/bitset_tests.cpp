////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/bitset.hpp"

using namespace iresearch;

TEST(bitset_tests, static_functions) {
  // word index for the specified bit
  ASSERT_EQ(0, bitset::word(0));
  ASSERT_EQ(64 / (sizeof(bitset::word_t)*8), bitset::word(64));
  ASSERT_EQ(376 / (sizeof(bitset::word_t)*8), bitset::word(376));

  // bit index for the specified number
  ASSERT_EQ(7, bitset::bit(7));
  ASSERT_EQ(65 % (sizeof(bitset::word_t)*8), bitset::bit(65));

  // bit offset for the specified word
  ASSERT_EQ(0, bitset::bit_offset(0));
  ASSERT_EQ(2*(sizeof(bitset::word_t)*8), bitset::bit_offset(2));
}

TEST(bitset_tests, ctor) {
  // zero size bitset
  {
    const bitset::index_t words = 0;
    const bitset::index_t size = 0;
    const bitset bs(size);
    ASSERT_EQ(nullptr, bs.data());
    ASSERT_EQ(bs.data(), bs.begin());
    ASSERT_EQ(bs.data() + words, bs.end());
    ASSERT_EQ(size, bs.size());
    ASSERT_EQ(words, bs.words());
    ASSERT_EQ(words, std::distance(bs.begin(), bs.end()));
    ASSERT_EQ(0, bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_TRUE(bs.all());
  }

  // less that 1 word size bitset
  {
    const bitset::index_t words = 1;
    const bitset::index_t size = 32;
    const bitset bs( size );
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ(bs.data(), bs.begin());
    ASSERT_EQ(bs.data() + words, bs.end());
    ASSERT_EQ(size, bs.size());
    ASSERT_EQ(words, bs.words());
    ASSERT_EQ(words, std::distance(bs.begin(), bs.end()));
    ASSERT_EQ(sizeof(bitset::word_t)*8*words, bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_FALSE(bs.all());
  }

  // uint64_t size bitset
  {
    const bitset::index_t size = 64;
    const bitset::index_t words = 1;
    const bitset bs(size);
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ(bs.data(), bs.begin());
    ASSERT_EQ(bs.data() + words, bs.end());
    ASSERT_EQ(size, bs.size());
    ASSERT_EQ(words, bs.words());
    ASSERT_EQ(words, std::distance(bs.begin(), bs.end()));
    ASSERT_EQ(size, bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_FALSE(bs.all());
  }

  // nonzero size bitset
  {
    const bitset::index_t words = 2;
    const bitset::index_t size = 78;
    const bitset bs( size );
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ(bs.data(), bs.begin());
    ASSERT_EQ(bs.data() + words, bs.end());
    ASSERT_EQ(size, bs.size());
    ASSERT_EQ(words, bs.words());
    ASSERT_EQ(bs.data() + words, bs.end());
    ASSERT_EQ(sizeof(bitset::word_t)*8*words, bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_FALSE(bs.all());
  }
}

TEST(bitset_tests, set_unset) {
  const bitset::index_t words = 3;
  const bitset::index_t size = 155;
  bitset bs(size);
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(bs.data(), bs.begin());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(sizeof(bitset::word_t)*8*words, bs.capacity());
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_FALSE(bs.all());

  // set
  const bitset::index_t i = 43;
  ASSERT_FALSE(bs.test(i));
  bs.set(i);
  ASSERT_TRUE(bs.test(i));
  ASSERT_EQ(1, bs.count());

  // unset
  bs.unset(i);
  ASSERT_FALSE(bs.test(i));
  ASSERT_EQ(0, bs.count());

  // reset
  bs.reset(i, true);
  ASSERT_TRUE(bs.test(i));
  bs.reset(i, false);
  ASSERT_FALSE(bs.test(i));
}

TEST(bitset_tests, reset) {
  bitset bs;
  ASSERT_EQ(nullptr, bs.data());
  ASSERT_EQ(bs.data(), bs.begin());
  ASSERT_EQ(0, bs.size());
  ASSERT_EQ(0, bs.words());
  ASSERT_EQ(0, bs.capacity());
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_TRUE(bs.all());

  // reset to bigger size
  bitset::index_t words = 3;
  bitset::index_t size = 155;

  bs.reset(size);
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(sizeof(bitset::word_t)*8*words, bs.capacity());
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_FALSE(bs.all());
  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(73);
  ASSERT_EQ(2, bs.count());
  ASSERT_FALSE(bs.none() );
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());
  const auto* prev_data = bs.data();

  // reset to smaller size
  words = 2;
  size = 89;

  bs.reset(size); // reset to smaller size
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(prev_data, bs.data()); // storage haven't changed
  ASSERT_EQ(sizeof(bitset::word_t)*8*words, bs.capacity());;
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none()); // content cleared
  ASSERT_FALSE(bs.any()); // content cleared
  ASSERT_FALSE(bs.all()); // content cleared
  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(73); 
  ASSERT_EQ(2, bs.count());
  ASSERT_FALSE(bs.none() );
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());

  // reset to bigger size
  words = 5;
  size = 319;

  bs.reset(size); // reset to smaller size
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_NE(prev_data, bs.data()); // storage was reallocated
  ASSERT_EQ(sizeof(bitset::word_t)*8*words, bs.capacity());
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none()); // content cleared
  ASSERT_FALSE(bs.any()); // content cleared
  ASSERT_FALSE(bs.all()); // content cleared
  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(73);
  ASSERT_EQ(2, bs.count());
  ASSERT_FALSE(bs.none());
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());
}

TEST(bitset_tests, clear_count) {
  const bitset::index_t words = 3;
  const bitset::index_t size = 155;
  bitset bs(size);
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(sizeof(bitset::word_t)*8*words, bs.capacity());
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_FALSE(bs.all());

  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(42);
  ASSERT_EQ(1, bs.count());
  bs.set(73);
  ASSERT_EQ(2, bs.count());
  ASSERT_FALSE(bs.none());
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());

  // set almost all bits
  const bitset::index_t end = 100;
  for (bitset::index_t i = 0; i < end; ++i) {
    bs.set(i);
  }
  ASSERT_EQ(end, bs.count());
  ASSERT_FALSE(bs.none());
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());

  // set almost all
  for (bitset::index_t i = 0; i < bs.size(); ++i) {
    bs.set(i);
  }
  ASSERT_EQ(bs.size(), bs.count());
  ASSERT_FALSE(bs.none());
  ASSERT_TRUE(bs.any());
  ASSERT_TRUE(bs.all());

  // clear all bits
  bs.clear();
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_FALSE(bs.all());
}

TEST(bitset_tests, memset) {
  // empty bitset
  {
    bitset bs;
    ASSERT_EQ(nullptr, bs.data());
    ASSERT_EQ(0, bs.size());
    ASSERT_EQ(0, bs.words());
    ASSERT_EQ(0, bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_TRUE(bs.all());

    bs.memset(0x723423);

    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_TRUE(bs.all());
  }

  // single word bitset
  {
    const bitset::index_t words = 1;
    const bitset::index_t size = 15;

    bitset bs(size);
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ(size, bs.size());
    ASSERT_EQ(words, bs.words());
    ASSERT_EQ(words*sizeof(bitset::word_t)*8, bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_FALSE(bs.all());

    bitset::word_t value = 0x723423;
    bs.memset(value);

    ASSERT_EQ(6, bs.count()); // only first 15 bits from 'value' are set
    ASSERT_EQ(*bs.begin(), value & 0x7FFF);
    ASSERT_FALSE(bs.none());
    ASSERT_TRUE(bs.any());
    ASSERT_FALSE(bs.all());

    value = 0xFFFFFFFF;
    bs.memset(value); // set another value

    ASSERT_EQ(size, bs.count()); // only first 15 bits from 'value' are set
    ASSERT_EQ(*bs.begin(), value& 0x7FFF);
    ASSERT_FALSE(bs.none());
    ASSERT_TRUE(bs.any());
    ASSERT_TRUE(bs.all());
  }

  // multiple words bitset
  {
    const bitset::index_t words = 2;
    const bitset::index_t size = 78;

    bitset bs(size);
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ(size, bs.size());
    ASSERT_EQ(words, bs.words());
    ASSERT_EQ(words*sizeof(bitset::word_t)*8, bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_FALSE(bs.all());

    const uint64_t value = UINT64_C(0x14FFFFFFFFFFFFFF);
    bs.memset(value);

    ASSERT_EQ(58, bs.count()); // only first 15 bits from 'value' are set
    ASSERT_EQ(*(bs.begin() + bitset::word(0)), value); // 1st word
    ASSERT_EQ(*(bs.begin() + bitset::word(64)), 0); // 2nd word
    ASSERT_FALSE(bs.none());
    ASSERT_TRUE(bs.any());
    ASSERT_FALSE(bs.all());
  }

  // multiple words bitset (all set)
  {
    const bitset::index_t words = 2;
    const bitset::index_t size = 2 * irs::bits_required<irs::bitset::word_t>();

    irs::bitset bs(size);
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ(size, bs.size());
    ASSERT_EQ(words, bs.words());
    ASSERT_EQ(words * irs::bits_required<irs::bitset::word_t>(), bs.capacity());
    ASSERT_EQ(0, bs.count());
    ASSERT_TRUE(bs.none());
    ASSERT_FALSE(bs.any());
    ASSERT_FALSE(bs.all());

    struct value_t {
      uint64_t value0;
      uint64_t value1;
    } value;
    value.value0 = UINT64_C(0xFFFFFFFFFFFFFFFF);
    value.value1 = UINT64_C(0xFFFFFFFFFFFFFFFF);
    bs.memset(value);

    ASSERT_EQ(sizeof(value_t) * irs::bits_required<uint8_t>(), bs.size()); // full size of bitset
    ASSERT_EQ(128, bs.count()); // all 128 from 'value' are set
    ASSERT_EQ(*(bs.begin() + irs::bitset::word(0)), value.value0); // 1st word
    ASSERT_EQ(*(bs.begin() + irs::bitset::word(64)), value.value1); // 2nd word
    ASSERT_FALSE(bs.none());
    ASSERT_TRUE(bs.any());
    ASSERT_TRUE(bs.all());
  }
}
