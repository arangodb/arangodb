////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/string.hpp"
#include "utils/std.hpp"
#include "utils/utf8_utils.hpp"

TEST(utf8_utils_test, next) {
  // ascii sequence
  {
    const irs::byte_type invalid = 0;
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("abcd"));
    const std::vector<uint32_t> expected = { 0x0061, 0x0062, 0x0063, 0x0064 };

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(); begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, &invalid);
        ASSERT_NE(&invalid, next);
        ASSERT_EQ(1, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::to_utf8(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }
  }

  // 2-bytes sequence
  {
    const irs::byte_type invalid = 0;
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"));
    const std::vector<uint32_t> expected = { 0x043F, 0x0440, 0x0438, 0x0432, 0x0435, 0x0442};

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(); begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, &invalid);
        ASSERT_NE(&invalid, next);
        ASSERT_EQ(2, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::to_utf8(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }
  }

  // 3-bytes sequence
  {
    const irs::byte_type invalid = 0;
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xE2\x9D\xA4"));
    const std::vector<uint32_t> expected = {
      0x2796, // heavy minus sign
      0x2764  // heavy black heart
    };

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(); begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, &invalid);
        ASSERT_NE(&invalid, next);
        ASSERT_EQ(3, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::to_utf8(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }
  }

  // 4-bytes sequence
  {
    const irs::byte_type invalid = 0;
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xF0\x9F\x98\x82"));
    const std::vector<uint32_t> expected = {
      0x1F601, // grinning face with smiling eyes
      0x1F602, // face with tears of joy
    };

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(); begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, &invalid);
        ASSERT_NE(&invalid, next);
        ASSERT_EQ(4, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::to_utf8(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }
  }
}
