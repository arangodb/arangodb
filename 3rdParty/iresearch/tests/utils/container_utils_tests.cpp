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

#include "gtest/gtest.h"
#include "utils/container_utils.hpp"

TEST(container_utils_tests, test_bucket_allocator) {
  static bool WAS_MADE{};
  static size_t ACTUAL_SIZE{};

  struct bucket_builder {
    typedef std::shared_ptr<irs::byte_type> ptr;

    static ptr make(size_t size) {
      ACTUAL_SIZE = size;
      WAS_MADE = true;

      // starting from gcc6 it's impossible to instanciate
      // std::shared_ptr<T> from std::unique_ptr<T[]>
      auto buf = irs::memory::make_unique<irs::byte_type[]>(size);
      auto p = std::shared_ptr<irs::byte_type>(
        buf.get(), std::default_delete<irs::byte_type[]>()
      );
      buf.release();
      return p;
    }
  };

  irs::container_utils::memory::bucket_allocator<bucket_builder, 16> alloc(
    1   // how many buckets to cache for each level
  );

  typedef irs::container_utils::raw_block_vector<
    16, // total number of levels
    8,  // size of the first block 2^8
    decltype(alloc) // allocator
  > raw_block_vector_t;

  static_assert(
    16 == raw_block_vector_t::NUM_BUCKETS,
    "invalid number of buckets"
  );

  static_assert(
    256 == raw_block_vector_t::FIRST_BUCKET_SIZE,
    "invalid size of the first bucket"
  );

  raw_block_vector_t blocks(alloc);

  // just created
  ASSERT_TRUE(blocks.empty());
  ASSERT_EQ(0, blocks.buffer_count());

  // first bucket
  ACTUAL_SIZE = 0;
  WAS_MADE = false;
  blocks.push_buffer();
  ASSERT_TRUE(WAS_MADE); // buffer was actually built by builder
  ASSERT_EQ(256, ACTUAL_SIZE); // first bucket 2^8
  ASSERT_TRUE(1 == blocks.buffer_count());
  ASSERT_TRUE(!blocks.empty());
  blocks.pop_buffer(); // pop recently pushed buffer
  ASSERT_TRUE(blocks.empty());
  ASSERT_TRUE(0 == blocks.buffer_count());
  ACTUAL_SIZE = 0;
  WAS_MADE = false;
  blocks.push_buffer();
  ASSERT_FALSE(WAS_MADE); // buffer has been reused from the pool
  ASSERT_EQ(0, ACTUAL_SIZE); // didn't change
  ASSERT_TRUE(1 == blocks.buffer_count());
  ASSERT_TRUE(!blocks.empty());

  // second bukcet
  ACTUAL_SIZE = 0;
  WAS_MADE = false;
  blocks.push_buffer();
  ASSERT_TRUE(WAS_MADE); // buffer was actually built by builder
  ASSERT_EQ(512, ACTUAL_SIZE); // first bucket 2^9
  ASSERT_TRUE(2 == blocks.buffer_count());
  ASSERT_TRUE(!blocks.empty());
  blocks.pop_buffer(); // pop recently pushed buffer
  ASSERT_TRUE(!blocks.empty());
  ASSERT_TRUE(1 == blocks.buffer_count());
  ACTUAL_SIZE = 0;
  WAS_MADE = false;
  blocks.push_buffer();
  ASSERT_FALSE(WAS_MADE); // buffer has been reused from the pool
  ASSERT_EQ(0, ACTUAL_SIZE); // didn't change
  ASSERT_TRUE(2 == blocks.buffer_count());
  ASSERT_TRUE(!blocks.empty());

  // cleanup
  blocks.clear();
  ASSERT_TRUE(blocks.empty());
  ASSERT_EQ(0, blocks.buffer_count());
}

TEST(container_utils_tests, test_compute_bucket_meta) {
  // test meta for num buckets == 0, skip bits == 0
  {
    auto& meta = irs::container_utils::bucket_meta<0, 0>::get();
    ASSERT_EQ(0, meta.size());
  }

  // test meta for num buckets == 1, skip bits == 0
  {
    auto& meta = irs::container_utils::bucket_meta<1, 0>::get();
    ASSERT_EQ(1, meta.size());
    ASSERT_EQ(&(meta[0]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(1, meta[0].size);
  }

  // test meta for num buckets == 2, skip bits == 0
  {
    auto& meta = irs::container_utils::bucket_meta<2, 0>::get();
    ASSERT_EQ(2, meta.size());
    ASSERT_EQ(&(meta[1]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(1, meta[0].size);
    ASSERT_EQ(&(meta[1]), meta[1].next);
    ASSERT_EQ(1, meta[1].offset);
    ASSERT_EQ(2, meta[1].size);
  }

  // test meta for num buckets == 3, skip bits == 0
  {
    auto& meta = irs::container_utils::bucket_meta<3, 0>::get();
    ASSERT_EQ(3, meta.size());
    ASSERT_EQ(&(meta[1]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(1, meta[0].size);
    ASSERT_EQ(&(meta[2]), meta[1].next);
    ASSERT_EQ(1, meta[1].offset);
    ASSERT_EQ(2, meta[1].size);
    ASSERT_EQ(&(meta[2]), meta[2].next);
    ASSERT_EQ(3, meta[2].offset);
    ASSERT_EQ(4, meta[2].size);
  }

  // test meta for num buckets == 0, skip bits == 2
  {
    auto& meta = irs::container_utils::bucket_meta<0, 2>::get();
    ASSERT_EQ(0, meta.size());
  }

  // test meta for num buckets == 1, skip bits == 2
  {
    auto& meta = irs::container_utils::bucket_meta<1, 2>::get();
    ASSERT_EQ(1, meta.size());
    ASSERT_EQ(&(meta[0]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(4, meta[0].size);
  }

  // test meta for num buckets == 2, skip bits == 2
  {
    auto& meta = irs::container_utils::bucket_meta<2, 2>::get();
    ASSERT_EQ(2, meta.size());
    ASSERT_EQ(&(meta[1]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(4, meta[0].size);
    ASSERT_EQ(&(meta[1]), meta[1].next);
    ASSERT_EQ(4, meta[1].offset);
    ASSERT_EQ(8, meta[1].size);
  }

  // test meta for num buckets == 3, skip bits == 2
  {
    auto& meta = irs::container_utils::bucket_meta<3, 2>::get();
    ASSERT_EQ(3, meta.size());
    ASSERT_EQ(&(meta[1]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(4, meta[0].size);
    ASSERT_EQ(&(meta[2]), meta[1].next);
    ASSERT_EQ(4, meta[1].offset);
    ASSERT_EQ(8, meta[1].size);
    ASSERT_EQ(&(meta[2]), meta[2].next);
    ASSERT_EQ(12, meta[2].offset);
    ASSERT_EQ(16, meta[2].size);
  }
}

TEST(container_utils_tests, compute_bucket_offset) {
  // test boundaries for skip bits == 0
  {
     ASSERT_EQ(0, (iresearch::container_utils::compute_bucket_offset<0>(0)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<0>(1)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<0>(2)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<0>(3)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<0>(4)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<0>(5)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<0>(6)));
     ASSERT_EQ(3, (iresearch::container_utils::compute_bucket_offset<0>(7)));
  }

  // test boundaries for skip bits == 2
  {
     ASSERT_EQ(0, (iresearch::container_utils::compute_bucket_offset<2>(0)));
     ASSERT_EQ(0, (iresearch::container_utils::compute_bucket_offset<2>(1)));
     ASSERT_EQ(0, (iresearch::container_utils::compute_bucket_offset<2>(2)));
     ASSERT_EQ(0, (iresearch::container_utils::compute_bucket_offset<2>(3)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(4)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(5)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(6)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(7)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(8)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(9)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(10)));
     ASSERT_EQ(1, (iresearch::container_utils::compute_bucket_offset<2>(11)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(12)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(13)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(14)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(15)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(16)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(17)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(18)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(19)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(20)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(21)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(22)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(23)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(24)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(25)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(26)));
     ASSERT_EQ(2, (iresearch::container_utils::compute_bucket_offset<2>(27)));
     ASSERT_EQ(3, (iresearch::container_utils::compute_bucket_offset<2>(28)));
  }
}

TEST(container_utils_array_tests, check_alignment) {
  typedef irs::container_utils::array<size_t, 5> array_size_t;

  static_assert(
    alignof(array_size_t) == alignof(size_t),
    "wrong data alignment"
  );

  typedef irs::container_utils::array<std::max_align_t, 5> array_aligned_max_t;

  static_assert(
    alignof(array_aligned_max_t) == alignof(std::max_align_t),
    "wrong data alignment"
  );

  struct alignas(16) aligned16 { char c; };
  typedef irs::container_utils::array<aligned16, 5> array_aligned16_t;

  static_assert(
    alignof(array_aligned16_t) == alignof(aligned16),
    "wrong data alignment"
  );

  struct alignas(32) aligned32 { char c; };
  typedef irs::container_utils::array<aligned32, 5> array_aligned32_t;

  static_assert(
    alignof(array_aligned32_t) == alignof(aligned32),
    "wrong data alignment"
  );
}

TEST(container_utils_array_tests, construct) {
  static size_t DEFAULT_CTOR;
  static size_t NON_DEFAULT_CTOR;

  struct object {
    object() {
      ++DEFAULT_CTOR;
    }

    explicit object(int i) noexcept : i(i) {
      ++NON_DEFAULT_CTOR;
    }

    int i{3};
  };

  // empty array
  {
    DEFAULT_CTOR = 0;
    NON_DEFAULT_CTOR = 0;

    irs::container_utils::array<object, 0> objects;

    static_assert(
      irs::container_utils::array<object, 0>::SIZE == 0,
      "invalid array size"
    );

    ASSERT_EQ(0, objects.size());
    ASSERT_TRUE(objects.empty());
    ASSERT_EQ(0, DEFAULT_CTOR);
    ASSERT_EQ(0, NON_DEFAULT_CTOR);
    ASSERT_EQ(objects.begin(), objects.end());
    ASSERT_EQ(objects.rbegin(), objects.rend());
  }

  // non empty array, default constructor
  {
    DEFAULT_CTOR = 0;
    NON_DEFAULT_CTOR = 0;

    irs::container_utils::array<object, 2> objects;

    static_assert(
      irs::container_utils::array<object, 2>::SIZE == 2,
      "invalid array size"
    );

    ASSERT_EQ(2, objects.size());
    ASSERT_FALSE(objects.empty());
    ASSERT_EQ(objects.size(), DEFAULT_CTOR);
    ASSERT_EQ(0, NON_DEFAULT_CTOR);
    ASSERT_EQ(objects.begin() + objects.size(), objects.end());
    ASSERT_EQ(objects.rbegin() + objects.size(), objects.rend());
    ASSERT_EQ(3, objects.front().i);
    ASSERT_EQ(3, objects.back().i);
    ASSERT_EQ(objects.begin(), &objects.front());
    ASSERT_EQ(objects.end()-1, &objects.back());
    ASSERT_EQ(&objects[0], &objects.front());
    ASSERT_EQ(&objects[1], &objects.back());
  }

  // non empty array
  {
    DEFAULT_CTOR = 0;
    NON_DEFAULT_CTOR = 0;

    const irs::container_utils::array<object, 2> objects(5);

    static_assert(
      irs::container_utils::array<object, 2>::SIZE == 2,
      "invalid array size"
    );

    ASSERT_EQ(2, objects.size());
    ASSERT_FALSE(objects.empty());
    ASSERT_EQ(0, DEFAULT_CTOR);
    ASSERT_EQ(objects.size(), NON_DEFAULT_CTOR);
    ASSERT_EQ(objects.begin() + objects.size(), objects.end());
    ASSERT_EQ(objects.rbegin() + objects.size(), objects.rend());
    ASSERT_EQ(5, objects.front().i);
    ASSERT_EQ(5, objects.back().i);
    ASSERT_EQ(objects.begin(), &objects.front());
    ASSERT_EQ(objects.end()-1, &objects.back());
    ASSERT_EQ(&objects[0], &objects.front());
    ASSERT_EQ(&objects[1], &objects.back());
  }
}
