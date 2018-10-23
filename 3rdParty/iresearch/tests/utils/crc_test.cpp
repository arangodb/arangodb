////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#ifdef IRESEARCH_SSE4_2

#include "utils/crc.hpp"

#include <fstream>

#if defined(_MSC_VER)
  #pragma warning(disable : 4244)
  #pragma warning(disable : 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

#include <boost/crc.hpp>

#if defined(_MSC_VER)
  #pragma warning(default: 4244)
  #pragma warning(default: 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

TEST(crc_test, check) {
  typedef boost::crc_optimal<32, 0x1EDC6F41, 0, 0, true, true> crc32c_expected;

  irs::crc32c crc;
  crc32c_expected crc_expected;
  ASSERT_EQ(crc.checksum(), crc_expected.checksum());

  char buf[65536];

  std::fstream stream(test_base::resource("simple_two_column.csv"));
  ASSERT_FALSE(!stream);

  while (stream) {
    stream.read(buf, sizeof buf);
    const auto read = stream.gcount();
    crc.process_bytes(reinterpret_cast<const void*>(buf), read);
    crc_expected.process_bytes(reinterpret_cast<const void*>(buf), read);
  }

  ASSERT_EQ(crc.checksum(), crc_expected.checksum());
}

#endif
