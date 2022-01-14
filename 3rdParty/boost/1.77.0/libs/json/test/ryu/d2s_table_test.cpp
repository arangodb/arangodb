// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

/*
    This is a derivative work
*/

#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER) && !defined(RYU_ONLY_64_BIT_OPS)
#define BOOST_JSON_RYU_HAS_UINT128
#elif defined(_MSC_VER) && !defined(RYU_ONLY_64_BIT_OPS) && defined(_M_X64) \
  && !defined(__clang__) // https://bugs.llvm.org/show_bug.cgi?id=37755
#define BOOST_JSON_RYU_HAS_64_BIT_INTRINSICS
#endif

// We want to test the size-optimized case here.
#if !defined(BOOST_JSON_RYU_OPTIMIZE_SIZE)
#define BOOST_JSON_RYU_OPTIMIZE_SIZE
#endif
#include <boost/json/detail/ryu/detail/d2s.hpp>
#include <boost/json/detail/ryu/detail/d2s_full_table.hpp>
#include <array>
#include <cstdint>
#include <cmath>
#include "gtest.hpp"

BOOST_JSON_NS_BEGIN
namespace detail {

namespace ryu {
namespace detail {

TEST(D2sTableTest, double_computePow5) {
  for (int i = 0; i < 326; i++) {
    uint64_t m[2];
    double_computePow5(i, m);
    ASSERT_EQ(m[0], DOUBLE_POW5_SPLIT()[i][0]);
    ASSERT_EQ(m[1], DOUBLE_POW5_SPLIT()[i][1]);
  }
}

TEST(D2sTableTest, compute_offsets_for_double_computePow5) {
  uint32_t totalErrors = 0;
  uint32_t offsets[13] = {0};
  for (int i = 0; i < 326; i++) {
    uint64_t m[2];
    double_computePow5(i, m);
    if (m[0] != DOUBLE_POW5_SPLIT()[i][0]) {
      offsets[i / POW5_TABLE_SIZE] |= 1 << (i % POW5_TABLE_SIZE);
      totalErrors++;
    }
  }
  if (totalErrors != 0) {
    for (int i = 0; i < 13; i++) {
      printf("0x%08x,\n", offsets[i]);
    }
  }
  ASSERT_EQ(totalErrors, 0);
}

TEST(D2sTableTest, double_computeInvPow5) {
  for (int i = 0; i < 292; i++) {
    uint64_t m[2];
    double_computeInvPow5(i, m);
    ASSERT_EQ(m[0], DOUBLE_POW5_INV_SPLIT()[i][0]);
    ASSERT_EQ(m[1], DOUBLE_POW5_INV_SPLIT()[i][1]);
  }
}

TEST(D2sTableTest, compute_offsets_for_double_computeInvPow5) {
  uint32_t totalErrors = 0;
  uint32_t offsets[20] = {0};
  for (int i = 0; i < 292; i++) {
    uint64_t m[2];
    double_computeInvPow5(i, m);
    if (m[0] != DOUBLE_POW5_INV_SPLIT()[i][0]) {
      offsets[i / 16] |= ((DOUBLE_POW5_INV_SPLIT()[i][0] - m[0]) & 3) << ((i % 16) << 1);
      totalErrors++;
    }
  }
  if (totalErrors != 0) {
    for (int i = 0; i < 20; i++) {
      printf("0x%08x,\n", offsets[i]);
    }
  }
  ASSERT_EQ(totalErrors, 0);
}

} // detail
} // ryu

} // detail
BOOST_JSON_NS_END
