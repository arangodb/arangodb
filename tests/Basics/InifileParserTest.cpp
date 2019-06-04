////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for StringUtils class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "ProgramOptions/IniFileParser.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb;
using namespace arangodb::basics;

TEST(InifileParserTest, test_options) {
  using namespace arangodb::options;

  uint64_t writeBufferSize = UINT64_MAX;
  uint64_t totalWriteBufferSize = UINT64_MAX;
  uint64_t maxWriteBufferNumber = UINT64_MAX;
  uint64_t maxTotalWalSize = UINT64_MAX;
  uint64_t blockCacheSize = UINT64_MAX;
  bool enforceBlockCacheSizeLimit = false;
  uint64_t cacheSize = UINT64_MAX;
  uint64_t nonoSetOption = UINT64_MAX;
  uint64_t someValueUsingSuffixes = UINT64_MAX;
  uint64_t someOtherValueUsingSuffixes = UINT64_MAX;
  uint64_t yetSomeOtherValueUsingSuffixes = UINT64_MAX;
  uint64_t andAnotherValueUsingSuffixes = UINT64_MAX;
  uint64_t andFinallySomeGb = UINT64_MAX;
  uint64_t aValueWithAnInlineComment = UINT64_MAX;
  double aDouble = -2.0;
  double aDoubleWithAComment = -2.0;
  double aDoubleNotSet = -2.0;
  
  ProgramOptions options("testi", "testi [options]", "bla", "/tmp/bla");
  options.addSection("rocksdb", "bla");
  options.addOption("--rocksdb.write-buffer-size", "bla", new UInt64Parameter(&writeBufferSize));
  options.addOption("--rocksdb.total-write-buffer-size", "bla", new UInt64Parameter(&totalWriteBufferSize));
  options.addOption("--rocksdb.max-write-buffer-number", "bla", new UInt64Parameter(&maxWriteBufferNumber));
  options.addOption("--rocksdb.max-total-wal-size", "bla", new UInt64Parameter(&maxTotalWalSize));
  options.addOption("--rocksdb.block-cache-size", "bla", new UInt64Parameter(&blockCacheSize));
  options.addOption("--rocksdb.enforce-block-cache-size-limit", "bla", new BooleanParameter(&enforceBlockCacheSizeLimit));
  
  options.addSection("cache", "bla");
  options.addOption("--cache.size", "bla", new UInt64Parameter(&cacheSize));
  options.addOption("--cache.nono-set-option", "bla", new UInt64Parameter(&nonoSetOption));
  
  options.addSection("pork", "bla");
  options.addOption("--pork.some-value-using-suffixes", "bla", new UInt64Parameter(&someValueUsingSuffixes));
  options.addOption("--pork.some-other-value-using-suffixes", "bla", new UInt64Parameter(&someOtherValueUsingSuffixes));
  options.addOption("--pork.yet-some-other-value-using-suffixes", "bla", new UInt64Parameter(&yetSomeOtherValueUsingSuffixes));
  options.addOption("--pork.and-another-value-using-suffixes", "bla", new UInt64Parameter(&andAnotherValueUsingSuffixes));
  options.addOption("--pork.and-finally-some-gb", "bla", new UInt64Parameter(&andFinallySomeGb));
  options.addOption("--pork.a-value-with-an-inline-comment", "bla", new UInt64Parameter(&aValueWithAnInlineComment));
  options.addOption("--pork.a-double", "bla", new DoubleParameter(&aDouble));
  options.addOption("--pork.a-double-with-a-comment", "bla", new DoubleParameter(&aDoubleWithAComment));
  options.addOption("--pork.a-double-not-set", "bla", new DoubleParameter(&aDoubleNotSet));

  auto contents = R"data(
[rocksdb]
# Write buffers
write-buffer-size = 2048000 # 2M
total-write-buffer-size = 536870912
max-write-buffer-number = 4
max-total-wal-size = 1024000 # 1M

# Read buffers
block-cache-size = 268435456
enforce-block-cache-size-limit = true

[cache]
size = 268435456 # 256M

[pork]
some-value-using-suffixes = 1M
some-other-value-using-suffixes = 1MiB
yet-some-other-value-using-suffixes = 12MB  
   and-another-value-using-suffixes = 256kb  
   and-finally-some-gb = 256GB
a-value-with-an-inline-comment = 12345#1234M
a-double = 335.25
a-double-with-a-comment = 2948.434#343
)data";
  
  IniFileParser parser(&options);

  bool result = parser.parseContent("arangod.conf", contents, true);
  ASSERT_TRUE(result);
  ASSERT_EQ(2048000U, writeBufferSize);
  ASSERT_EQ(536870912U, totalWriteBufferSize);
  ASSERT_EQ(4U, maxWriteBufferNumber);
  ASSERT_EQ(1024000U, maxTotalWalSize);
  ASSERT_EQ(268435456U, blockCacheSize);
  ASSERT_TRUE(enforceBlockCacheSizeLimit);
  
  ASSERT_EQ(268435456U, cacheSize);
  ASSERT_EQ(UINT64_MAX, nonoSetOption);
  
  ASSERT_EQ(1000000U, someValueUsingSuffixes);
  ASSERT_EQ(1048576U, someOtherValueUsingSuffixes);
  ASSERT_EQ(12000000U, yetSomeOtherValueUsingSuffixes);
  ASSERT_EQ(256000U, andAnotherValueUsingSuffixes);
  ASSERT_EQ(256000000000U, andFinallySomeGb);
  ASSERT_EQ(12345U, aValueWithAnInlineComment);

  ASSERT_FLOAT_EQ(335.25, aDouble);
  ASSERT_FLOAT_EQ(2948.434, aDoubleWithAComment);
  ASSERT_FLOAT_EQ(-2.0, aDoubleNotSet);
}
