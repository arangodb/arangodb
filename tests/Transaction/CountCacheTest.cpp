////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <thread>

#include "Basics/system-functions.h"
#include "Transaction/CountCache.h"
#include "gtest/gtest.h"

using namespace arangodb;

struct SpeedyCountCache : public transaction::CountCache {
  explicit SpeedyCountCache(double ttl)
      : CountCache(ttl), _time(TRI_microtime()) {}

  double getTime() const override { return _time; }

  void advanceTime(double value) { _time += value; }

  double _time;
};

TEST(TransactionCountCacheTest, testExpireShort) {
  SpeedyCountCache cache(0.5);

  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());

  cache.store(0);
  EXPECT_EQ(0, cache.get());
  EXPECT_EQ(0, cache.getWithTtl());

  cache.store(555);
  EXPECT_EQ(555, cache.get());
  EXPECT_EQ(555, cache.getWithTtl());

  cache.advanceTime(0.550);

  EXPECT_EQ(555, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());

  cache.store(21111234);
  EXPECT_EQ(21111234, cache.get());
  EXPECT_EQ(21111234, cache.getWithTtl());

  cache.store(0);
  EXPECT_EQ(0, cache.get());
  EXPECT_EQ(0, cache.getWithTtl());

  cache.advanceTime(0.550);

  EXPECT_EQ(0, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
}

TEST(TransactionCountCacheTest, testExpireMedium) {
  SpeedyCountCache cache(1.5);

  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());

  cache.store(0);
  EXPECT_EQ(0, cache.get());
  EXPECT_EQ(0, cache.getWithTtl());

  cache.store(555);
  EXPECT_EQ(555, cache.get());
  EXPECT_EQ(555, cache.getWithTtl());

  cache.advanceTime(0.250);

  EXPECT_EQ(555, cache.get());
  EXPECT_EQ(555, cache.getWithTtl());

  cache.advanceTime(0.250);

  EXPECT_EQ(555, cache.get());
  EXPECT_EQ(555, cache.getWithTtl());

  cache.advanceTime(1.100);

  EXPECT_EQ(555, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());

  cache.store(21111234);

  EXPECT_EQ(21111234, cache.get());
  EXPECT_EQ(21111234, cache.getWithTtl());

  cache.advanceTime(0.250);

  EXPECT_EQ(21111234, cache.get());
  EXPECT_EQ(21111234, cache.getWithTtl());

  cache.advanceTime(1.350);

  EXPECT_EQ(21111234, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
}

TEST(TransactionCountCacheTest, testExpireLong) {
  SpeedyCountCache cache(60.0);

  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());

  cache.store(0);
  EXPECT_EQ(0, cache.get());
  EXPECT_EQ(0, cache.getWithTtl());

  cache.store(666);
  EXPECT_EQ(666, cache.get());
  EXPECT_EQ(666, cache.getWithTtl());

  cache.advanceTime(0.250);

  EXPECT_EQ(666, cache.get());
  EXPECT_EQ(666, cache.getWithTtl());

  cache.advanceTime(1.100);

  EXPECT_EQ(666, cache.get());
  EXPECT_EQ(666, cache.getWithTtl());

  cache.store(777);

  EXPECT_EQ(777, cache.get());
  EXPECT_EQ(777, cache.getWithTtl());

  cache.store(888);

  EXPECT_EQ(888, cache.get());
  EXPECT_EQ(888, cache.getWithTtl());

  cache.advanceTime(55.0);
  EXPECT_EQ(888, cache.get());
  EXPECT_EQ(888, cache.getWithTtl());

  cache.advanceTime(5.01);
  EXPECT_EQ(888, cache.get());
  EXPECT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
}
