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
#include "utils/string.hpp"
#include "utils/string_utils.hpp"
#include "utils/block_pool.hpp"
#include "utils/misc.hpp"

using namespace iresearch;

template<typename T, size_t BlockSize>
class block_pool_test : public test_base {
 public:
  typedef block_pool<T, BlockSize> pool_t;
  typedef typename pool_t::block_type block_type;
  typedef typename pool_t::inserter inserter_t;
  typedef typename pool_t::sliced_inserter sliced_inserter_t;
  typedef typename pool_t::sliced_reader sliced_reader_t;
  typedef typename pool_t::sliced_greedy_inserter sliced_greedy_inserter_t;
  typedef typename pool_t::sliced_greedy_reader sliced_greedy_reader_t;

  void ctor() {
    ASSERT_EQ(BlockSize, size_t(block_type::SIZE));
    ASSERT_EQ(0, pool_.block_count());
    ASSERT_EQ(0, pool_.size());
    ASSERT_EQ(0, pool_.begin().pool_offset());
    ASSERT_EQ(0, pool_.end().pool_offset());
    ASSERT_EQ(pool_.begin(), pool_.end());

    // check const iterators
    {
      const auto& cpool = pool_;
      ASSERT_EQ(cpool.begin(), cpool.end());
    }
  }

  void next_buffer_clear() {
    ASSERT_EQ(BlockSize, size_t(block_type::SIZE));
    ASSERT_EQ(0, pool_.block_count());
    ASSERT_EQ(0, pool_.value_count());
    ASSERT_EQ( pool_.begin(), pool_.end());

    // add buffer
    pool_.alloc_buffer();
    ASSERT_EQ(1, pool_.block_count());
    ASSERT_EQ(BlockSize, pool_.value_count());
    ASSERT_NE(pool_.begin(), pool_.end());

    // add several buffers
    const size_t count = 15;
    pool_.alloc_buffer(count);
    ASSERT_EQ(1 + count, pool_.block_count());
    ASSERT_EQ(BlockSize *(1 + count), pool_.value_count());
    ASSERT_NE( pool_.begin(), pool_.end());

    // clear buffers
    pool_.clear();
    ASSERT_EQ(0, pool_.block_count());
    ASSERT_EQ(0, pool_.value_count());
    ASSERT_EQ(pool_.begin(), pool_.end());
  }

  void write_read(uint32_t max, uint32_t step) {
    inserter_t w(pool_.begin());

    for (uint32_t i = 0; max; i += step, --max) {
      irs::vwrite<uint32_t>(w, i);
    }

    auto it = pool_.begin();

    for (uint32_t i = 0; max; i += step, --max) {
      ASSERT_EQ(i, irs::vread<uint32_t>(it));
    }
  }

  void sliced_write_read(uint32_t max, uint32_t step) {
    inserter_t ins( pool_.begin() ); 
    ins.alloc_slice();

    sliced_inserter_t w(ins, pool_.begin());

    const char* const payload = "payload";
    const auto len = static_cast<uint32_t>(strlen(payload)); // only 8 chars
    std::string slice_data("test_data");

    int count = max;
    for (uint32_t i = 0; count; i += step, --count) {
      irs::vwrite<uint32_t>(w, i);

      if (i % 3 == 0) { // write data within slice
        w.write(reinterpret_cast<const byte_type*>(slice_data.c_str()), slice_data.size());
      }

      if ( i % 2 ) {
        ins.write((const irs::byte_type*)payload, len);
      }
    }

    sliced_reader_t r(pool_, 0, w.pool_offset());

    count = max;
    for (uint32_t i = 0; count; i += step, --count) {
      int res = irs::vread<uint32_t>(r);

      EXPECT_EQ(i, res);

      if (i % 3 == 0) { // read data within slice
        bstring payload;
        size_t size = r.read(&(string_utils::oversize(payload, slice_data.size())[0]), slice_data.size());
        EXPECT_TRUE(slice_data.size() == size);
        EXPECT_TRUE(memcmp(slice_data.c_str(), payload.data(), std::min(slice_data.size(), size)) == 0);
      }
    }
  }

  void slice_chunked_read_write() {
    decltype(pool_.begin().pool_offset()) slice_chain_begin;
    decltype(slice_chain_begin) slice_chain_end;

    bytes_ref data0 = ref_cast<byte_type>(string_ref("first_payload"));
    bytes_ref data1 = ref_cast<byte_type>(string_ref("second_payload_1234"));

    // write data
    {
      inserter_t ins(pool_.begin());
      // alloc slice chain
      slice_chain_begin = ins.alloc_slice();

      // insert payload
      {
        T payload[500];
        std::fill(payload, payload + sizeof payload, 20);
        ins.write(payload, sizeof payload);
      }

      sliced_inserter_t sliced_ins(ins, slice_chain_begin);
      sliced_ins.write(data0.c_str(), data0.size()); // fill 1st slice & alloc 2nd slice here
      sliced_ins.write(data1.c_str(), data1.size()); // fill 2st slice & alloc 3nd slice here

      slice_chain_end = sliced_ins.pool_offset();
    }

    // read data
    {
      
      sliced_reader_t sliced_rdr(pool_, slice_chain_begin, slice_chain_end);
      
      // read first data
      {
        byte_type read[100]{};
        sliced_rdr.read(read, data0.size());
        ASSERT_EQ(data0, bytes_ref(read, data0.size()));
      }
      
      // read second data
      {
        byte_type read[100]{};
        sliced_rdr.read(read, data1.size());
        ASSERT_EQ(data1, bytes_ref(read, data1.size()));
      }
    }
  }

  void alloc_slice() {
    const size_t offset = 5;

    // check slice format
    for (size_t i = 0; i < irs::detail::LEVEL_MAX; ++i) {
      pool_.alloc_buffer();
      inserter_t writer(offset + pool_.begin());
      ASSERT_EQ(offset, writer.pool_offset());
      auto p = writer.alloc_slice(i);
      ASSERT_EQ(offset, p);
      ASSERT_EQ(offset + irs::detail::LEVELS[i].size, writer.pool_offset());
      auto begin = pool_.seek(p);
      for (size_t j = 0; j < irs::detail::LEVELS[i].size-1; ++j) {
        ASSERT_EQ(0, *begin);
        ++begin;
      }
      ASSERT_EQ(irs::detail::LEVELS[i].next, *begin);
      pool_.clear();
    }
  }

  void slice_between_blocks() {
    // add initial block
    pool_.alloc_buffer();

    decltype(pool_.begin().pool_offset()) slice_chain_begin;
    decltype(slice_chain_begin) slice_chain_end;

    // write phase
    {
      // seek to the 1 item before the end of the first block
      auto begin = pool_.seek(BlockSize - 1); // begin of the slice chain
      inserter_t ins(begin);

      // we align slices the way they never overcome block boundaries
      slice_chain_begin = ins.alloc_slice();
      ASSERT_EQ(BlockSize, slice_chain_begin);
      ASSERT_EQ(BlockSize + irs::detail::LEVELS[0].size, ins.pool_offset());

      // slice should be initialized
      ASSERT_TRUE(
          std::all_of(pool_.seek(BlockSize),
                      pool_.seek(BlockSize + irs::detail::LEVELS[0].size - 1),
                      [](T val) { return 0 == val; })
          );

      sliced_inserter_t sliced_ins(ins, slice_chain_begin);

      // write single value 
      sliced_ins = 5;

      // insert payload
      {
        T payload[BlockSize-5];
        std::fill(payload, payload + sizeof payload, 20);
        ins.write(payload, sizeof payload);
      }

      // write additional values
      sliced_ins = 6; // will be moved to the 2nd slice
      sliced_ins = 7; // will be moved to the 2nd slice
      sliced_ins = 8; // will be moved to the 2nd slice
      sliced_ins = 9; // here new block will be allocated and value will be written in 2nd slice
      ASSERT_EQ(2*BlockSize+3+1, sliced_ins.pool_offset());

      slice_chain_end = sliced_ins.pool_offset();
    }

    // read phase
    {
      sliced_reader_t sliced_rdr(pool_, slice_chain_begin, slice_chain_end);
      ASSERT_EQ(5, *sliced_rdr); ++sliced_rdr;
      ASSERT_FALSE(sliced_rdr.eof());
      ASSERT_EQ(6, *sliced_rdr); ++sliced_rdr;
      ASSERT_FALSE(sliced_rdr.eof());
      ASSERT_EQ(7, *sliced_rdr); ++sliced_rdr;
      ASSERT_FALSE(sliced_rdr.eof());
      ASSERT_EQ(8, *sliced_rdr); ++sliced_rdr;
      ASSERT_FALSE(sliced_rdr.eof());
      ASSERT_EQ(9, *sliced_rdr); ++sliced_rdr;
      ASSERT_TRUE(sliced_rdr.eof());
      ASSERT_EQ(slice_chain_end, sliced_rdr.pool_offset());
    }
  }

  void alloc_greedy_slice() {
    const size_t offset = 5;

    // check slice format
    // level 0 makes no sense for greedy format
    for (size_t i = 1; i < irs::detail::LEVEL_MAX; ++i) {
      pool_.alloc_buffer();
      inserter_t writer(offset + pool_.begin());
      ASSERT_EQ(offset, writer.pool_offset());
      auto p = writer.alloc_greedy_slice(i);
      ASSERT_EQ(offset, p);
      ASSERT_EQ(offset + irs::detail::LEVELS[i].size, writer.pool_offset());
      auto begin = pool_.seek(p);
      ASSERT_EQ(i, *begin); ++begin; // slice header
      for (size_t j = 0; j < irs::detail::LEVELS[i].size-sizeof(uint32_t)-1; ++j) {
        ASSERT_EQ(0, *begin);
        ++begin;
      }
      // address part
      ASSERT_EQ(irs::detail::LEVELS[i].next, *begin); ++begin;
      ASSERT_EQ(0, *begin); ++begin;
      ASSERT_EQ(0, *begin); ++begin;
      ASSERT_EQ(0, *begin);
      pool_.clear();
    }
  }

  void greedy_slice_read_write() {
    const bytes_ref data[] {
      ref_cast<byte_type>(string_ref("first_payload")),
      ref_cast<byte_type>(string_ref("second_payload_1234"))
    };

    std::vector<std::pair<size_t, size_t>> cookies; // slice_offset + offset

    auto push_cookie = [&cookies](const sliced_greedy_inserter_t& writer) {
      cookies.emplace_back(writer.slice_offset(), writer.pool_offset() - writer.slice_offset());
    };

    // write data
    {
      inserter_t ins(pool_.begin());
      cookies.emplace_back(ins.alloc_greedy_slice(), 1); // alloc slice chain

      // insert payload
      {
        T payload[500];
        std::fill(payload, payload + sizeof payload, 20);
        ins.write(payload, sizeof payload);
      }

      sliced_greedy_inserter_t sliced_ins(ins, cookies.back().second, cookies.back().first);
      sliced_ins.write(data[0].c_str(), data[0].size()); // fill 1st slice & alloc 2nd slice here
      push_cookie(sliced_ins);
      sliced_ins.write(data[1].c_str(), data[1].size()); // fill 2st slice & alloc 3nd slice here

      // insert payload
      {
        T payload[500];
        std::fill(payload, payload + sizeof payload, 20);
        ins.write(payload, sizeof payload);
      }

      for (size_t i = 0; i < 1024; ++i) {
        push_cookie(sliced_ins);
        irs::vwrite(sliced_ins, i);
        const auto str = std::to_string(i);
        sliced_ins.write(reinterpret_cast<const irs::byte_type*>(str.c_str()), str.size());
      }
    }

    // read data
    {
      byte_type read[100]{};
      size_t i = cookies.size();

      while (--i > 2) {
        const auto expected = i - 2;
        sliced_greedy_reader_t sliced_rdr(pool_, cookies[i].first, cookies[i].second);
        ASSERT_EQ(expected, irs::vread<size_t>(sliced_rdr));

        const auto str = std::to_string(expected);
        sliced_rdr.read(read, str.size());

        ASSERT_EQ(
          irs::bytes_ref(reinterpret_cast<const byte_type*>(str.c_str()), str.size()),
          irs::bytes_ref(read, str.size())
        );
      }

      while (i--) {
        sliced_greedy_reader_t sliced_rdr(pool_, cookies[i].first, cookies[i].second);
        sliced_rdr.read(read, data[i].size());
        ASSERT_EQ(data[i], bytes_ref(read, data[i].size()));
      }
    }
  }

  void greedy_slice_between_blocks() {
    // add initial block
    pool_.alloc_buffer();

    decltype(pool_.begin().pool_offset()) slice_chain_begin;
    decltype(slice_chain_begin) slice_chain_end;

    // write phase
    {
      // seek to the 1 item before the end of the first block
      auto begin = pool_.seek(BlockSize - 1); // begin of the slice chain
      inserter_t ins(begin);

      // we align slices the way they never overcome block boundaries
      slice_chain_begin = ins.alloc_greedy_slice();
      ASSERT_EQ(BlockSize, slice_chain_begin);
      ASSERT_EQ(BlockSize + irs::detail::LEVELS[1].size, ins.pool_offset()); // level 0 makes no sense for greedy slice

      // slice should be initialized
      ASSERT_TRUE(
          std::all_of(pool_.seek(BlockSize + 1),
                      pool_.seek(BlockSize + irs::detail::LEVELS[1].size - sizeof(uint32_t) - 1),
                      [](T val) { return 0 == val; })
          );

      sliced_greedy_inserter_t sliced_ins(ins, slice_chain_begin, 1);

      // write single value
      sliced_ins = 5;

      // insert payload
      {
        T payload[BlockSize-5];
        std::fill(payload, payload + sizeof payload, 20);
        ins.write(payload, sizeof payload);
      }

      // write additional values
      sliced_ins = 6; // will be moved to the 2nd slice
      sliced_ins = 7; // will be moved to the 2nd slice
      sliced_ins = 8; // will be moved to the 2nd slice
      sliced_ins = 9; // will be moved to the 2nd slice
      sliced_ins = 10; // will be moved to the 2nd slice
      sliced_ins = 11; // will be moved to the 2nd slice
      sliced_ins = 12; // will be moved to the 2nd slice
      sliced_ins = 13; // will be moved to the 2nd slice
      ASSERT_EQ(2*BlockSize+9+1, sliced_ins.pool_offset());

      slice_chain_end = sliced_ins.pool_offset();
    }

    // read phase
    {
      sliced_greedy_reader_t sliced_rdr(pool_, slice_chain_begin, 1);
      ASSERT_EQ(5, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(6, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(7, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(8, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(9, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(10, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(11, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(12, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(13, *sliced_rdr); ++sliced_rdr;
      ASSERT_EQ(slice_chain_end, sliced_rdr.pool_offset());
    }
  }

 protected:
  irs::block_pool<T, BlockSize> pool_;
}; // block_pool_test

typedef block_pool_test<irs::byte_type, 32768> byte_block_pool_test;

TEST_F(byte_block_pool_test, ctor) {
  ctor();
}

TEST_F(byte_block_pool_test, next_buffer_clear) {
  byte_block_pool_test::next_buffer_clear();
}

TEST_F(byte_block_pool_test, write_read) {
  byte_block_pool_test::write_read(1000000, 7);
}

TEST_F(byte_block_pool_test, sliced_write_read) {
  byte_block_pool_test::sliced_write_read(1000000, 7);
}

TEST_F(byte_block_pool_test, slice_alignment) {
  slice_between_blocks();     
}

TEST_F(byte_block_pool_test, slice_alignment_with_reuse) {
  slice_between_blocks();
  slice_between_blocks(); // reuse block_pool from previous run
}

TEST_F(byte_block_pool_test, alloc_slice) {
  alloc_slice();
}

TEST_F(byte_block_pool_test, slice_chunked_read_write) {
  slice_chunked_read_write();
}

TEST_F(byte_block_pool_test, alloc_greedy_slice) {
  alloc_greedy_slice();
}

TEST_F(byte_block_pool_test, greedy_slice_chunked_read_write) {
  greedy_slice_read_write();
}

TEST_F(byte_block_pool_test, greedy_slice_alignment) {
  greedy_slice_between_blocks();
}

TEST_F(byte_block_pool_test, greedy_slice_alignment_with_reuse) {
  greedy_slice_between_blocks();
  greedy_slice_between_blocks(); // reuse block_pool from previous run
}
