////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Auth/Rbac/AuthorizationCache.h"
#include "Auth/Rbac/Backend.h"
#include "Auth/Rbac/ServiceImpl.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>

using namespace arangodb;
using namespace std::chrono_literals;
using Cache = rbac::AuthorizationCache;

namespace {

// A manually advanced clock, so that ageing can be tested deterministically.
struct ManualClock {
  std::shared_ptr<Cache::Clock::time_point> now =
      std::make_shared<Cache::Clock::time_point>(Cache::Clock::now());

  auto function() const -> Cache::ClockFunction {
    return [now = now] { return *now; };
  }
  void advance(Cache::Duration d) const { *now += d; }
};

auto makeOptions(Cache::Duration unused, Cache::Duration refresh)
    -> Cache::Options {
  return Cache::Options{.unusedTimeout = unused, .refreshAge = refresh};
}

Cache::Decision const allow{.allowed = true, .message = {}};
Cache::Decision const deny{.allowed = false, .message = "nope"};

}  // namespace

// ---------------------------------------------------------------------------
// AuthorizationCache on its own
// ---------------------------------------------------------------------------

TEST(RbacAuthorizationCacheTest, missThenHit) {
  ManualClock clock;
  Cache cache{makeOptions(60s, 30s), clock.function()};

  EXPECT_FALSE(cache.lookup("tok", "db:Read", "db:database:d").has_value());
  cache.insert("tok", "db:Read", "db:database:d", allow);
  EXPECT_EQ(cache.size(), 1u);
  auto hit = cache.lookup("tok", "db:Read", "db:database:d");
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, allow);

  cache.insert("tok", "db:Drop", "db:database:d", deny);
  hit = cache.lookup("tok", "db:Drop", "db:database:d");
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, deny);
}

TEST(RbacAuthorizationCacheTest, keyComponentsAreDistinct) {
  ManualClock clock;
  Cache cache{makeOptions(60s, 30s), clock.function()};
  cache.insert("tok", "db:Read", "db:database:d", allow);

  EXPECT_FALSE(cache.lookup("other", "db:Read", "db:database:d").has_value());
  EXPECT_FALSE(cache.lookup("tok", "db:Drop", "db:database:d").has_value());
  EXPECT_FALSE(cache.lookup("tok", "db:Read", "db:database:e").has_value());
  EXPECT_TRUE(cache.lookup("tok", "db:Read", "db:database:d").has_value());
}

TEST(RbacAuthorizationCacheTest, optionsActiveDependsOnUnusedTimeout) {
  EXPECT_TRUE(Cache::Options{}.active());
  EXPECT_TRUE(makeOptions(1ms, 30s).active());
  EXPECT_FALSE(makeOptions(0s, 30s).active());
}

TEST(RbacAuthorizationCacheTest, inactiveCacheStoresNothing) {
  ManualClock clock;
  Cache cache{makeOptions(0s, 30s), clock.function()};
  EXPECT_FALSE(cache.active());
  cache.insert("tok", "db:Read", "db:database:d", allow);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_FALSE(cache.lookup("tok", "db:Read", "db:database:d").has_value());
}

TEST(RbacAuthorizationCacheTest, activatingViaOptions) {
  ManualClock clock;
  Cache cache{makeOptions(0s, 30s), clock.function()};
  EXPECT_FALSE(cache.active());
  cache.setOptions(makeOptions(60s, 30s));
  EXPECT_TRUE(cache.active());
  cache.insert("tok", "db:Read", "db:database:d", allow);
  EXPECT_EQ(cache.lookup("tok", "db:Read", "db:database:d"), allow);
}

TEST(RbacAuthorizationCacheTest, deactivatingViaOptionsClearsEntries) {
  ManualClock clock;
  Cache cache{makeOptions(60s, 30s), clock.function()};
  cache.insert("tok", "db:Read", "db:database:d", allow);
  cache.setOptions(makeOptions(0s, 30s));
  EXPECT_EQ(cache.size(), 0u);
}

TEST(RbacAuthorizationCacheTest, maintenanceEvictsUnusedEntries) {
  ManualClock clock;
  Cache cache{makeOptions(10s, 100s), clock.function()};
  cache.insert("tok", "db:Read", "a", allow);
  cache.insert("tok", "db:Read", "b", allow);

  clock.advance(6s);
  ASSERT_TRUE(cache.lookup("tok", "db:Read", "a").has_value());  // touch a
  clock.advance(6s);  // b unused for 12s, a for 6s
  cache.runMaintenance();

  EXPECT_EQ(cache.size(), 1u);
  EXPECT_TRUE(cache.lookup("tok", "db:Read", "a").has_value());
  EXPECT_FALSE(cache.lookup("tok", "db:Read", "b").has_value());
}

TEST(RbacAuthorizationCacheTest, maintenanceRefreshesOldEntries) {
  ManualClock clock;
  Cache cache{makeOptions(100s, 10s), clock.function()};
  std::vector<Cache::Key> refreshed;
  cache.setRefresher([&](Cache::Key const& key) {
    refreshed.push_back(key);
    return std::optional{deny};
  });
  cache.insert("tok", "db:Read", "old", allow);
  clock.advance(8s);
  cache.insert("tok", "db:Read", "young", allow);
  clock.advance(4s);  // old: 12s, young: 4s

  cache.runMaintenance();

  ASSERT_EQ(refreshed.size(), 1u);
  EXPECT_EQ(refreshed[0].token, "tok");
  EXPECT_EQ(refreshed[0].action, "db:Read");
  EXPECT_EQ(refreshed[0].resource, "old");
  EXPECT_EQ(cache.lookup("tok", "db:Read", "old"), deny);
  EXPECT_EQ(cache.lookup("tok", "db:Read", "young"), allow);

  // The refresh reset the creation time, so nothing is due right away.
  refreshed.clear();
  cache.runMaintenance();
  EXPECT_TRUE(refreshed.empty());
}

TEST(RbacAuthorizationCacheTest, refreshDoesNotKeepUnusedEntriesAlive) {
  ManualClock clock;
  Cache cache{makeOptions(25s, 10s), clock.function()};
  int calls = 0;
  cache.setRefresher([&](Cache::Key const&) {
    ++calls;
    return std::optional{allow};
  });
  cache.insert("tok", "db:Read", "x", allow);
  clock.advance(11s);
  cache.runMaintenance();  // refreshed, but not "used"
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(cache.size(), 1u);
  clock.advance(11s);
  cache.runMaintenance();  // refreshed again
  EXPECT_EQ(calls, 2);
  clock.advance(11s);  // last used 33s ago
  cache.runMaintenance();
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(cache.size(), 0u);
}

TEST(RbacAuthorizationCacheTest, failedRefreshEvictsEntry) {
  ManualClock clock;
  Cache cache{makeOptions(100s, 10s), clock.function()};
  cache.setRefresher(
      [](Cache::Key const&) -> std::optional<Cache::Decision> { return {}; });
  cache.insert("tok", "db:Read", "x", allow);
  clock.advance(11s);
  cache.runMaintenance();
  EXPECT_EQ(cache.size(), 0u);
}

TEST(RbacAuthorizationCacheTest, throwingRefresherEvictsEntry) {
  ManualClock clock;
  Cache cache{makeOptions(100s, 10s), clock.function()};
  cache.setRefresher([](Cache::Key const&) -> std::optional<Cache::Decision> {
    throw std::runtime_error("boom");
  });
  cache.insert("tok", "db:Read", "x", allow);
  clock.advance(11s);
  EXPECT_NO_THROW(cache.runMaintenance());
  EXPECT_EQ(cache.size(), 0u);
}

TEST(RbacAuthorizationCacheTest, withoutRefresherOldEntriesAreEvicted) {
  ManualClock clock;
  Cache cache{makeOptions(100s, 10s), clock.function()};
  cache.insert("tok", "db:Read", "x", allow);
  clock.advance(11s);
  cache.runMaintenance();
  EXPECT_EQ(cache.size(), 0u);
}

TEST(RbacAuthorizationCacheTest, veryOldEntriesAreNotServed) {
  // Without maintenance running, an entry older than 2 * refreshAge must not
  // be served any more, even if it is in active use.
  ManualClock clock;
  Cache cache{makeOptions(100s, 10s), clock.function()};
  cache.insert("tok", "db:Read", "x", allow);
  clock.advance(15s);
  EXPECT_TRUE(cache.lookup("tok", "db:Read", "x").has_value());
  clock.advance(6s);
  EXPECT_FALSE(cache.lookup("tok", "db:Read", "x").has_value());
  // A fresh insert makes it servable again.
  cache.insert("tok", "db:Read", "x", deny);
  EXPECT_EQ(cache.lookup("tok", "db:Read", "x"), deny);
}

TEST(RbacAuthorizationCacheTest, chunkedMaintenanceHandlesManyEntries) {
  // Many more entries than one maintenance chunk: every entry must still be
  // evicted or refreshed exactly once.
  ManualClock clock;
  Cache cache{makeOptions(100s, 10s), clock.function()};
  std::size_t const n = 10 * Cache::kMaintenanceChunkEntries + 7;
  std::map<std::string, int> refreshCount;
  cache.setRefresher([&](Cache::Key const& key) {
    ++refreshCount[key.resource];
    return std::optional{deny};
  });
  for (std::size_t i = 0; i < n; ++i) {
    cache.insert("tok", "db:Read", std::to_string(i), allow);
  }
  clock.advance(60s);
  // Use the even ones, so the odd ones become unused.
  for (std::size_t i = 0; i < n; i += 2) {
    ASSERT_FALSE(cache.lookup("tok", "db:Read", std::to_string(i)));  // stale
    cache.insert("tok", "db:Read", std::to_string(i), allow);
  }
  clock.advance(50s);  // odd: unused for 110s; even: 50s old, used 50s ago

  cache.runMaintenance();

  EXPECT_EQ(cache.size(), (n + 1) / 2);
  EXPECT_EQ(refreshCount.size(), (n + 1) / 2);
  for (auto const& [resource, count] : refreshCount) {
    EXPECT_EQ(count, 1) << resource;
    EXPECT_EQ(std::stoul(resource) % 2, 0u) << resource;
    EXPECT_EQ(cache.lookup("tok", "db:Read", resource), deny);
  }
}

TEST(RbacAuthorizationCacheTest, concurrentAccessDuringMaintenance) {
  // Lookups and inserts (which may rehash the map) run while maintenance
  // scans in chunks. Mainly meant to be meaningful under ASan/TSan.
  Cache cache{makeOptions(50ms, 10ms)};
  cache.setRefresher(
      [](Cache::Key const&) { return std::optional<Cache::Decision>{allow}; });
  for (int i = 0; i < 2000; ++i) {
    cache.insert("tok", "db:Read", std::to_string(i), allow);
  }
  std::atomic<bool> done = false;
  std::thread worker([&] {
    int i = 0;
    while (!done.load()) {
      auto resource = std::to_string(i++ % 5000);
      if (!cache.lookup("tok", "db:Read", resource)) {
        cache.insert("tok", "db:Read", resource, allow);
      }
    }
  });
  for (int round = 0; round < 5; ++round) {
    std::this_thread::sleep_for(5ms);
    EXPECT_NO_THROW(cache.runMaintenance());
  }
  done.store(true);
  worker.join();
}

TEST(RbacAuthorizationCacheTest, maintenanceThreadEvictsInBackground) {
  Cache cache{makeOptions(20ms, 1s)};
  cache.insert("tok", "db:Read", "x", allow);
  cache.startMaintenanceThread();
  cache.startMaintenanceThread();  // idempotent

  auto deadline = std::chrono::steady_clock::now() + 10s;
  while (cache.size() != 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(5ms);
  }
  EXPECT_EQ(cache.size(), 0u);

  cache.stopMaintenanceThread();
  cache.stopMaintenanceThread();  // idempotent
}

// ---------------------------------------------------------------------------
// ServiceImpl using the cache
// ---------------------------------------------------------------------------

namespace {

struct CountingBackend : rbac::Backend {
  std::atomic<int> calls = 0;
  Effect nextEffect = Effect::Allow;
  std::string nextMessage;
  bool fail = false;

  auto evaluateTokenManyImpl(rbac::JwtToken const&, RequestItems const& items,
                             transaction::MethodsApi)
      -> futures::Future<ResultT<EvaluateResponseMany>> override {
    ++calls;
    if (fail) {
      co_return Result{TRI_ERROR_CLUSTER_CONNECTION_LOST, "down"};
    }
    EvaluateResponseMany resp{};
    resp.effect = nextEffect;
    resp.message = nextMessage;
    for ([[maybe_unused]] auto const& item : items.items) {
      resp.items.push_back(ResponseItem{.effect = nextEffect, .message = ""});
    }
    co_return resp;
  }
};

struct ServiceFixture {
  ManualClock clock;
  Cache cache{makeOptions(60s, 30s), clock.function()};
  CountingBackend* backend;
  std::unique_ptr<rbac::ServiceImpl> service;

  ServiceFixture() {
    auto b = std::make_unique<CountingBackend>();
    backend = b.get();
    service = std::make_unique<rbac::ServiceImpl>(std::move(b), &cache);
  }
};

auto const readDb = std::array{rbac::ActionResource{
    rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
auto constexpr token = "tok";

}  // namespace

TEST(RbacAuthorizationCacheServiceTest, repeatedCheckHitsCache) {
  ServiceFixture f;
  EXPECT_TRUE(f.service->check({token}, readDb).ok());
  EXPECT_TRUE(f.service->check({token}, readDb).ok());
  EXPECT_EQ(f.backend->calls, 1);
  EXPECT_TRUE(f.cache.lookup(token, "db:Read", "db:database:mydb").has_value());

  // A different token is a different cache key.
  EXPECT_TRUE(f.service->check({"other"}, readDb).ok());
  EXPECT_EQ(f.backend->calls, 2);
}

TEST(RbacAuthorizationCacheServiceTest, denyIsCachedWithMessage) {
  ServiceFixture f;
  f.backend->nextEffect = rbac::Backend::Effect::Deny;
  f.backend->nextMessage = "role missing";
  auto r1 = f.service->check({token}, readDb);
  auto r2 = f.service->check({token}, readDb);
  EXPECT_EQ(f.backend->calls, 1);
  EXPECT_TRUE(r1.is(TRI_ERROR_FORBIDDEN));
  EXPECT_TRUE(r2.is(TRI_ERROR_FORBIDDEN));
  EXPECT_EQ(r1.errorMessage(), r2.errorMessage());
  EXPECT_NE(r2.errorMessage().find("role missing"), std::string::npos);
}

TEST(RbacAuthorizationCacheServiceTest, batchesBypassCache) {
  ServiceFixture f;
  std::array batch{
      rbac::ActionResource{rbac::Action::Create,
                           rbac::resources::View{.db = "d", .name = "v"}},
      rbac::ActionResource{rbac::Action::Read,
                           rbac::resources::Collection{.db = "d", .name = "c"}},
  };
  EXPECT_TRUE(f.service->check({token}, batch).ok());
  EXPECT_TRUE(f.service->check({token}, batch).ok());
  EXPECT_EQ(f.backend->calls, 2);
  EXPECT_EQ(f.cache.size(), 0u);
}

TEST(RbacAuthorizationCacheServiceTest, backendErrorsAreNotCached) {
  ServiceFixture f;
  f.backend->fail = true;
  EXPECT_THROW(std::ignore = f.service->check({token}, readDb),
               basics::Exception);
  EXPECT_EQ(f.cache.size(), 0u);
  f.backend->fail = false;
  EXPECT_TRUE(f.service->check({token}, readDb).ok());
  EXPECT_EQ(f.backend->calls, 2);
}

TEST(RbacAuthorizationCacheServiceTest, maintenanceRefreshesViaBackend) {
  ServiceFixture f;
  EXPECT_TRUE(f.service->check({token}, readDb).ok());
  EXPECT_EQ(f.backend->calls, 1);

  // Permissions are revoked in the external service.
  f.backend->nextEffect = rbac::Backend::Effect::Deny;
  f.clock.advance(31s);
  f.cache.runMaintenance();
  EXPECT_EQ(f.backend->calls, 2);

  EXPECT_TRUE(f.service->check({token}, readDb).is(TRI_ERROR_FORBIDDEN));
  EXPECT_EQ(f.backend->calls, 2);  // served from the refreshed entry
}

TEST(RbacAuthorizationCacheServiceTest, destroyingServiceUnregistersRefresher) {
  ServiceFixture f;
  EXPECT_TRUE(f.service->check({token}, readDb).ok());
  f.service.reset();  // backend is gone now
  f.clock.advance(31s);
  EXPECT_NO_THROW(f.cache.runMaintenance());
  EXPECT_EQ(f.cache.size(), 0u);  // no refresher: old entry is evicted
}

TEST(RbacAuthorizationCacheServiceTest, worksWithoutCache) {
  auto b = std::make_unique<CountingBackend>();
  auto* backend = b.get();
  rbac::ServiceImpl service{std::move(b)};
  EXPECT_TRUE(service.check({token}, readDb).ok());
  EXPECT_TRUE(service.check({token}, readDb).ok());
  EXPECT_EQ(backend->calls, 2);
}
