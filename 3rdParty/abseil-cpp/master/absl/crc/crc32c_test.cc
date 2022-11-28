// Copyright 2022 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/crc/crc32c.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "gtest/gtest.h"
#include "absl/crc/internal/crc32c.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace {

TEST(CRC32C, RFC3720) {
  // Test the results of the vectors from
  // https://www.rfc-editor.org/rfc/rfc3720#appendix-B.4
  char data[32];

  // 32 bytes of ones.
  memset(data, 0, sizeof(data));
  EXPECT_EQ(absl::ComputeCrc32c(absl::string_view(data, sizeof(data))),
            absl::ToCrc32c(0x8a9136aa));

  // 32 bytes of ones.
  memset(data, 0xff, sizeof(data));
  EXPECT_EQ(absl::ComputeCrc32c(absl::string_view(data, sizeof(data))),
            absl::ToCrc32c(0x62a8ab43));

  // 32 incrementing bytes.
  for (int i = 0; i < 32; ++i) data[i] = static_cast<char>(i);
  EXPECT_EQ(absl::ComputeCrc32c(absl::string_view(data, sizeof(data))),
            absl::ToCrc32c(0x46dd794e));

  // 32 decrementing bytes.
  for (int i = 0; i < 32; ++i) data[i] = static_cast<char>(31 - i);
  EXPECT_EQ(absl::ComputeCrc32c(absl::string_view(data, sizeof(data))),
            absl::ToCrc32c(0x113fdb5c));

  // An iSCSI - SCSI Read (10) Command PDU.
  constexpr uint8_t cmd[48] = {
      0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18, 0x28, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_EQ(absl::ComputeCrc32c(absl::string_view(
                reinterpret_cast<const char*>(cmd), sizeof(cmd))),
            absl::ToCrc32c(0xd9963a56));
}

std::string TestString(size_t len) {
  std::string result;
  result.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    result.push_back(static_cast<char>(i % 256));
  }
  return result;
}

TEST(CRC32C, Compute) {
  EXPECT_EQ(absl::ComputeCrc32c(""), absl::ToCrc32c(0));
  EXPECT_EQ(absl::ComputeCrc32c("hello world"), absl::ToCrc32c(0xc99465aa));
}

TEST(CRC32C, Extend) {
  uint32_t base = 0xC99465AA;  // CRC32C of "Hello World"
  std::string extension = "Extension String";

  EXPECT_EQ(
      absl::ExtendCrc32c(absl::ToCrc32c(base), extension),
      absl::ToCrc32c(0xD2F65090));  // CRC32C of "Hello WorldExtension String"
}

TEST(CRC32C, ExtendByZeroes) {
  std::string base = "hello world";
  absl::crc32c_t base_crc = absl::ToCrc32c(0xc99465aa);

  for (const size_t extend_by : {100, 10000, 100000}) {
    SCOPED_TRACE(extend_by);
    absl::crc32c_t crc2 = absl::ExtendCrc32cByZeroes(base_crc, extend_by);
    EXPECT_EQ(crc2, absl::ComputeCrc32c(base + std::string(extend_by, '\0')));
  }
}

TEST(CRC32C, UnextendByZeroes) {
  for (auto seed_crc : {absl::ToCrc32c(0), absl::ToCrc32c(0xc99465aa)}) {
    SCOPED_TRACE(seed_crc);
    for (const size_t size_1 : {2, 200, 20000, 200000, 20000000}) {
      for (const size_t size_2 : {0, 100, 10000, 100000, 10000000}) {
        size_t extend_size = std::max(size_1, size_2);
        size_t unextend_size = std::min(size_1, size_2);
        SCOPED_TRACE(extend_size);
        SCOPED_TRACE(unextend_size);

        // Extending by A zeroes an unextending by B<A zeros should be identical
        // to extending by A-B zeroes.
        absl::crc32c_t crc1 = seed_crc;
        crc1 = absl::ExtendCrc32cByZeroes(crc1, extend_size);
        crc1 = absl::crc_internal::UnextendCrc32cByZeroes(crc1, unextend_size);

        absl::crc32c_t crc2 = seed_crc;
        crc2 = absl::ExtendCrc32cByZeroes(crc2, extend_size - unextend_size);

        EXPECT_EQ(crc1, crc2);
      }
    }
  }
  for (const size_t size : {0, 1, 100, 10000}) {
    SCOPED_TRACE(size);
    std::string string_before = TestString(size);
    std::string string_after = string_before + std::string(size, '\0');

    absl::crc32c_t crc_before = absl::ComputeCrc32c(string_before);
    absl::crc32c_t crc_after = absl::ComputeCrc32c(string_after);

    EXPECT_EQ(crc_before,
              absl::crc_internal::UnextendCrc32cByZeroes(crc_after, size));
  }
}

TEST(CRC32C, Concat) {
  std::string hello = "Hello, ";
  std::string world = "world!";
  std::string hello_world = absl::StrCat(hello, world);

  absl::crc32c_t crc_a = absl::ComputeCrc32c(hello);
  absl::crc32c_t crc_b = absl::ComputeCrc32c(world);
  absl::crc32c_t crc_ab = absl::ComputeCrc32c(hello_world);

  EXPECT_EQ(absl::ConcatCrc32c(crc_a, crc_b, world.size()), crc_ab);
}

TEST(CRC32C, Memcpy) {
  for (size_t bytes : {0, 1, 20, 500, 100000}) {
    SCOPED_TRACE(bytes);
    std::string sample_string = TestString(bytes);
    std::string target_buffer = std::string(bytes, '\0');

    absl::crc32c_t memcpy_crc =
        absl::MemcpyCrc32c(&(target_buffer[0]), sample_string.data(), bytes);
    absl::crc32c_t compute_crc = absl::ComputeCrc32c(sample_string);

    EXPECT_EQ(memcpy_crc, compute_crc);
    EXPECT_EQ(sample_string, target_buffer);
  }
}

TEST(CRC32C, RemovePrefix) {
  std::string hello = "Hello, ";
  std::string world = "world!";
  std::string hello_world = absl::StrCat(hello, world);

  absl::crc32c_t crc_a = absl::ComputeCrc32c(hello);
  absl::crc32c_t crc_b = absl::ComputeCrc32c(world);
  absl::crc32c_t crc_ab = absl::ComputeCrc32c(hello_world);

  EXPECT_EQ(absl::RemoveCrc32cPrefix(crc_a, crc_ab, world.size()), crc_b);
}

TEST(CRC32C, RemoveSuffix) {
  std::string hello = "Hello, ";
  std::string world = "world!";
  std::string hello_world = absl::StrCat(hello, world);

  absl::crc32c_t crc_a = absl::ComputeCrc32c(hello);
  absl::crc32c_t crc_b = absl::ComputeCrc32c(world);
  absl::crc32c_t crc_ab = absl::ComputeCrc32c(hello_world);

  EXPECT_EQ(absl::RemoveCrc32cSuffix(crc_ab, crc_b, world.size()), crc_a);
}
}  // namespace
