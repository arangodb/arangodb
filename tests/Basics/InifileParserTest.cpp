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

#include "catch.hpp"

#include <iomanip>
#include <sstream>

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/IniFileParser.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb;
using namespace arangodb::basics;

TEST_CASE("IniFileParserTest", "[ini]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test_parsing
////////////////////////////////////////////////////////////////////////////////

SECTION("test_parsing") {
  using namespace arangodb::options;

  uint64_t writeBufferSize = UINT64_MAX;
  uint64_t totalWriteBufferSize = UINT64_MAX;
  uint64_t maxWriteBufferNumber = UINT64_MAX;
  uint64_t maxTotalWalSize = UINT64_MAX;
  uint64_t blockCacheSize = UINT64_MAX;
  bool enforceBlockCacheSizeLimit = false;
  uint64_t cacheSize = UINT64_MAX;
  uint64_t nonoSetOption = UINT64_MAX;
  
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
)data";
  
  IniFileParser parser(&options);

  bool result = parser.parseContent("arangod.conf", contents, true);
  CHECK(true == result);
  CHECK(2048000U == writeBufferSize);;
  CHECK(536870912U == totalWriteBufferSize);;
  CHECK(4U == maxWriteBufferNumber);
  CHECK(1024000U == maxTotalWalSize);
  CHECK(268435456U == blockCacheSize);
  CHECK(true == enforceBlockCacheSizeLimit);
  CHECK(268435456U == cacheSize);
  CHECK(UINT64_MAX == nonoSetOption);
}

}
