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

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Cache/Metadata.h"

#include <stdint.h>
#include <memory>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CCacheMetadataSetup {
  CCacheMetadataSetup() { BOOST_TEST_MESSAGE("setup Metadata"); }

  ~CCacheMetadataSetup() { BOOST_TEST_MESSAGE("tear-down Metadata"); }
};
// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CCacheMetadataTest, CCacheMetadataSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor with valid data
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_constructor) {
  uint64_t dummy;
  std::shared_ptr<Cache> dummyCache(reinterpret_cast<Cache*>(&dummy),
                                    [](Cache* p) -> void {});
  uint8_t dummyTable;
  uint32_t logSize = 1;
  uint64_t limit = 1024;

  Metadata metadata(dummyCache, limit, &dummyTable, logSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test getters
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_getters) {
  uint64_t dummy;
  std::shared_ptr<Cache> dummyCache(reinterpret_cast<Cache*>(&dummy),
                                    [](Cache* p) -> void {});
  uint8_t dummyTable;
  uint32_t logSize = 1;
  uint64_t limit = 1024;

  Metadata metadata(dummyCache, limit, &dummyTable, logSize);

  metadata.lock();

  BOOST_CHECK(dummyCache == metadata.cache());

  BOOST_CHECK_EQUAL(logSize, metadata.logSize());
  BOOST_CHECK_EQUAL(0UL, metadata.auxiliaryLogSize());

  BOOST_CHECK_EQUAL(limit, metadata.softLimit());
  BOOST_CHECK_EQUAL(limit, metadata.hardLimit());
  BOOST_CHECK_EQUAL(0UL, metadata.usage());

  BOOST_CHECK(&dummyTable == metadata.table());
  BOOST_CHECK(nullptr == metadata.auxiliaryTable());

  metadata.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test usage limits
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_usage_limits) {
  uint64_t dummy;
  std::shared_ptr<Cache> dummyCache(reinterpret_cast<Cache*>(&dummy),
                                    [](Cache* p) -> void {});
  uint8_t dummyTable;
  uint32_t logSize = 1;
  bool success;

  Metadata metadata(dummyCache, 1024ULL, &dummyTable, logSize);

  metadata.lock();

  success = metadata.adjustUsageIfAllowed(512LL);
  BOOST_CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  BOOST_CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  BOOST_CHECK(!success);

  success = metadata.adjustLimits(2048ULL, 2048ULL);
  BOOST_CHECK(success);

  success = metadata.adjustUsageIfAllowed(1024LL);
  BOOST_CHECK(success);

  success = metadata.adjustLimits(1024ULL, 2048ULL);
  BOOST_CHECK(success);

  success = metadata.adjustUsageIfAllowed(512LL);
  BOOST_CHECK(!success);
  success = metadata.adjustUsageIfAllowed(-512LL);
  BOOST_CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  BOOST_CHECK(success);
  success = metadata.adjustUsageIfAllowed(-1024LL);
  BOOST_CHECK(success);
  success = metadata.adjustUsageIfAllowed(512LL);
  BOOST_CHECK(!success);

  success = metadata.adjustLimits(1024ULL, 1024ULL);
  BOOST_CHECK(success);
  success = metadata.adjustLimits(512ULL, 512ULL);
  BOOST_CHECK(!success);

  metadata.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test migration methods
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_migration) {
  uint64_t dummy;
  std::shared_ptr<Cache> dummyCache(reinterpret_cast<Cache*>(&dummy),
                                    [](Cache* p) -> void {});
  uint8_t dummyTable;
  uint8_t dummyAuxiliaryTable;
  uint32_t logSize = 1;
  uint32_t auxiliaryLogSize = 2;
  uint64_t limit = 1024;

  Metadata metadata(dummyCache, limit, &dummyTable, logSize);

  metadata.lock();

  metadata.grantAuxiliaryTable(&dummyAuxiliaryTable, auxiliaryLogSize);
  BOOST_CHECK_EQUAL(auxiliaryLogSize, metadata.auxiliaryLogSize());
  BOOST_CHECK(&dummyAuxiliaryTable == metadata.auxiliaryTable());

  metadata.swapTables();
  BOOST_CHECK_EQUAL(logSize, metadata.auxiliaryLogSize());
  BOOST_CHECK_EQUAL(auxiliaryLogSize, metadata.logSize());
  BOOST_CHECK(&dummyTable == metadata.auxiliaryTable());
  BOOST_CHECK(&dummyAuxiliaryTable == metadata.table());

  uint8_t* result = metadata.releaseAuxiliaryTable();
  BOOST_CHECK_EQUAL(0UL, metadata.auxiliaryLogSize());
  BOOST_CHECK(nullptr == metadata.auxiliaryTable());
  BOOST_CHECK(result == &dummyTable);

  result = metadata.releaseTable();
  BOOST_CHECK_EQUAL(0UL, metadata.logSize());
  BOOST_CHECK(nullptr == metadata.table());
  BOOST_CHECK(result == &dummyAuxiliaryTable);

  metadata.unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
