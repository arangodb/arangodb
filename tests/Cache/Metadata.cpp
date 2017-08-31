////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::Metadata
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cache/Metadata.h"
#include "Basics/Common.h"
#include "Cache/PlainCache.h"
#include "Cache/Table.h"

#include "catch.hpp"

#include <stdint.h>
#include <memory>

using namespace arangodb::cache;

TEST_CASE("cache::Metadata", "[cache]") {
  SECTION("test basic constructor") {
    uint64_t usageLimit = 1024;
    uint64_t fixed = 128;
    uint64_t table = Table::allocationSize(Table::minLogSize);
    uint64_t max = UINT64_MAX;
    Metadata metadata(usageLimit, fixed, table, max);

    REQUIRE(metadata.fixedSize == fixed);
    REQUIRE(metadata.tableSize == table);
    REQUIRE(metadata.maxSize == max);
    REQUIRE(metadata.allocatedSize > (usageLimit + fixed + table));
    REQUIRE(metadata.deservedSize == metadata.allocatedSize);

    REQUIRE(metadata.usage == 0);
    REQUIRE(metadata.softUsageLimit == usageLimit);
    REQUIRE(metadata.hardUsageLimit == usageLimit);
  }

  SECTION("verify usage limits are adjusted and enforced correctly") {
    uint64_t overhead = 80;
    Metadata metadata(1024, 0, 0, 2048 + overhead);

    metadata.writeLock();

    REQUIRE(metadata.adjustUsageIfAllowed(512));
    REQUIRE(metadata.adjustUsageIfAllowed(512));
    REQUIRE(!metadata.adjustUsageIfAllowed(512));

    REQUIRE(!metadata.adjustLimits(2048, 2048));
    REQUIRE(metadata.allocatedSize == 1024 + overhead);
    REQUIRE(metadata.adjustDeserved(2048 + overhead));
    REQUIRE(metadata.adjustLimits(2048, 2048));
    REQUIRE(metadata.allocatedSize == 2048 + overhead);

    REQUIRE(metadata.adjustUsageIfAllowed(1024));

    REQUIRE(metadata.adjustLimits(1024, 2048));
    REQUIRE(metadata.allocatedSize == 2048 + overhead);

    REQUIRE(!metadata.adjustUsageIfAllowed(512));
    REQUIRE(metadata.adjustUsageIfAllowed(-512));
    REQUIRE(metadata.adjustUsageIfAllowed(512));
    REQUIRE(metadata.adjustUsageIfAllowed(-1024));
    REQUIRE(!metadata.adjustUsageIfAllowed(512));

    REQUIRE(metadata.adjustLimits(1024, 1024));
    REQUIRE(metadata.allocatedSize == 1024 + overhead);
    REQUIRE(!metadata.adjustLimits(512, 512));

    REQUIRE(!metadata.adjustLimits(2049, 2049));
    REQUIRE(metadata.allocatedSize == 1024 + overhead);

    metadata.writeUnlock();
  }

  SECTION("verify table methods work correctly") {
    uint64_t overhead = 80;
    Metadata metadata(1024, 0, 512, 2048 + overhead);

    metadata.writeLock();

    REQUIRE(!metadata.migrationAllowed(1024));
    REQUIRE(2048 + overhead == metadata.adjustDeserved(2048 + overhead));

    REQUIRE(metadata.migrationAllowed(1024));
    metadata.changeTable(1024);
    REQUIRE(metadata.tableSize == 1024);
    REQUIRE(metadata.allocatedSize == 2048 + overhead);

    REQUIRE(!metadata.migrationAllowed(1025));
    REQUIRE(metadata.migrationAllowed(512));
    metadata.changeTable(512);
    REQUIRE(metadata.tableSize == 512);
    REQUIRE(metadata.allocatedSize == 1536 + overhead);

    metadata.writeUnlock();
  }
}
