//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_INDEX_TESTS_MISC_H
#define IRESEARCH_INDEX_TESTS_MISC_H

#include <algorithm>

NS_BEGIN( tests )

inline bool utf8_less(
    const iresearch::byte_type* lhs, size_t lhs_len,
    const iresearch::byte_type* rhs, size_t rhs_len
) {
  const auto len = (std::min)(lhs_len, rhs_len);

  for (size_t i = 0; i < len; ++i) {
    const uint8_t lhs_b = *lhs; ++lhs;
    const uint8_t rhs_b = *rhs; ++rhs;

    if ( lhs_b < rhs_b ) {
      return true;
    }

    if ( lhs_b > rhs_b ) {
      return false;
    }
  }

  return lhs_len < rhs_len;
}

NS_END

#endif
