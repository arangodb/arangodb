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

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace arangodb::rbac {

struct AuthorizationCacheOptions {
  // Entries not used for this long are evicted. Zero disables the cache.
  std::chrono::steady_clock::duration unusedTimeout = std::chrono::seconds{60};
  // Entries older than this are refreshed by the maintenance pass.
  std::chrono::steady_clock::duration refreshAge = std::chrono::seconds{30};

  // The cache is active unless the unused timeout is zero.
  [[nodiscard]] bool active() const noexcept {
    return unusedTimeout > std::chrono::steady_clock::duration::zero();
  }
};

// A process-local cache for answers of the external RBAC authorization
// service. Each entry answers exactly one authorization question, keyed by
// (JWT token, action wire string, resource wire string), and records whether
// it was allowed or denied.
//
// Entries carry a creation time and a last-used time. A maintenance pass
// (normally run by a background thread, see startMaintenanceThread()):
//  - evicts every entry that has not been used for `unusedTimeout`, and
//  - re-asks the authorization service (via the registered Refresher) for
//    every remaining entry older than `refreshAge`, storing the new answer
//    with a fresh creation time. A failed refresh evicts the entry, so the
//    next request asks the service synchronously again.
// A refresh deliberately does not touch the last-used time; otherwise entries
// that are refreshed periodically would never become unused.
//
// As a safety net in case the maintenance thread is not running or lags
// behind, lookup() never returns an entry older than 2 * refreshAge.
//
// All operations are thread-safe. The map is protected by a single mutex,
// which is never held while talking to the authorization service. The
// maintenance scan only holds it for chunks of kMaintenanceChunkEntries
// entries and pauses for kMaintenancePause in between, so that lookups are
// not blocked for long even when the cache is large.
class AuthorizationCache {
 public:
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;

  using Options = AuthorizationCacheOptions;

  // The maintenance scan releases the mutex and pauses after this many
  // entries (or hash buckets, which matters for a sparse map), so that it
  // never blocks lookups for long.
  static constexpr std::size_t kMaintenanceChunkEntries = 100;
  static constexpr std::size_t kMaintenanceChunkBuckets = 10000;
  static constexpr std::chrono::milliseconds kMaintenancePause{1};

  // The cached answer to one authorization question.
  struct Decision {
    bool allowed = false;
    // Message from the authorization service (only meaningful on deny).
    std::string message;

    friend bool operator==(Decision const&, Decision const&) = default;
  };

  struct Key {
    std::string token;
    std::string action;
    std::string resource;
  };

  // Asks the authorization service again for one key. Returns std::nullopt if
  // the question could not be answered (e.g. network error); must not throw.
  using Refresher = std::function<std::optional<Decision>(Key const&)>;
  using ClockFunction = std::function<Clock::time_point()>;

  AuthorizationCache();
  explicit AuthorizationCache(Options options,
                              ClockFunction clock = &Clock::now);
  ~AuthorizationCache();

  AuthorizationCache(AuthorizationCache const&) = delete;
  AuthorizationCache& operator=(AuthorizationCache const&) = delete;

  void setOptions(Options options);
  [[nodiscard]] Options options() const;
  // Lock-free; lookup() and insert() use it to skip the mutex entirely when
  // the cache is not active.
  [[nodiscard]] bool active() const noexcept;

  // Returns the cached decision and bumps the last-used time, or std::nullopt
  // on a miss (or if the cache is not active).
  [[nodiscard]] std::optional<Decision> lookup(std::string_view token,
                                               std::string_view action,
                                               std::string_view resource);

  // Inserts or overwrites an entry, with creation and last-used time = now.
  // No-op if the cache is not active.
  void insert(std::string_view token, std::string_view action,
              std::string_view resource, Decision decision);

  // Registers the callback used by maintenance to refresh old entries. Pass
  // nullptr to unregister; this blocks until an in-flight refresh has
  // finished, so the callback's captures may be destroyed afterwards. Without
  // a refresher, old entries are evicted instead of refreshed.
  void setRefresher(Refresher refresher);

  // One maintenance pass: evict unused entries, refresh old ones.
  void runMaintenance();

  // Starts / stops the background thread that periodically calls
  // runMaintenance(). Both are idempotent.
  void startMaintenanceThread();
  void stopMaintenanceThread();

  [[nodiscard]] std::size_t size() const;
  void clear();

 private:
  struct KeyView {
    std::string_view token;
    std::string_view action;
    std::string_view resource;
  };

  struct Entry {
    Decision decision;
    Clock::time_point created;
    Clock::time_point lastUsed;
  };

  struct KeyHash {
    using is_transparent = void;
    std::size_t operator()(KeyView const& k) const noexcept;
    std::size_t operator()(Key const& k) const noexcept;
  };
  struct KeyEqual {
    using is_transparent = void;
    static KeyView view(Key const& k) noexcept {
      return {k.token, k.action, k.resource};
    }
    static KeyView view(KeyView const& k) noexcept { return k; }
    template<typename A, typename B>
    bool operator()(A const& a, B const& b) const noexcept {
      auto va = view(a);
      auto vb = view(b);
      return va.token == vb.token && va.action == vb.action &&
             va.resource == vb.resource;
    }
  };

  [[nodiscard]] Duration maintenanceInterval() const;
  void runMaintenance(std::stop_token const& stopToken);

  ClockFunction const _clock;

  mutable std::mutex _mutex;
  Options _options;
  // Mirrors _options.active(), so that the hot path can check it without
  // taking _mutex. Only written under _mutex. Relaxed ordering suffices: an
  // active cache is only ever accessed under _mutex, which re-checks
  // _options, and a stale value only delays (de)activation by a moment.
  std::atomic<bool> _active;
  std::unordered_map<Key, Entry, KeyHash, KeyEqual> _entries;

  // Held while calling the refresher, see setRefresher().
  std::mutex _refresherMutex;
  Refresher _refresher;

  std::mutex _threadMutex;
  std::condition_variable_any _threadCv;
  std::jthread _thread;
};

}  // namespace arangodb::rbac
