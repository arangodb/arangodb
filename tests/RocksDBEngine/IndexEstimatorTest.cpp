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
#include "RocksDBEngine/RocksDBCollectionMeta.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// @brief setup

TEST_CASE("IndexEstimator", "[rocksdb][indexestimator]") {
  // @brief Test insert unique correctness
  rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);

  SECTION("test_unique_values") {
    std::vector<uint64_t> toInsert(100);
    uint64_t i = 0;
    RocksDBCuckooIndexEstimator<uint64_t> est(2048);
    std::generate(toInsert.begin(), toInsert.end(), [&i] { return i++; });
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
    std::generate(toInsert.begin(), toInsert.end(),
                  [&i] { return (i++) % 10; });
    for (auto it : toInsert) {
      est.insert(it);
    }
    CHECK(est.nrUsed() == 10);
    CHECK(est.nrCuckood() == 0);
    CHECK(est.computeEstimate() == (double)10 / 100);

    for (size_t k = 0; k < 10; ++k) {
      est.remove(toInsert[k]);
    }
    CHECK(est.nrCuckood() == 0);
    CHECK(est.computeEstimate() == (double)10 / 90);
  }

  SECTION("test_serialize_deserialize") {
    std::vector<uint64_t> toInsert(10000);
    uint64_t i = 0;
    std::string serialization;
    RocksDBCuckooIndexEstimator<uint64_t> est(2048);
    std::generate(toInsert.begin(), toInsert.end(), [&i] { return i++; });
    for (auto it : toInsert) {
      est.insert(it);
    }

    uint64_t seq = 42;
    uint64_t applied = est.serialize(serialization, 1);
    CHECK(applied == 0);

    // Test that the serialization first reports the correct length
    uint64_t length = serialization.size() - 8;  // don't count the seq

    // We read starting from the 10th char. The first 8 are reserved for the
    // seq, and the ninth char is reserved for the type
    uint64_t persLength =
        rocksutils::uint64FromPersistent(serialization.data() + 9);
    CHECK(persLength == length);

    // We first have an uint64_t representing the length.
    // This has to be extracted BEFORE initialization.
    StringRef ref(serialization.data() + 8, persLength);

    RocksDBCuckooIndexEstimator<uint64_t> copy(seq, ref);

    // After serialization => deserialization
    // both estimates have to be identical
    CHECK(est.nrUsed() == copy.nrUsed());
    CHECK(est.nrCuckood() == copy.nrCuckood());
    CHECK(est.computeEstimate() == copy.computeEstimate());
    CHECK(seq == copy.committedSeq());

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

  SECTION("test_blocker_logic_basic") {
    rocksdb::SequenceNumber currentSeq(0);
    rocksdb::SequenceNumber expected = currentSeq;
    std::string serialization;
    RocksDBCuckooIndexEstimator<uint64_t> est(2048);
    RocksDBCollectionMeta meta;

    // test basic insertion buffering
    for (size_t iteration = 0; iteration < 10; iteration++) {
      uint64_t index = 0;
      std::vector<uint64_t> toInsert(10);
      std::vector<uint64_t> toRemove(0);
      std::generate(toInsert.begin(), toInsert.end(),
                    [&index] { return ++index; });
      expected = currentSeq; // only commit up to blocker
      auto res = meta.placeBlocker(iteration, ++currentSeq);
      REQUIRE(res.ok());
      est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));

      // make sure we don't apply yet
      auto actual = est.serialize(serialization, meta.committableSeq());
      serialization.clear();
      REQUIRE(actual == expected);
      REQUIRE((1.0 / std::max(1.0, static_cast<double>(iteration))) ==
              est.computeEstimate());

      meta.removeBlocker(iteration);

      // now make sure we apply it
      actual = est.serialize(serialization, meta.committableSeq());
      expected = currentSeq;
      serialization.clear();
      REQUIRE(actual == expected);
      REQUIRE((1.0 / std::max(1.0, static_cast<double>(iteration + 1))) ==
              est.computeEstimate());
      REQUIRE(est.committedSeq() == expected);
    }

    // test basic removal buffering
    for (size_t iteration = 0; iteration < 10; iteration++) {
      uint64_t index = 0;
      std::vector<uint64_t> toInsert(0);
      std::vector<uint64_t> toRemove(10);
      std::generate(toRemove.begin(), toRemove.end(),
                    [&index] { return ++index; });
      expected = currentSeq; // only commit up to blocker
      auto res = meta.placeBlocker(iteration, ++currentSeq);
      REQUIRE(res.ok());
      est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));

      // make sure we don't apply yet
      auto actual = est.serialize(serialization, meta.committableSeq());
      serialization.clear();
      REQUIRE(actual == expected);
      REQUIRE((1.0 / std::max(1.0, static_cast<double>(10 - iteration))) ==
              est.computeEstimate());

      meta.removeBlocker(iteration);

      // now make sure we apply it
      actual = est.serialize(serialization, meta.committableSeq());
      serialization.clear();
      expected = currentSeq;
      REQUIRE(actual == expected);
      REQUIRE(
          (1.0 / std::max(1.0, static_cast<double>(10 - (iteration + 1)))) ==
          est.computeEstimate());
      REQUIRE(est.committedSeq() == expected);
    }
  }

  SECTION("test_blocker_logic_overlapping") {
    rocksdb::SequenceNumber currentSeq(0);
    std::string serialization;
    RocksDBCuckooIndexEstimator<uint64_t> est(2048);
    RocksDBCollectionMeta meta;

    // test buffering with multiple blockers, but remove blockers in order
    for (size_t iteration = 0; iteration < 10; iteration++) {
      uint64_t index = 0;
      std::vector<uint64_t> toInsert(10);
      std::vector<uint64_t> toRemove(0);
      std::generate(toInsert.begin(), toInsert.end(),
                    [&index] { return ++index; });

      auto expected = currentSeq; // only commit up to blocker
      auto res = meta.placeBlocker(iteration, ++currentSeq);
      REQUIRE(res.ok());
      est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));

      // remove previous blocker
      meta.removeBlocker(iteration - 1);

      // now make sure we applied last batch, but not this one
      auto actual = est.serialize(serialization, meta.committableSeq());
      serialization.clear();
      REQUIRE(actual == expected);
      REQUIRE((1.0 / std::max(1.0, static_cast<double>(iteration))) ==
              est.computeEstimate());
    }
  }

  SECTION("test_blocker_logic_out_of_order") {
    rocksdb::SequenceNumber currentSeq(0);
    rocksdb::SequenceNumber expected;
    std::string serialization;
    RocksDBCuckooIndexEstimator<uint64_t> est(2048);
    RocksDBCollectionMeta meta;
    
    // test buffering where we keep around one old blocker
    for (size_t iteration = 0; iteration < 10; iteration++) {
      uint64_t index = 0;
      std::vector<uint64_t> toInsert(10);
      std::vector<uint64_t> toRemove(0);
      std::generate(toInsert.begin(), toInsert.end(),
                    [&index] { return ++index; });
      if (0 == iteration) {
        expected = currentSeq; // only commit up to blocker
      }
      auto res = meta.placeBlocker(iteration, ++currentSeq);
      REQUIRE(res.ok());
      est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));
      // remove only if not first blocker
      meta.removeBlocker(std::max(static_cast<size_t>(1), iteration));

      // now make sure we haven't applied anything
      auto actual = est.serialize(serialization, meta.committableSeq());
      serialization.clear();
      REQUIRE(actual == expected);
      REQUIRE(1.0 == est.computeEstimate());
    }

    // now remove first blocker and make sure we apply everything
    meta.removeBlocker(0);
    auto actual = est.serialize(serialization, meta.committableSeq());
    expected = currentSeq;
    serialization.clear();
    REQUIRE(actual == expected);
    REQUIRE(0.1 == est.computeEstimate());
  }

  // @brief generate tests
}
