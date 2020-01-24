////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for CuckooFilter based index selectivity estimator
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018-2019, ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein, Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "gtest/gtest.h"

#include "RocksDBEngine/RocksDBMetadata.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <velocypack/StringRef.h>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// @brief setup

class IndexEstimatorTest : public ::testing::Test {
 protected:
  IndexEstimatorTest() {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);
  }
};

TEST_F(IndexEstimatorTest, test_unique_values) {
  std::vector<uint64_t> toInsert(100);
  uint64_t i = 0;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  std::generate(toInsert.begin(), toInsert.end(), [&i] { return i++; });
  for (auto it : toInsert) {
    est.insert(it);
  }
  EXPECT_EQ(est.nrUsed(), 100);
  EXPECT_EQ(est.computeEstimate(), 1);

  for (size_t k = 0; k < 10; ++k) {
    est.remove(toInsert[k]);
  }
  EXPECT_EQ(est.nrUsed(), 90);
  EXPECT_EQ(est.computeEstimate(), 1);
}

TEST_F(IndexEstimatorTest, test_multiple_values) {
  std::vector<uint64_t> toInsert(100);
  uint64_t i = 0;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  std::generate(toInsert.begin(), toInsert.end(), [&i] { return (i++) % 10; });
  for (auto it : toInsert) {
    est.insert(it);
  }
  EXPECT_EQ(est.nrUsed(), 10);
  EXPECT_EQ(est.nrCuckood(), 0);
  EXPECT_EQ(est.computeEstimate(), (double)10 / 100);

  for (size_t k = 0; k < 10; ++k) {
    est.remove(toInsert[k]);
  }
  EXPECT_EQ(est.nrCuckood(), 0);
  EXPECT_EQ(est.computeEstimate(), (double)10 / 90);
}

TEST_F(IndexEstimatorTest, test_serialize_deserialize) {
  std::vector<uint64_t> toInsert(10000);
  uint64_t i = 0;
  std::string serialization;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  std::generate(toInsert.begin(), toInsert.end(), [&i] { return i++; });
  for (auto it : toInsert) {
    est.insert(it);
  }

  uint64_t seq = 42;
  est.setAppliedSeq(seq);
  est.serialize(serialization, seq);

  // Test that the serialization first reports the correct length
  uint64_t length = serialization.size() - 8;  // don't count the seq

  // We read starting from the 10th char. The first 8 are reserved for the
  // seq, and the ninth char is reserved for the type
  uint64_t persLength = rocksutils::uint64FromPersistent(serialization.data() + 9);
  EXPECT_EQ(persLength, length);

  // We first have an uint64_t representing the length.
  // This has to be extracted BEFORE initialization.
  arangodb::velocypack::StringRef ref(serialization.data(), persLength + 8);
  RocksDBCuckooIndexEstimator<uint64_t> copy(ref);

  // After serialization => deserialization
  // both estimates have to be identical
  EXPECT_EQ(est.nrUsed(), copy.nrUsed());
  EXPECT_EQ(est.nrCuckood(), copy.nrCuckood());
  EXPECT_EQ(est.computeEstimate(), copy.computeEstimate());
  EXPECT_EQ(seq, copy.appliedSeq());

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

  EXPECT_EQ(est.nrUsed(), copy.nrUsed());
  EXPECT_EQ(est.nrCuckood(), copy.nrCuckood());
  EXPECT_EQ(est.computeEstimate(), copy.computeEstimate());
}

TEST_F(IndexEstimatorTest, test_blocker_logic_basic) {
  rocksdb::SequenceNumber currentSeq(0);
  rocksdb::SequenceNumber expected = currentSeq;
  std::string serialization;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  RocksDBMetadata meta;

  // test basic insertion buffering
  for (size_t iteration = 0; iteration < 10; iteration++) {
    uint64_t index = 0;
    std::vector<uint64_t> toInsert(10);
    std::vector<uint64_t> toRemove(0);
    std::generate(toInsert.begin(), toInsert.end(), [&index] { return ++index; });
    expected = currentSeq;  // only commit up to blocker
    auto res = meta.placeBlocker(iteration, ++currentSeq);
    ASSERT_TRUE(res.ok());
    est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));

    // make sure we don't apply yet
    est.serialize(serialization, meta.committableSeq(UINT64_MAX));
    serialization.clear();
    ASSERT_EQ(est.appliedSeq(), expected);
    ASSERT_EQ(1.0 / std::max(1.0, static_cast<double>(iteration)), est.computeEstimate());

    meta.removeBlocker(iteration);
    EXPECT_EQ(meta.committableSeq(UINT64_MAX), UINT64_MAX);

    // now make sure we apply it
    est.serialize(serialization, currentSeq);
    expected = currentSeq;
    serialization.clear();
    ASSERT_EQ(est.appliedSeq(), expected);
    ASSERT_TRUE((1.0 / std::max(1.0, static_cast<double>(iteration + 1))) ==
            est.computeEstimate());
  }

  // test basic removal buffering
  for (size_t iteration = 0; iteration < 10; iteration++) {
    uint64_t index = 0;
    std::vector<uint64_t> toInsert(0);
    std::vector<uint64_t> toRemove(10);
    std::generate(toRemove.begin(), toRemove.end(), [&index] { return ++index; });
    expected = currentSeq;  // only commit up to blocker
    auto res = meta.placeBlocker(iteration, ++currentSeq);
    ASSERT_TRUE(res.ok());
    est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));

    // make sure we don't apply yet
    ASSERT_EQ(meta.committableSeq(UINT64_MAX), expected + 1);
    est.serialize(serialization, meta.committableSeq(UINT64_MAX));
    serialization.clear();
    ASSERT_EQ(est.appliedSeq(), expected);
    ASSERT_TRUE((1.0 / std::max(1.0, static_cast<double>(10 - iteration))) ==
            est.computeEstimate());

    meta.removeBlocker(iteration);

    // now make sure we apply it
    est.serialize(serialization, meta.committableSeq(UINT64_MAX));
    serialization.clear();
    expected = currentSeq;
    ASSERT_EQ(est.appliedSeq(), expected);
    ASSERT_TRUE((1.0 / std::max(1.0, static_cast<double>(10 - (iteration + 1)))) ==
            est.computeEstimate());
    ASSERT_EQ(est.appliedSeq(), expected);
  }
}

TEST_F(IndexEstimatorTest, test_blocker_logic_overlapping) {
  rocksdb::SequenceNumber currentSeq(0);
  std::string serialization;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  RocksDBMetadata meta;

  // test buffering with multiple blockers, but remove blockers in order
  for (size_t iteration = 0; iteration < 10; iteration++) {
    uint64_t index = 0;
    std::vector<uint64_t> toInsert(10);
    std::vector<uint64_t> toRemove(0);
    std::generate(toInsert.begin(), toInsert.end(), [&index] { return ++index; });

    auto expected = currentSeq;  // only commit up to blocker
    auto res = meta.placeBlocker(iteration, ++currentSeq);
    ASSERT_TRUE(res.ok());
    est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));

    // remove previous blocker
    meta.removeBlocker(iteration - 1);

    // now make sure we applied last batch, but not this one
    est.serialize(serialization, meta.committableSeq(UINT64_MAX));
    serialization.clear();
    ASSERT_EQ(est.appliedSeq(), expected);
    ASSERT_EQ(1.0 / std::max(1.0, static_cast<double>(iteration)), est.computeEstimate());
  }
}

TEST_F(IndexEstimatorTest, test_blocker_logic_out_of_order) {
  rocksdb::SequenceNumber currentSeq(0);
  rocksdb::SequenceNumber expected(0);
  std::string serialization;
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  RocksDBMetadata meta;

  // test buffering where we keep around one old blocker
  for (size_t iteration = 0; iteration < 10; iteration++) {
    uint64_t index = 0;
    std::vector<uint64_t> toInsert(10);
    std::vector<uint64_t> toRemove(0);
    std::generate(toInsert.begin(), toInsert.end(), [&index] { return ++index; });
    if (0 == iteration) {
      expected = currentSeq;  // only commit up to blocker
    }
    auto res = meta.placeBlocker(iteration, ++currentSeq);
    ASSERT_TRUE(res.ok());
    est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));
    // remove only if not first blocker
    meta.removeBlocker(std::max(static_cast<size_t>(1), iteration));

    // now make sure we haven't applied anything
    est.serialize(serialization, meta.committableSeq(UINT64_MAX));
    serialization.clear();
    ASSERT_EQ(est.appliedSeq(), expected);
    ASSERT_EQ(1.0, est.computeEstimate());
  }

  // now remove first blocker and make sure we apply everything
  meta.removeBlocker(0);
  est.serialize(serialization, meta.committableSeq(UINT64_MAX));
  expected = currentSeq;
  serialization.clear();
  ASSERT_EQ(est.appliedSeq(), expected);
  ASSERT_EQ(0.1, est.computeEstimate());
}

TEST_F(IndexEstimatorTest, test_truncate_logic) {
  rocksdb::SequenceNumber currentSeq(0);
  rocksdb::SequenceNumber expected(0);
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  RocksDBMetadata meta;

  // test buffering where we keep around one old blocker
  for (size_t iteration = 0; iteration < 10; iteration++) {
    uint64_t index = 0;
    std::vector<uint64_t> toInsert(10);
    std::vector<uint64_t> toRemove(0);
    std::generate(toInsert.begin(), toInsert.end(), [&index] { return ++index; });

    est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));
  }

  // now make sure we haven't applied anything
  std::string serialization;
  expected = currentSeq;
  est.serialize(serialization, ++currentSeq);
  serialization.clear();
  ASSERT_EQ(est.appliedSeq(), expected);
  ASSERT_EQ(0.1, est.computeEstimate());

  // multiple turncate
  est.bufferTruncate(currentSeq++);
  est.bufferTruncate(currentSeq++);
  est.bufferTruncate(currentSeq++);

  uint64_t index = 0;
  std::vector<uint64_t> toInsert(10);
  std::vector<uint64_t> toRemove(0);
  std::generate(toInsert.begin(), toInsert.end(), [&index] { return ++index; });
  est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));

  expected = currentSeq;
  // now make sure we haven't applied anything
  est.serialize(serialization, currentSeq);
  serialization.clear();
  ASSERT_EQ(est.appliedSeq(), expected);
  ASSERT_EQ(1.0, est.computeEstimate());
}

TEST_F(IndexEstimatorTest, test_truncate_logic_2) {
  rocksdb::SequenceNumber currentSeq(0);
  RocksDBCuckooIndexEstimator<uint64_t> est(2048);
  RocksDBMetadata meta;

  // test buffering where we keep around one old blocker
  for (size_t iteration = 0; iteration < 10; iteration++) {
    uint64_t index = 0;
    std::vector<uint64_t> toInsert(10);
    std::vector<uint64_t> toRemove(0);
    std::generate(toInsert.begin(), toInsert.end(), [&index] { return ++index; });

    est.bufferUpdates(++currentSeq, std::move(toInsert), std::move(toRemove));
  }

  // truncate in the middle
  est.bufferTruncate(++currentSeq);

  auto expected = currentSeq;
  std::string serialization;
  est.serialize(serialization, ++currentSeq);
  serialization.clear();
  ASSERT_EQ(est.appliedSeq(), expected);
  ASSERT_EQ(1.0, est.computeEstimate());

  est.serialize(serialization, ++currentSeq);
  ASSERT_EQ(est.appliedSeq(), expected);
  ASSERT_EQ(1.0, est.computeEstimate());
}
