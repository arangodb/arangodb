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
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "Basics/VelocyPackHelper.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionalCache.h"
#include "Cache/VPackKeyHasher.h"
#include "Random/RandomGenerator.h"
#include "RestServer/SharedPRNGFeature.h"

#include "Mocks/Servers.h"
#include "MockScheduler.h"

using namespace arangodb;
using namespace arangodb::cache;
using namespace arangodb::tests::mocks;

TEST(CacheTransactionalCacheVPackKeyHasherTest,
     verify_that_insertion_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 4 * cacheLimit);
  auto cache = manager.createCache<VPackKeyHasher>(CacheType::Transactional,
                                                   false, cacheLimit);

  VPackBuilder builder;
  for (std::uint64_t i = 0; i < 16384; i++) {
    builder.clear();
    builder.add(VPackValue(i));
    VPackSlice s = builder.slice();

    CachedValue* value = CachedValue::construct(s.start(), s.byteSize(),
                                                s.start(), s.byteSize());
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      auto f = cache->find(s.start(), static_cast<uint32_t>(s.byteSize()));
      ASSERT_TRUE(f.found());
      ASSERT_EQ(0, memcmp(f.value()->value(), s.start(), s.byteSize()));
    } else {
      delete value;
    }
  }

  for (std::uint64_t i = 0; i < 1024; i++) {
    builder.clear();
    builder.add(VPackValue(std::string("test") + std::to_string(i)));
    VPackSlice s = builder.slice();

    CachedValue* value = CachedValue::construct(s.start(), s.byteSize(),
                                                s.start(), s.byteSize());
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      auto f = cache->find(s.start(), static_cast<uint32_t>(s.byteSize()));
      ASSERT_TRUE(f.found());
      ASSERT_EQ(0, memcmp(f.value()->value(), s.start(), s.byteSize()));
    } else {
      delete value;
    }
  }

  manager.destroyCache(cache);
}

TEST(CacheTransactionalCacheVPackKeyHasherTest,
     verify_similar_values_work_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 4 * cacheLimit);
  auto cache = manager.createCache<VPackKeyHasher>(CacheType::Transactional,
                                                   false, cacheLimit);

  std::vector<std::uint8_t> builder;
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    // add as uint
    builder.push_back(0x28U);
    builder.push_back(i);

    CachedValue* value = CachedValue::construct(builder.data(), builder.size(),
                                                builder.data(), builder.size());
    TRI_ASSERT(value != nullptr);
    Result status;
    do {
      status = cache->insert(value);
    } while (!status.ok());
  }

  // look up as UInt
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    builder.push_back(0x28U);
    builder.push_back(i);
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isUInt());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        continue;
      }
      ASSERT_TRUE(f.found());
      VPackSlice foundSlice(f.value()->value());
      ASSERT_EQ(
          0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
      ASSERT_TRUE(foundSlice.isUInt());
      ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                          f.value()->keySize(), builder.data(),
                                          builder.size()));
      break;
    }
  }

  // look up as Int
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    if (i <= 127) {
      builder.push_back(0x20U);
      builder.push_back(i);
    } else {
      builder.push_back(0x21U);
      builder.push_back(i & 0xffU);
      builder.push_back((i >> 8) & 0xffU);
    }
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isInt());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        continue;
      }
      ASSERT_TRUE(f.found());
      VPackSlice foundSlice(f.value()->value());
      ASSERT_EQ(
          0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
      ASSERT_TRUE(foundSlice.isUInt());
      ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                          f.value()->keySize(), builder.data(),
                                          builder.size()));
      break;
    }
  }

  // look up as double
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    builder.push_back(0x1bU);
    uint8_t buffer[8];
    double v = static_cast<double>(i);
    memcpy(&buffer[0], &v, sizeof(v));
    for (std::size_t j = 0; j < sizeof(buffer); ++j) {
      builder.push_back(buffer[j]);
    }
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isDouble());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        continue;
      }
      ASSERT_TRUE(f.found());
      VPackSlice foundSlice(f.value()->value());
      ASSERT_EQ(
          0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
      ASSERT_TRUE(foundSlice.isUInt());
      ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                          f.value()->keySize(), builder.data(),
                                          builder.size()));
      break;
    }
  }

  // look up as SmallInt
  for (std::uint32_t i = 0; i < 10; i++) {
    builder.clear();
    builder.push_back(0x30U + i);
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isSmallInt());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        continue;
      }
      ASSERT_TRUE(f.found());
      VPackSlice foundSlice(f.value()->value());
      ASSERT_EQ(
          0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
      ASSERT_TRUE(foundSlice.isUInt());
      ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                          f.value()->keySize(), builder.data(),
                                          builder.size()));
      break;
    }
  }

  manager.destroyCache(cache);
}

TEST(CacheTransactionalCacheVPackKeyHasherTest,
     verify_removal_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 4 * cacheLimit);
  auto cache = manager.createCache<VPackKeyHasher>(CacheType::Transactional,
                                                   false, cacheLimit);

  std::vector<std::uint8_t> builder;
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    // add as uint
    builder.push_back(0x28U);
    builder.push_back(i);

    CachedValue* value = CachedValue::construct(builder.data(), builder.size(),
                                                builder.data(), builder.size());
    TRI_ASSERT(value != nullptr);
    Result status;
    do {
      status = cache->insert(value);
    } while (!status.ok());
  }

  // remove as Int
  for (std::uint32_t i = 0; i < 256; i += 2) {
    builder.clear();
    if (i <= 127) {
      builder.push_back(0x20U);
      builder.push_back(i);
    } else {
      builder.push_back(0x21U);
      builder.push_back(i & 0xffU);
      builder.push_back((i >> 8) & 0xffU);
    }
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isInt());

    while (true) {
      auto s =
          cache->remove(builder.data(), static_cast<uint32_t>(builder.size()));
      if (s.fail() && s.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        continue;
      }
      ASSERT_TRUE(s.ok());
      break;
    }
  }

  // look up as UInt
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    builder.push_back(0x28U);
    builder.push_back(i);
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isUInt());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (i % 2 == 0) {
        ASSERT_FALSE(f.found());
      } else {
        if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
          continue;
        }
        ASSERT_TRUE(f.found());
        VPackSlice foundSlice(f.value()->value());
        ASSERT_EQ(
            0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
        ASSERT_TRUE(foundSlice.isUInt());
        ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                            f.value()->keySize(),
                                            builder.data(), builder.size()));
      }
      break;
    }
  }

  // look up as Int
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    if (i <= 127) {
      builder.push_back(0x20U);
      builder.push_back(i);
    } else {
      builder.push_back(0x21U);
      builder.push_back(i & 0xffU);
      builder.push_back((i >> 8) & 0xffU);
    }
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isInt());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (i % 2 == 0) {
        ASSERT_FALSE(f.found());
      } else {
        if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
          continue;
        }
        ASSERT_TRUE(f.found());
        VPackSlice foundSlice(f.value()->value());
        ASSERT_EQ(
            0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
        ASSERT_TRUE(foundSlice.isUInt());
        ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                            f.value()->keySize(),
                                            builder.data(), builder.size()));
      }
      break;
    }
  }

  // look up as double
  for (std::uint32_t i = 0; i < 256; i++) {
    builder.clear();
    builder.push_back(0x1bU);
    uint8_t buffer[8];
    double v = static_cast<double>(i);
    memcpy(&buffer[0], &v, sizeof(v));
    for (std::size_t j = 0; j < sizeof(buffer); ++j) {
      builder.push_back(buffer[j]);
    }
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isDouble());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (i % 2 == 0) {
        ASSERT_FALSE(f.found());
      } else {
        if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
          continue;
        }
        ASSERT_TRUE(f.found());
        VPackSlice foundSlice(f.value()->value());
        ASSERT_EQ(
            0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
        ASSERT_TRUE(foundSlice.isUInt());
        ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                            f.value()->keySize(),
                                            builder.data(), builder.size()));
      }
      break;
    }
  }

  // look up as SmallInt
  for (std::uint32_t i = 0; i < 10; i++) {
    builder.clear();
    builder.push_back(0x30U + i);
    VPackSlice builtSlice(reinterpret_cast<uint8_t const*>(builder.data()));
    ASSERT_TRUE(builtSlice.isSmallInt());

    while (true) {
      auto f =
          cache->find(builder.data(), static_cast<uint32_t>(builder.size()));
      if (i % 2 == 0) {
        ASSERT_FALSE(f.found());
      } else {
        if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
          continue;
        }
        ASSERT_TRUE(f.found());
        VPackSlice foundSlice(f.value()->value());
        ASSERT_EQ(
            0, basics::VelocyPackHelper::compare(foundSlice, builtSlice, true));
        ASSERT_TRUE(foundSlice.isUInt());
        ASSERT_TRUE(VPackKeyHasher::sameKey(f.value()->key(),
                                            f.value()->keySize(),
                                            builder.data(), builder.size()));
      }
      break;
    }
  }

  manager.destroyCache(cache);
}

TEST(CacheTransactionalCacheVPackKeyHasherTest,
     verify_banishing_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 4 * cacheLimit);
  auto cache = manager.createCache<VPackKeyHasher>(CacheType::Transactional,
                                                   false, cacheLimit);

  Transaction* tx = manager.beginTransaction(false);

  VPackBuilder builder;
  for (std::uint64_t i = 0; i < 1024; i++) {
    builder.clear();
    // add a uint
    builder.add(VPackValue(i));
    VPackSlice s = builder.slice();

    CachedValue* value = CachedValue::construct(s.start(), s.byteSize(),
                                                s.start(), s.byteSize());
    TRI_ASSERT(value != nullptr);
    Result status;
    do {
      status = cache->insert(value);
    } while (!status.ok());
  }

  for (std::uint64_t i = 512; i < 1024; i++) {
    builder.clear();
    // banish as int
    builder.add(VPackValue(static_cast<int64_t>(i)));
    VPackSlice s = builder.slice();

    Result status;
    do {
      status = cache->banish(s.start(), static_cast<uint32_t>(s.byteSize()));
    } while (!status.ok());
    ASSERT_TRUE(status.ok());

    while (true) {
      auto f = cache->find(s.start(), static_cast<uint32_t>(s.byteSize()));
      if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        continue;
      }
      ASSERT_FALSE(f.found());
      break;
    }
  }

  for (std::uint64_t i = 512; i < 1024; i++) {
    builder.clear();
    // try to re-insert as double
    builder.add(VPackValue(static_cast<double>(i)));
    VPackSlice s = builder.slice();

    CachedValue* value = CachedValue::construct(s.start(), s.byteSize(),
                                                s.start(), s.byteSize());
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    ASSERT_TRUE(status.fail());
    delete value;
    auto f = cache->find(s.start(), static_cast<uint32_t>(s.byteSize()));
    ASSERT_FALSE(f.found());
  }

  manager.endTransaction(tx);
  tx = manager.beginTransaction(false);

  for (std::uint64_t i = 512; i < 1024; i++) {
    builder.clear();
    // re-insert as double
    builder.add(VPackValue(static_cast<double>(i)));
    VPackSlice s = builder.slice();

    CachedValue* value = CachedValue::construct(s.start(), s.byteSize(),
                                                s.start(), s.byteSize());
    TRI_ASSERT(value != nullptr);
    Result status;
    do {
      status = cache->insert(value);
    } while (!status.ok());
    ASSERT_TRUE(status.ok());

    // look it up again as int64
    builder.clear();
    builder.add(VPackValue(static_cast<int64_t>(i)));
    s = builder.slice();

    while (true) {
      auto f = cache->find(s.start(), static_cast<uint32_t>(s.byteSize()));
      if (!f.found() && f.result() != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        continue;
      }
      ASSERT_TRUE(f.found());
      break;
    }
  }

  manager.endTransaction(tx);
  manager.destroyCache(cache);
}
