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
