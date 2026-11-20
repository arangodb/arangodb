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

#include "AuthorizationCache.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <algorithm>
#include <exception>
#include <stop_token>
#include <vector>

namespace arangodb::rbac {

namespace {

std::size_t hashCombine(std::size_t seed, std::size_t value) noexcept {
  return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

}  // namespace

std::size_t AuthorizationCache::KeyHash::operator()(
    KeyView const& k) const noexcept {
  std::hash<std::string_view> h;
  return hashCombine(hashCombine(h(k.token), h(k.action)), h(k.resource));
}

std::size_t AuthorizationCache::KeyHash::operator()(
    Key const& k) const noexcept {
  return (*this)(KeyEqual::view(k));
}

AuthorizationCache::AuthorizationCache() : AuthorizationCache(Options{}) {}

AuthorizationCache::AuthorizationCache(Options options, ClockFunction clock)
    : _clock(std::move(clock)), _options(options) {}

AuthorizationCache::~AuthorizationCache() { stopMaintenanceThread(); }

void AuthorizationCache::setOptions(Options options) {
  std::lock_guard guard{_mutex};
  _options = options;
  if (_options.unusedTimeout <= Duration::zero()) {
    _entries.clear();
  }
}

auto AuthorizationCache::options() const -> Options {
  std::lock_guard guard{_mutex};
  return _options;
}

bool AuthorizationCache::enabled() const {
  std::lock_guard guard{_mutex};
  return _options.unusedTimeout > Duration::zero();
}

auto AuthorizationCache::lookup(std::string_view token, std::string_view action,
                                std::string_view resource)
    -> std::optional<Decision> {
  auto now = _clock();
  std::lock_guard guard{_mutex};
  if (_options.unusedTimeout <= Duration::zero()) {
    return std::nullopt;
  }
  auto it = _entries.find(KeyView{token, action, resource});
  if (it == _entries.end()) {
    return std::nullopt;
  }
  // Safety net: never serve an entry that maintenance should long have
  // refreshed. The caller will ask the service and overwrite it.
  if (now - it->second.created >= 2 * _options.refreshAge) {
    return std::nullopt;
  }
  it->second.lastUsed = now;
  return it->second.decision;
}

void AuthorizationCache::insert(std::string_view token, std::string_view action,
                                std::string_view resource, Decision decision) {
  auto now = _clock();
  std::lock_guard guard{_mutex};
  if (_options.unusedTimeout <= Duration::zero()) {
    return;
  }
  auto it = _entries.find(KeyView{token, action, resource});
  if (it == _entries.end()) {
    _entries.emplace(
        Key{std::string{token}, std::string{action}, std::string{resource}},
        Entry{std::move(decision), now, now});
  } else {
    it->second = Entry{std::move(decision), now, now};
  }
}

void AuthorizationCache::setRefresher(Refresher refresher) {
  std::lock_guard guard{_refresherMutex};
  _refresher = std::move(refresher);
}

void AuthorizationCache::runMaintenance() { runMaintenance(std::stop_token{}); }

void AuthorizationCache::runMaintenance(std::stop_token const& stopToken) {
  std::vector<Key> toRefresh;
  {
    auto now = _clock();
    std::lock_guard guard{_mutex};
    if (_options.unusedTimeout <= Duration::zero()) {
      _entries.clear();
      return;
    }
    for (auto it = _entries.begin(); it != _entries.end();) {
      if (now - it->second.lastUsed >= _options.unusedTimeout) {
        it = _entries.erase(it);
        continue;
      }
      if (now - it->second.created >= _options.refreshAge) {
        toRefresh.push_back(it->first);
      }
      ++it;
    }
  }

  // Refresh outside of _mutex, so that request threads are never blocked by
  // the network round-trip to the authorization service.
  for (auto const& key : toRefresh) {
    if (stopToken.stop_requested()) {
      return;
    }
    std::optional<Decision> decision;
    {
      std::lock_guard refresherGuard{_refresherMutex};
      if (_refresher) {
        try {
          decision = _refresher(key);
        } catch (std::exception const& ex) {
          LOG_TOPIC("2a7c1", DEBUG, Logger::AUTHORIZATION)
              << "failed to refresh RBAC authorization cache entry: "
              << ex.what();
        } catch (...) {
          LOG_TOPIC("2a7c2", DEBUG, Logger::AUTHORIZATION)
              << "failed to refresh RBAC authorization cache entry";
        }
      }
    }
    auto now = _clock();
    std::lock_guard guard{_mutex};
    auto it = _entries.find(key);
    if (it == _entries.end()) {
      continue;
    }
    if (decision.has_value()) {
      it->second.decision = std::move(*decision);
      it->second.created = now;
    } else {
      // Could not refresh: drop it, so the next request asks synchronously.
      _entries.erase(it);
    }
  }
}

auto AuthorizationCache::maintenanceInterval() const -> Duration {
  auto opts = options();
  auto shortest = std::min(opts.unusedTimeout, opts.refreshAge);
  return std::clamp<Duration>(shortest / 4, std::chrono::milliseconds{10},
                              std::chrono::seconds{1});
}

void AuthorizationCache::startMaintenanceThread() {
  if (_thread.joinable()) {
    return;
  }
  _thread = std::jthread([this](std::stop_token stopToken) {
    LOG_TOPIC("2a7c3", DEBUG, Logger::AUTHORIZATION)
        << "RBAC authorization cache maintenance thread started";
    while (!stopToken.stop_requested()) {
      {
        std::unique_lock lock{_threadMutex};
        _threadCv.wait_for(lock, stopToken, maintenanceInterval(),
                           [] { return false; });
      }
      if (stopToken.stop_requested()) {
        break;
      }
      try {
        runMaintenance(stopToken);
      } catch (std::exception const& ex) {
        LOG_TOPIC("2a7c4", WARN, Logger::AUTHORIZATION)
            << "RBAC authorization cache maintenance failed: " << ex.what();
      }
    }
    LOG_TOPIC("2a7c5", DEBUG, Logger::AUTHORIZATION)
        << "RBAC authorization cache maintenance thread stopped";
  });
}

void AuthorizationCache::stopMaintenanceThread() {
  if (!_thread.joinable()) {
    return;
  }
  _thread.request_stop();
  _thread.join();
  _thread = std::jthread{};
}

std::size_t AuthorizationCache::size() const {
  std::lock_guard guard{_mutex};
  return _entries.size();
}

void AuthorizationCache::clear() {
  std::lock_guard guard{_mutex};
  _entries.clear();
}

}  // namespace arangodb::rbac
