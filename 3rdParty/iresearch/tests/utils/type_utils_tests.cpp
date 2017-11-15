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
#include "utils/type_utils.hpp"

TEST(type_utils_tests, is_convertible) {
  struct A { };
  struct B : A {};
  struct C {};

  ASSERT_FALSE((irs::is_convertible<A>()));
  ASSERT_FALSE((irs::is_convertible<A, void>()));
  ASSERT_TRUE((irs::is_convertible<void*, A*>()));
  ASSERT_TRUE((irs::is_convertible<A, B, C>()));
}

TEST(type_utils_tests, in_list) {
  struct A { };
  struct B : A {};
  struct C {};

  ASSERT_FALSE((irs::in_list<A>()));
  ASSERT_FALSE((irs::in_list<A, B, C>()));
  ASSERT_TRUE((irs::in_list<A, A, B, C>()));
}

TEST(type_utils_tests, template_traits) {
  // test count
  {
    ASSERT_EQ(0, (irs::template_traits_t<>::count()));
    ASSERT_EQ(1, (irs::template_traits_t<int32_t>::count()));
    ASSERT_EQ(2, (irs::template_traits_t<int32_t, bool>::count()));
    ASSERT_EQ(3, (irs::template_traits_t<int32_t, bool, int64_t>::count()));
    ASSERT_EQ(4, (irs::template_traits_t<int32_t, bool, int64_t, short>::count()));
  }

  // test size
  {
    ASSERT_EQ(0, (irs::template_traits_t<>::size()));
    ASSERT_EQ(4, (irs::template_traits_t<int32_t>::size()));
    ASSERT_EQ(5, (irs::template_traits_t<int32_t, bool>::size()));
    ASSERT_EQ(13, (irs::template_traits_t<int32_t, bool, int64_t>::size()));
    ASSERT_EQ(15, (irs::template_traits_t<int32_t, bool, int64_t, short>::size()));
  }

  // test size_aligned
  {
    ASSERT_EQ(0, (irs::template_traits_t<>::size_aligned()));
    ASSERT_EQ(4, (irs::template_traits_t<int32_t>::size_aligned()));
    ASSERT_EQ(5, (irs::template_traits_t<int32_t, bool>::size_aligned()));
    ASSERT_EQ(16, (irs::template_traits_t<int32_t, bool, int64_t>::size_aligned()));
    ASSERT_EQ(18, (irs::template_traits_t<int32_t, bool, int64_t, short>::size_aligned()));
  }

  // test size_aligned with offset
  {
    ASSERT_EQ(3, (irs::template_traits_t<>::size_aligned(3)));
    ASSERT_EQ(8, (irs::template_traits_t<int32_t>::size_aligned(3)));
    ASSERT_EQ(9, (irs::template_traits_t<int32_t, bool>::size_aligned(3)));
    ASSERT_EQ(24, (irs::template_traits_t<int32_t, bool, int64_t>::size_aligned(3)));
    ASSERT_EQ(26, (irs::template_traits_t<int32_t, bool, int64_t, short>::size_aligned(3)));
  }

  // test align_max
  {
    ASSERT_EQ(0, (irs::template_traits_t<>::align_max()));

    ASSERT_EQ(
      std::alignment_of<int32_t>::value,
      irs::template_traits_t<int32_t>::align_max()
    );

    ASSERT_EQ(
      std::max(std::alignment_of<int32_t>::value, std::alignment_of<bool>::value),
      (irs::template_traits_t<int32_t, bool>::size_max())
    );

    {
      static const size_t alignments[] {
        std::alignment_of<int32_t>::value,
        std::alignment_of<bool>::value,
        std::alignment_of<int64_t>::value
      };

      ASSERT_EQ(
        *std::max_element(std::begin(alignments), std::end(alignments)),
        (irs::template_traits_t<int32_t, bool, int64_t>::size_max())
      );
    }

    {
      static const size_t alignments[] {
        std::alignment_of<int32_t>::value,
        std::alignment_of<bool>::value,
        std::alignment_of<int64_t>::value,
        std::alignment_of<short>::value
      };

      ASSERT_EQ(
        *std::max_element(std::begin(alignments), std::end(alignments)),
        (irs::template_traits_t<int32_t, bool, int64_t, short>::size_max())
      );
    }
  }

  // test size_max
  {
    ASSERT_EQ(0, (irs::template_traits_t<>::size_max()));
    ASSERT_EQ(4, (irs::template_traits_t<int32_t>::size_max()));
    ASSERT_EQ(4, (irs::template_traits_t<int32_t, bool>::size_max()));
    ASSERT_EQ(8, (irs::template_traits_t<int32_t, bool, int64_t>::size_max()));
    ASSERT_EQ(8, (irs::template_traits_t<int32_t, bool, int64_t, short>::size_max()));
  }

  // test size_max_aligned
  {
    ASSERT_EQ(0, (irs::template_traits_t<>::size_max_aligned()));
    ASSERT_EQ(4, (irs::template_traits_t<int32_t>::size_max_aligned()));
    ASSERT_EQ(4, (irs::template_traits_t<int32_t, bool>::size_max_aligned()));
    ASSERT_EQ(8, (irs::template_traits_t<int32_t, bool, int64_t>::size_max_aligned()));
    ASSERT_EQ(8, (irs::template_traits_t<int32_t, bool, int64_t, short>::size_max_aligned()));
  }

  // test size_max_aligned with offset
  {
    ASSERT_EQ(3, (irs::template_traits_t<>::size_max_aligned(3)));
    ASSERT_EQ(8, (irs::template_traits_t<int32_t>::size_max_aligned(3)));
    ASSERT_EQ(8, (irs::template_traits_t<int32_t, bool>::size_max_aligned(3)));
    ASSERT_EQ(16, (irs::template_traits_t<int32_t, bool, int64_t>::size_max_aligned(3)));
    ASSERT_EQ(16, (irs::template_traits_t<int32_t, bool, int64_t, short>::size_max_aligned(3)));
  }
}
