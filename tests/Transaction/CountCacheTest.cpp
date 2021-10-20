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

#include "Transaction/CountCache.h"

#include "gtest/gtest.h"
#include <chrono>
#include <thread>

using namespace arangodb;

TEST(TransactionCountCacheTest, testExpireShort) {
  transaction::CountCache cache(0.5);

  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.get());
  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());

  cache.store(0);
  ASSERT_EQ(0, cache.get());
  ASSERT_EQ(0, cache.getWithTtl());
  
  cache.store(555);
  ASSERT_EQ(555, cache.get());
  ASSERT_EQ(555, cache.getWithTtl());

  std::this_thread::sleep_for(std::chrono::milliseconds(550));

  ASSERT_EQ(555, cache.get());
  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
  
  cache.store(21111234);
  ASSERT_EQ(21111234, cache.get());
  ASSERT_EQ(21111234, cache.getWithTtl());
  
  cache.store(0);
  ASSERT_EQ(0, cache.get());
  ASSERT_EQ(0, cache.getWithTtl());
  
  std::this_thread::sleep_for(std::chrono::milliseconds(550));

  ASSERT_EQ(0, cache.get());
  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
}

TEST(TransactionCountCacheTest, testExpireMedium) {
  transaction::CountCache cache(1.5);

  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.get());
  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
  
  cache.store(0);
  ASSERT_EQ(0, cache.get());
  ASSERT_EQ(0, cache.getWithTtl());
  
  cache.store(555);
  ASSERT_EQ(555, cache.get());
  ASSERT_EQ(555, cache.getWithTtl());

  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  
  ASSERT_EQ(555, cache.get());
  ASSERT_EQ(555, cache.getWithTtl());
  
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  ASSERT_EQ(555, cache.get());
  ASSERT_EQ(555, cache.getWithTtl());
  
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  ASSERT_EQ(555, cache.get());
  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
  
  cache.store(21111234);
  
  ASSERT_EQ(21111234, cache.get());
  ASSERT_EQ(21111234, cache.getWithTtl());
  
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  
  ASSERT_EQ(21111234, cache.get());
  ASSERT_EQ(21111234, cache.getWithTtl());
  
  std::this_thread::sleep_for(std::chrono::milliseconds(1350));
  
  ASSERT_EQ(21111234, cache.get());
  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
}

TEST(TransactionCountCacheTest, testExpireLong) {
  transaction::CountCache cache(60.0);

  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.get());
  ASSERT_EQ(transaction::CountCache::NotPopulated, cache.getWithTtl());
  
  cache.store(0);
  ASSERT_EQ(0, cache.get());
  ASSERT_EQ(0, cache.getWithTtl());
  
  cache.store(666);
  ASSERT_EQ(666, cache.get());
  ASSERT_EQ(666, cache.getWithTtl());

  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  
  ASSERT_EQ(666, cache.get());
  ASSERT_EQ(666, cache.getWithTtl());
  
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  ASSERT_EQ(666, cache.get());
  ASSERT_EQ(666, cache.getWithTtl());
  
  cache.store(777);
  
  ASSERT_EQ(777, cache.get());
  ASSERT_EQ(777, cache.getWithTtl());
  
  cache.store(888);
  
  ASSERT_EQ(888, cache.get());
  ASSERT_EQ(888, cache.getWithTtl());
}
