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

#include "utils/utf8_utils.hpp"

#include "tests_shared.hpp"
#include "utils/std.hpp"
#include "utils/string.hpp"

TEST(utf8_utils_test, test) {
  // ascii sequence
  {
    const irs::bytes_view str =
      irs::ViewCast<irs::byte_type>(std::string_view("abcd"));
    const std::vector<uint32_t> expected = {0x0061, 0x0062, 0x0063, 0x0064};

    {
      auto expected_begin = expected.data();
      auto begin = str.data();
      auto end = (str.data() + str.size());
      for (; begin != (str.data() + str.size());) {
        auto next = irs::utf8_utils::Next(begin, end);
        ASSERT_EQ(1, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.data() + expected.size()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::ToChar32(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::ToUTF32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(
        irs::utf8_utils::ToUTF32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }
  }

  // 2-bytes sequence
  {
    const irs::bytes_view str = irs::ViewCast<irs::byte_type>(
      std::string_view("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"));
    const std::vector<uint32_t> expected = {0x043F, 0x0440, 0x0438,
                                            0x0432, 0x0435, 0x0442};

    {
      auto expected_begin = expected.data();
      auto begin = str.data();
      auto end = str.data() + str.size();
      for (; begin != str.data() + str.size();) {
        auto next = irs::utf8_utils::Next(begin, end);
        ASSERT_EQ(2, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.data() + expected.size()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::ToChar32(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::ToUTF32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(
        irs::utf8_utils::ToUTF32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }

    {
      size_t i = 0;
      auto begin = str.data();
      for (auto expected_value : expected) {
        irs::utf8_utils::ToChar32(begin);
        ++i;
      }
    }
  }

  // 3-bytes sequence
  {
    const irs::bytes_view str = irs::ViewCast<irs::byte_type>(
      std::string_view("\xE2\x9E\x96\xE2\x9D\xA4"));
    const std::vector<uint32_t> expected = {
      0x2796,  // heavy minus sign
      0x2764   // heavy black heart
    };

    {
      auto expected_begin = expected.data();
      auto begin = str.data();
      auto end = (str.data() + str.size());
      for (; begin != (str.data() + str.size());) {
        auto next = irs::utf8_utils::Next(begin, end);
        ASSERT_EQ(3, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.data() + expected.size()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::ToChar32(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::ToUTF32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(
        irs::utf8_utils::ToUTF32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }
  }

  // 4-bytes sequence
  {
    const irs::bytes_view str = irs::ViewCast<irs::byte_type>(
      std::string_view("\xF0\x9F\x98\x81\xF0\x9F\x98\x82"));
    const std::vector<uint32_t> expected = {
      0x1F601,  // grinning face with smiling eyes
      0x1F602,  // face with tears of joy
    };

    {
      auto expected_begin = expected.data();
      auto begin = str.data();
      auto end = (str.data() + str.size());
      for (; begin != (str.data() + str.size());) {
        auto next = irs::utf8_utils::Next(begin, end);
        ASSERT_EQ(4, std::distance(begin, next));
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
        ASSERT_EQ(begin, next);
        ++expected_begin;
        if (expected_begin == expected.data() + expected.size()) {
          ASSERT_EQ(end, next);
        } else {
          ASSERT_NE(end, next);
        }
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        ASSERT_EQ(*expected_begin, irs::utf8_utils::ToChar32(begin));
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      auto expected_begin = expected.data();
      for (auto begin = str.data(), end = (str.data() + str.size());
           begin != end; ++expected_begin) {
        const auto cp = irs::utf8_utils::ToChar32(begin, end);
        ASSERT_EQ(*expected_begin, cp);
      }
      ASSERT_EQ(expected.data() + expected.size(), expected_begin);
    }

    {
      std::vector<uint32_t> actual;
      irs::utf8_utils::ToUTF32<false>(str, irs::irstd::back_emplacer(actual));
      ASSERT_EQ(expected, actual);
    }

    {
      std::vector<uint32_t> actual;
      ASSERT_TRUE(
        irs::utf8_utils::ToUTF32<true>(str, irs::irstd::back_emplacer(actual)));
      ASSERT_EQ(expected, actual);
    }
  }
}

TEST(utf8_utils_test, find) {
  // null sequence
  {
    const auto str = irs::bytes_view{};
    ASSERT_EQ(0, irs::utf8_utils::Length(str));
  }

  // empty sequence
  {
    const auto str = irs::kEmptyStringView<irs::byte_type>;
    ASSERT_EQ(0, irs::utf8_utils::Length(str));
  }

  // 1-byte sequence
  {
    const irs::bytes_view str =
      irs::ViewCast<irs::byte_type>(std::string_view("abcd"));
    const std::vector<uint32_t> expected = {0x0061, 0x0062, 0x0063, 0x0064};

    ASSERT_EQ(expected.size(), irs::utf8_utils::Length(str));

    size_t i = 0;
    auto begin = str.data();
    for (auto expected_value : expected) {
      irs::utf8_utils::ToChar32(begin);
      ++i;
    }
  }

  // 2-byte sequence
  {
    const irs::bytes_view str = irs::ViewCast<irs::byte_type>(
      std::string_view("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"));
    const std::vector<uint32_t> expected = {0x043F, 0x0440, 0x0438,
                                            0x0432, 0x0435, 0x0442};

    ASSERT_EQ(expected.size(), irs::utf8_utils::Length(str));

    size_t i = 0;
    auto begin = str.data();
    for (auto expected_value : expected) {
      irs::utf8_utils::ToChar32(begin);
      ++i;
    }
  }

  // 3-byte sequence
  {
    const irs::bytes_view str = irs::ViewCast<irs::byte_type>(
      std::string_view("\xE2\x9E\x96\xE2\x9D\xA4"));
    const std::vector<uint32_t> expected = {
      0x2796,  // heavy minus sign
      0x2764   // heavy black heart
    };

    ASSERT_EQ(expected.size(), irs::utf8_utils::Length(str));

    size_t i = 0;
    auto begin = str.data();
    for (auto expected_value : expected) {
      irs::utf8_utils::ToChar32(begin);
      ++i;
    }
  }

  // 4-byte sequence
  {
    const irs::bytes_view str = irs::ViewCast<irs::byte_type>(
      std::string_view("\xF0\x9F\x98\x81\xF0\x9F\x98\x82"));
    const std::vector<uint32_t> expected = {
      0x1F601,  // grinning face with smiling eyes
      0x1F602,  // face with tears of joy
    };

    ASSERT_EQ(expected.size(), irs::utf8_utils::Length(str));

    size_t i = 0;
    auto begin = str.data();
    for (auto expected_value : expected) {
      irs::utf8_utils::ToChar32(begin);
      ++i;
    }
  }
}

TEST(utf8_utils_test, LengthFromChar32) {
  ASSERT_EQ(1, irs::utf8_utils::LengthFromChar32(0x7F));
  ASSERT_EQ(2, irs::utf8_utils::LengthFromChar32(0x7FF));
  ASSERT_EQ(3, irs::utf8_utils::LengthFromChar32(0xFFFF));
  ASSERT_EQ(4, irs::utf8_utils::LengthFromChar32(0x10000));

  // Intentionally don't handle this in `cp_length`
  ASSERT_EQ(3,
            irs::utf8_utils::LengthFromChar32(irs::utf8_utils::kInvalidChar32));
}

TEST(utf8_utils_test, FromChar32) {
  irs::byte_type buf[irs::utf8_utils::kMaxCharSize];

  // 1 byte
  {
    const uint32_t cp = 0x46;
    ASSERT_EQ(1, irs::utf8_utils::FromChar32(cp, buf));
    ASSERT_EQ(buf[0], cp);
  }

  // 2 bytes
  {
    const uint32_t cp = 0xA9;
    ASSERT_EQ(2, irs::utf8_utils::FromChar32(cp, buf));
    ASSERT_EQ(buf[0], 0xC2);
    ASSERT_EQ(buf[1], 0xA9);
  }

  // 3 bytes
  {
    const uint32_t cp = 0x08F1;
    ASSERT_EQ(3, irs::utf8_utils::FromChar32(cp, buf));
    ASSERT_EQ(buf[0], 0xE0);
    ASSERT_EQ(buf[1], 0xA3);
    ASSERT_EQ(buf[2], 0xB1);
  }

  // 4 bytes
  {
    const uint32_t cp = 0x1F996;
    ASSERT_EQ(4, irs::utf8_utils::FromChar32(cp, buf));
    ASSERT_EQ(buf[0], 0xF0);
    ASSERT_EQ(buf[1], 0x9F);
    ASSERT_EQ(buf[2], 0xA6);
    ASSERT_EQ(buf[3], 0x96);
  }
}
