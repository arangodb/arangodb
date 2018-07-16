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

namespace tests {
  class container_utils_tests: public ::testing::Test {
    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };
}

using namespace tests;

TEST_F(container_utils_tests, test_compute_bucket_meta) {
  // test meta for num buckets == 0, skip bits == 0
  {
    auto meta = iresearch::container_utils::compute_bucket_meta<0, 0>();
    ASSERT_EQ(0, meta.size());
  }

  // test meta for num buckets == 1, skip bits == 0
  {
    auto meta = iresearch::container_utils::compute_bucket_meta<1, 0>();
    ASSERT_EQ(1, meta.size());
    ASSERT_EQ(&(meta[0]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(1, meta[0].size);
  }

  // test meta for num buckets == 2, skip bits == 0
  {
    auto meta = iresearch::container_utils::compute_bucket_meta<2, 0>();
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
    auto meta = iresearch::container_utils::compute_bucket_meta<3, 0>();
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
    auto meta = iresearch::container_utils::compute_bucket_meta<0, 2>();
    ASSERT_EQ(0, meta.size());
  }

  // test meta for num buckets == 1, skip bits == 2
  {
    auto meta = iresearch::container_utils::compute_bucket_meta<1, 2>();
    ASSERT_EQ(1, meta.size());
    ASSERT_EQ(&(meta[0]), meta[0].next);
    ASSERT_EQ(0, meta[0].offset);
    ASSERT_EQ(4, meta[0].size);
  }

  // test meta for num buckets == 2, skip bits == 2
  {
    auto meta = iresearch::container_utils::compute_bucket_meta<2, 2>();
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
    auto meta = iresearch::container_utils::compute_bucket_meta<3, 2>();
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

TEST_F(container_utils_tests, compute_bucket_offset) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
