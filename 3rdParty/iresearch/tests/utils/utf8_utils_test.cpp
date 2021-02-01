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

TEST(utf8_utils_test, static_const) {
  static_assert(4 == irs::utf8_utils::MAX_CODE_POINT_SIZE);
  static_assert(0 == irs::utf8_utils::MIN_CODE_POINT);
  static_assert(0x10FFFF == irs::utf8_utils::MAX_CODE_POINT);
  static_assert(0x80 == irs::utf8_utils::MIN_2BYTES_CODE_POINT);
  static_assert(0x800 == irs::utf8_utils::MIN_3BYTES_CODE_POINT);
  static_assert(0x10000 == irs::utf8_utils::MIN_4BYTES_CODE_POINT);
  static_assert(std::numeric_limits<uint32_t>::max() == irs::utf8_utils::INVALID_CODE_POINT);
}

TEST(utf8_utils_test, test) {
  // ascii sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("abcd"));
    const std::vector<uint32_t> expected = { 0x0061, 0x0062, 0x0063, 0x0064 };

    {
      auto expected_begin = expected.begin();
      auto begin = str.begin();
      auto end = str.end();
      for (; begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, end);
        ASSERT_EQ(1, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.end()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
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
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::next_checked(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::utf8_to_utf32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(irs::utf8_utils::utf8_to_utf32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }
  }

  // 2-bytes sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"));
    const std::vector<uint32_t> expected = { 0x043F, 0x0440, 0x0438, 0x0432, 0x0435, 0x0442};

    {
      auto expected_begin = expected.begin();
      auto begin = str.begin();
      auto end = str.end();
      for (; begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, end);
        ASSERT_EQ(2, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.end()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
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
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::next_checked(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::utf8_to_utf32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(irs::utf8_utils::utf8_to_utf32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }

    {
      size_t i = 0;
      auto begin = str.begin();
      for (auto expected_value : expected) {
        ASSERT_EQ(i, irs::utf8_utils::find(str.begin(), str.size(), expected_value));
        ASSERT_EQ(begin, irs::utf8_utils::find(str.begin(), str.end(), expected_value));
        irs::utf8_utils::next(begin);
        ++i;
      }

      ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find(str.begin(), str.size(), 0x80));
      ASSERT_EQ(str.end(), irs::utf8_utils::find(str.begin(), str.end(), 0x80));
    }
  }

  // 3-bytes sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xE2\x9D\xA4"));
    const std::vector<uint32_t> expected = {
      0x2796, // heavy minus sign
      0x2764  // heavy black heart
    };

    {
      auto expected_begin = expected.begin();
      auto begin = str.begin();
      auto end = str.end();
      for (; begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, end);
        ASSERT_EQ(3, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.end()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
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
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::next_checked(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::utf8_to_utf32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(irs::utf8_utils::utf8_to_utf32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }
  }

  // 4-bytes sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xF0\x9F\x98\x82"));
    const std::vector<uint32_t> expected = {
      0x1F601, // grinning face with smiling eyes
      0x1F602, // face with tears of joy
    };

    {
      auto expected_begin = expected.begin();
      auto begin = str.begin();
      auto end = str.end();
      for (; begin != str.end(); ) {
        auto next = irs::utf8_utils::next(begin, end);
        ASSERT_EQ(4, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::next(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.end()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
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
      auto expected_begin = expected.begin();
      for (auto begin = str.begin(), end = str.end(); begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::next_checked(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.end(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::utf8_to_utf32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(irs::utf8_utils::utf8_to_utf32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }
  }
}

TEST(utf8_utils_test, find) {
  // null sequence
  {
    const auto str = irs::bytes_ref::NIL;
    ASSERT_EQ(0, irs::utf8_utils::utf8_length(str));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<true>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<false>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<true>(str.begin(), str.end(), 0x81));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<false>(str.begin(), str.end(), 0x81));
  }

  // empty sequence
  {
    const auto str = irs::bytes_ref::EMPTY;
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<true>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<false>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<true>(str.begin(), str.end(), 0x81));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<false>(str.begin(), str.end(), 0x81));
  }

  // 1-byte sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("abcd"));
    const std::vector<uint32_t> expected = { 0x0061, 0x0062, 0x0063, 0x0064 };

    ASSERT_EQ(expected.size(), irs::utf8_utils::utf8_length(str));

    size_t i = 0;
    auto begin = str.begin();
    for (auto expected_value : expected) {
      ASSERT_EQ(i, irs::utf8_utils::find<true>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(i, irs::utf8_utils::find<false>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<true>(str.begin(), str.end(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<false>(str.begin(), str.end(), expected_value));
      irs::utf8_utils::next(begin);
      ++i;
    }

    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<true>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<false>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<true>(str.begin(), str.end(), 0x81));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<false>(str.begin(), str.end(), 0x81));
  }

  // 2-byte sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"));
    const std::vector<uint32_t> expected = { 0x043F, 0x0440, 0x0438, 0x0432, 0x0435, 0x0442};

    ASSERT_EQ(expected.size(), irs::utf8_utils::utf8_length(str));

    size_t i = 0;
    auto begin = str.begin();
    for (auto expected_value : expected) {
      ASSERT_EQ(i, irs::utf8_utils::find<true>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(i, irs::utf8_utils::find<false>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<true>(str.begin(), str.end(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<false>(str.begin(), str.end(), expected_value));
      irs::utf8_utils::next(begin);
      ++i;
    }

    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<true>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<false>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<true>(str.begin(), str.end(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<false>(str.begin(), str.end(), 0x81));
  }

  // 3-byte sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xE2\x9D\xA4"));
    const std::vector<uint32_t> expected = {
      0x2796, // heavy minus sign
      0x2764  // heavy black heart
    };

    ASSERT_EQ(expected.size(), irs::utf8_utils::utf8_length(str));

    size_t i = 0;
    auto begin = str.begin();
    for (auto expected_value : expected) {
      ASSERT_EQ(i, irs::utf8_utils::find<true>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(i, irs::utf8_utils::find<false>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<true>(str.begin(), str.end(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<false>(str.begin(), str.end(), expected_value));
      irs::utf8_utils::next(begin);
      ++i;
    }

    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<true>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<false>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<true>(str.begin(), str.end(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<false>(str.begin(), str.end(), 0x81));
  }

  // 4-byte sequence
  {
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xF0\x9F\x98\x82"));
    const std::vector<uint32_t> expected = {
      0x1F601, // grinning face with smiling eyes
      0x1F602, // face with tears of joy
    };

    ASSERT_EQ(expected.size(), irs::utf8_utils::utf8_length(str));

    size_t i = 0;
    auto begin = str.begin();
    for (auto expected_value : expected) {
      ASSERT_EQ(i, irs::utf8_utils::find<true>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(i, irs::utf8_utils::find<false>(str.begin(), str.size(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<true>(str.begin(), str.end(), expected_value));
      ASSERT_EQ(begin, irs::utf8_utils::find<false>(str.begin(), str.end(), expected_value));
      irs::utf8_utils::next(begin);
      ++i;
    }

    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<true>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<false>(str.begin(), str.size(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<true>(str.begin(), str.end(), 0x80));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<false>(str.begin(), str.end(), 0x81));
  }

  // invalid 4-byte sequence
  {
    const auto expected_value = 128512;
    const irs::bytes_ref str = irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x0"));
    ASSERT_EQ(irs::bstring::npos, irs::utf8_utils::find<true>(str.begin(), str.size(), expected_value));
    ASSERT_EQ(0, irs::utf8_utils::find<false>(str.begin(), str.size(), expected_value));
    ASSERT_EQ(str.end(), irs::utf8_utils::find<true>(str.begin(), str.end(), expected_value));
    ASSERT_EQ(str.begin(), irs::utf8_utils::find<false>(str.begin(), str.end(), expected_value));
  }
}

TEST(utf8_utils_test, cp_length) {
  ASSERT_EQ(1, irs::utf8_utils::cp_length(80));
  ASSERT_EQ(2, irs::utf8_utils::cp_length(192));
  ASSERT_EQ(3, irs::utf8_utils::cp_length(224));
  ASSERT_EQ(4, irs::utf8_utils::cp_length(244));

  // invalid leading byte
  ASSERT_EQ(0, irs::utf8_utils::cp_length(128));
  ASSERT_EQ(0, irs::utf8_utils::cp_length(150));
}

TEST(utf8_utils_test, utf32_to_utf8) {
  irs::byte_type buf[irs::utf8_utils::MAX_CODE_POINT_SIZE];

  // 1 byte
  {
    const uint32_t cp = 0x46;
    ASSERT_EQ(1, irs::utf8_utils::utf32_to_utf8(cp, buf));
    ASSERT_EQ(buf[0], cp);
  }

  // 2 bytes
  {
    const uint32_t cp = 0xA9;
    ASSERT_EQ(2, irs::utf8_utils::utf32_to_utf8(cp, buf));
    ASSERT_EQ(buf[0], 0xC2);
    ASSERT_EQ(buf[1], 0xA9);
  }

  // 3 bytes
  {
    const uint32_t cp = 0x08F1;
    ASSERT_EQ(3, irs::utf8_utils::utf32_to_utf8(cp, buf));
    ASSERT_EQ(buf[0], 0xE0);
    ASSERT_EQ(buf[1], 0xA3);
    ASSERT_EQ(buf[2], 0xB1);
  }

  // 4 bytes
  {
    const uint32_t cp = 0x1F996;
    ASSERT_EQ(4, irs::utf8_utils::utf32_to_utf8(cp, buf));
    ASSERT_EQ(buf[0], 0xF0);
    ASSERT_EQ(buf[1], 0x9F);
    ASSERT_EQ(buf[2], 0xA6);
    ASSERT_EQ(buf[3], 0x96);
  }
}
