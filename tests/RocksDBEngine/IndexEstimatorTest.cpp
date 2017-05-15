////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for CuckooFilter based index selectivity estimator
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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


#include "Basics/Common.h"
#include "Basics/StringRef.h"
#include "catch.hpp"

#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------


// @brief setup

TEST_CASE("IndexEstimator", "[indexestimator]") {

// @brief Test insert unique correctness

SECTION("test_unique_values") {
  std::vector<uint64_t> toInsert(100);
  uint64_t i = 0;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  std::generate(toInsert.begin(), toInsert.end(), [&i] {return i++;});
  for (auto it : toInsert) {
    est.insert(it);
  }
  CHECK(est.nrUsed() == 100);
  CHECK(est.computeEstimate() == 1);

  for (size_t k = 0; k < 10; ++k) {
    est.remove(toInsert[k]);
  }
  CHECK(est.nrUsed() == 90);
  CHECK(est.computeEstimate() == 1);
}

SECTION("test_multiple_values") {
  std::vector<uint64_t> toInsert(100);
  uint64_t i = 0;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  std::generate(toInsert.begin(), toInsert.end(), [&i] {return (i++) % 10;});
  for (auto it : toInsert) {
    est.insert(it);
  }
  CHECK(est.nrUsed() == 10);
  CHECK(est.nrCuckood() == 0);
  CHECK(est.computeEstimate() == (double) 10 / 100);

  for (size_t k = 0; k < 10; ++k) {
    est.remove(toInsert[k]);
  }
  CHECK(est.nrCuckood() == 0);
  CHECK(est.computeEstimate() == (double) 10 / 90);
}

SECTION("test_serialize_deserialize") {
  std::vector<uint64_t> toInsert(10000);
  uint64_t i = 0;
  std::string serialization;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  std::generate(toInsert.begin(), toInsert.end(), [&i] {return i++;});
  for (auto it : toInsert) {
    est.insert(it);
  }

  est.serialize(serialization);

  // Test that the serialization first reports the correct length
  uint64_t length = serialization.size();
  
  // We read starting from the second char. The first char is reserved for the type
  uint64_t persLength = rocksutils::uint64FromPersistent(serialization.data() + 1);
  CHECK(persLength == length);

  // We first have an uint64_t representing the length.
  // This has to be extracted BEFORE initialisation.
  StringRef ref(serialization.data(), persLength);

  RocksDBCuckooIndexEstimator<uint64_t> copy(ref);

  // After serialisation => deserialisation
  // both estimates have to be identical
  CHECK(est.nrUsed() == copy.nrUsed());
  CHECK(est.nrCuckood() == copy.nrCuckood());
  CHECK(est.computeEstimate() == copy.computeEstimate());

  // Now let us remove the same elements in both
  bool coin = false;
  for (auto it : toInsert) {
    if (coin) {
      est.remove(it);
      copy.remove(it);
    }
    coin = !coin;
  }

  // We cannot relibly check inserts because the cuckoo has a random factor
  // Still all values have to be identical

  CHECK(est.nrUsed() == copy.nrUsed());
  CHECK(est.nrCuckood() == copy.nrCuckood());
  CHECK(est.computeEstimate() == copy.computeEstimate());
}

// @brief generate tests
}
