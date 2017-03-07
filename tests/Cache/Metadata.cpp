////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::Metadata
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 triagens GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "Cache/Metadata.h"

#include <stdint.h>
#include <memory>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCacheMetadataTest", "[cache]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor with valid data
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_constructor") {
  uint64_t limit = 1024;
  Metadata metadata(limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test getters
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_getters") {
  uint64_t dummy;
  std::shared_ptr<Cache> dummyCache(reinterpret_cast<Cache*>(&dummy),
                                    [](Cache* p) -> void {});
  uint64_t limit = 1024;

  Metadata metadata(limit);
  metadata.link(dummyCache);

  metadata.lock();

  CHECK(dummyCache == metadata.cache());

  CHECK(limit == metadata.softLimit());
  CHECK(limit == metadata.hardLimit());
  CHECK(0UL == metadata.usage());

  metadata.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage limits
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_usage_limits") {
  bool success;

  Metadata metadata(1024ULL);

  metadata.lock();

  success = metadata.adjustUsageIfAllowed(512LL);
  CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  CHECK(!success);

  success = metadata.adjustLimits(2048ULL, 2048ULL);
  CHECK(success);

  success = metadata.adjustUsageIfAllowed(1024LL);
  CHECK(success);

  success = metadata.adjustLimits(1024ULL, 2048ULL);
  CHECK(success);

  success = metadata.adjustUsageIfAllowed(512LL);
  CHECK(!success);
  success = metadata.adjustUsageIfAllowed(-512LL);
  CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  CHECK(success);
  success = metadata.adjustUsageIfAllowed(-1024LL);
  CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  CHECK(!success);

  success = metadata.adjustLimits(1024ULL, 1024ULL);
  CHECK(success);
  success = metadata.adjustLimits(512ULL, 512ULL);
  CHECK(!success);

  metadata.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test migration methods
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_migration") {
  uint8_t dummyTable;
  uint8_t dummyAuxiliaryTable;
  uint32_t logSize = 1;
  uint32_t auxiliaryLogSize = 2;
  uint64_t limit = 1024;

  Metadata metadata(limit);

  metadata.lock();

  metadata.grantAuxiliaryTable(&dummyTable, logSize);
  metadata.swapTables();

  metadata.grantAuxiliaryTable(&dummyAuxiliaryTable, auxiliaryLogSize);
  CHECK(auxiliaryLogSize == metadata.auxiliaryLogSize());
  CHECK(&dummyAuxiliaryTable == metadata.auxiliaryTable());

  metadata.swapTables();
  CHECK(logSize == metadata.auxiliaryLogSize());
  CHECK(auxiliaryLogSize == metadata.logSize());
  CHECK(&dummyTable == metadata.auxiliaryTable());
  CHECK(&dummyAuxiliaryTable == metadata.table());

  uint8_t* result = metadata.releaseAuxiliaryTable();
  CHECK(0UL == metadata.auxiliaryLogSize());
  CHECK(nullptr == metadata.auxiliaryTable());
  CHECK(result == &dummyTable);

  result = metadata.releaseTable();
  CHECK(0UL == metadata.logSize());
  CHECK(nullptr == metadata.table());
  CHECK(result == &dummyAuxiliaryTable);

  metadata.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
