////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB Inc, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCache.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/AgencyStrings.h"
#include "Agency/Node.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "GeneralServer/RestHandler.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::consensus;

namespace std {
class shared_lock_guard {
private:
  std::shared_mutex& m;

public:
  explicit shared_lock_guard(std::shared_mutex& m_): m(m_) {
    m.lock_shared();
  }
  shared_lock_guard(std::shared_mutex& m_, std::adopt_lock_t): m(m_) {}
  ~shared_lock_guard() {
    m.unlock_shared();
  }
};
}

AgencyCache::AgencyCache(
  application_features::ApplicationServer& server,
  AgencyCallbackRegistry& callbackRegistry)
  : Thread(server, "AgencyCache"), _commitIndex(0),
    _readDB(server, nullptr, "readDB"), _callbackRegistry(callbackRegistry) {}


AgencyCache::~AgencyCache() {
  beginShutdown();
}

bool AgencyCache::isSystem() const { return true; }

/// Start all agent thread
bool AgencyCache::start() {
  LOG_TOPIC("9a90f", DEBUG, Logger::AGENCY) << "Starting agency cache worker.";
  Thread::start();
  return true;
}


// Fill existing Builder from readDB, mainly /Plan /Current
index_t AgencyCache::get(VPackBuilder& result, std::string const& path) const {
  result.clear();
  std::shared_lock_guard g(_storeLock);
  if (_commitIndex > 0) {
    _readDB.get("arango/" + path, result, false);
  }
  return _commitIndex;
}

// Create Builder from readDB, mainly /Plan /Current
std::tuple<query_t, index_t> AgencyCache::get(std::string const& path) const {
  auto ret = std::make_shared<VPackBuilder>();
  index_t commitIndex = get(*ret, path);
  return std::tuple(std::move(ret), commitIndex);
}

// Get Builder from readDB mainly /Plan /Current
query_t AgencyCache::dump() const {
  auto query = std::make_shared<arangodb::velocypack::Builder>();
  {
    VPackArrayBuilder outer(query.get());
    VPackArrayBuilder inner(query.get());
    query->add(VPackValue("/"));
  }

  auto ret = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder o(ret.get());
    std::shared_lock_guard g(_storeLock);
    ret->add("index", VPackValue(_commitIndex));
    ret->add(VPackValue("cache"));
    _readDB.read(query, ret);
  }
  return ret;
}

// Get Builder from readDB, mainly /Plan /Current
std::tuple<query_t, index_t> AgencyCache::read(std::vector<std::string> const& paths) const {
  auto query = std::make_shared<arangodb::velocypack::Builder>();
  {
    VPackArrayBuilder outer(query.get());
    VPackArrayBuilder inner(query.get());
    for (auto const& i : paths) {
      query->add(VPackValue(i));
    }
  }

  auto result = std::make_shared<arangodb::velocypack::Builder>();
  std::shared_lock_guard g(_storeLock);

  if (_commitIndex > 0) {
    _readDB.read(query, result);
  }
  return std::tuple(std::move(result), _commitIndex);
}

futures::Future<arangodb::Result> AgencyCache::waitFor(index_t index) {
  std::shared_lock_guard s(_storeLock);
  if (index <= _commitIndex) {
    return futures::makeFuture(arangodb::Result());
  }
  // intentionally don't release _storeLock here until we have inserted the promise
  std::lock_guard w(_waitLock);
  return _waiting.emplace(index, futures::Promise<arangodb::Result>())->second.getFuture();
}

index_t AgencyCache::index() const {
  std::shared_lock_guard g(_storeLock);
  return _commitIndex;
}

void AgencyCache::handleCallbacksNoLock(
  VPackSlice slice, std::unordered_set<uint64_t>& uniq, std::vector<uint64_t>& toCall,
  std::unordered_set<std::string>& planChanges, std::unordered_set<std::string>& currentChanges) {

  if (!slice.isObject()) {
    LOG_TOPIC("31514", DEBUG, Logger::CLUSTER) <<
      "Cannot handle callback on non-object " << slice.toJson();
    return;
  }

  // Collect and normalize keys
  std::vector<std::string> keys;
  keys.reserve(slice.length());
  for (auto const& i : VPackObjectIterator(slice)) {
    VPackValueLength l;
    char const* p = i.key.getString(l);
    keys.emplace_back(Store::normalize(p, l));
  }

  std::sort(keys.begin(), keys.end());
  // Find callbacks, which are a prefix of some key:
  for (auto const& cb : _callbacks) {
    auto const& cbkey = cb.first;
    auto it = std::lower_bound(keys.begin(), keys.end(), cbkey);
    if (it != keys.end() && it->compare(0, cbkey.size(), cbkey) == 0) {
      if (uniq.emplace(cb.second).second) {
        toCall.push_back(cb.second);
      }
    }
  }

  std::unordered_set<std::string> dbs;
  const char SLASH ('/');

  // Find keys, which are a prefix of a callback:
  for (auto const& k : keys) {

      auto it = _callbacks.lower_bound(k);
      while (it != _callbacks.end() && it->first.compare(0, k.size(), k) == 0) {
        if (uniq.emplace(it->second).second) {
          toCall.push_back(it->second);
        }
        ++it;
      }

          if (k.size() > 8) {
      std::string r(k.substr(8)); // TODO: Optimize for string_view
      auto rs = r.size();

      if (rs > strlen(PLAN) && r.compare(0, strlen(PLAN), PLAN) == 0) {
        if (rs >= strlen(PLAN_VERSION) &&                      // Plan/Version -> ignore
            r.compare(0, strlen(PLAN_VERSION), PLAN_VERSION) == 0) {
          continue;
        } else if (rs > strlen(PLAN_COLLECTIONS) &&                // Plan/Collections
                   r.compare(0, strlen(PLAN_COLLECTIONS), PLAN_COLLECTIONS) == 0) {
          auto tmp = r.substr(strlen(PLAN_COLLECTIONS));
          planChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (rs > strlen(PLAN_DATABASES) &&             // Plan/Databases
                   r.compare(0, strlen(PLAN_DATABASES), PLAN_DATABASES) == 0) {
          auto tmp = r.substr(strlen(PLAN_DATABASES));
          planChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (rs > strlen(PLAN_VIEWS) &&                 // Plan/Views
                   r.compare(0, strlen(PLAN_VIEWS), (PLAN_VIEWS)) == 0) {
          auto tmp = r.substr(strlen(PLAN_VIEWS));
          planChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (rs > strlen(PLAN_ANALYZERS) &&             // Plan/Analyzers
                   r.compare(0, strlen(PLAN_ANALYZERS), PLAN_ANALYZERS)==0) {
          auto tmp = r.substr(strlen(PLAN_ANALYZERS));
          planChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else {                                              // Plan others
          planChanges.emplace(std::string());
        }
      } else if (rs > strlen(CURRENT) && r.compare(0, strlen(CURRENT), CURRENT) == 0) {
        if (rs >= strlen(CURRENT_VERSION) &&                   // Current/Version is ignored
            r.compare(0, strlen(CURRENT_VERSION), CURRENT_VERSION) == 0) {
          continue;
        } else if (rs > strlen(CURRENT_COLLECTIONS) &&        // Current/Collections
                   r.compare(0, strlen(CURRENT_COLLECTIONS), CURRENT_COLLECTIONS) == 0) {
          auto tmp = r.substr(strlen(CURRENT_COLLECTIONS));
          currentChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (rs > strlen(CURRENT_DATABASES) &&          // Current/Databases
                   r.compare(0, strlen(CURRENT_DATABASES), CURRENT_DATABASES) == 0) {
          auto tmp = r.substr(strlen(CURRENT_DATABASES));
          currentChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else {                                              // Current others
          currentChanges.emplace(std::string());
        }
      }

    }
  }
}

void AgencyCache::run() {

  using namespace std::chrono;
  TRI_ASSERT(AsyncAgencyCommManager::INSTANCE != nullptr);

  _commitIndex = 0;
  _readDB.clear();
  double wait = 0.0;

  auto increaseWaitTime = [&wait]() noexcept {
    if (wait <= 1.9) {
      wait += 0.1;
    }
  };

  // Long poll to agency
  auto sendTransaction =
    [&]() {
      index_t commitIndex = 0;
      {
        std::shared_lock_guard g(_storeLock);
        commitIndex = _commitIndex + 1;
      }
      LOG_TOPIC("afede", TRACE, Logger::CLUSTER)
          << "AgencyCache: poll polls: waiting for commitIndex " << commitIndex;
      return AsyncAgencyComm().poll(60s, commitIndex);
    };

  // while not stopping
  //   poll agency
  //   if result
  //     if firstIndex == 0
  //       overwrite readDB, update commitIndex
  //     else
  //       apply logs to local cache
  //   else
  //     wait ever longer until success
  std::vector<uint64_t> toCall;
  std::unordered_set<uint64_t> uniq;
  while (!this->isStopping()) {
    // we need to make sure that this thread keeps running, so whenever
    // an exception happens in here, we log it and go on
    try {
      uniq.clear();
      toCall.clear();
      std::this_thread::sleep_for(std::chrono::duration<double>(wait));

      // rb holds receives either
      // * A complete overwrite (firstIndex == 0)
      //   {..., result:{commitIndex:X, firstIndex:0, readDB:{...}}}
      // * Incremental change to cache (firstIndex != 0)
      //   {..., result:{commitIndex:X, firstIndex:Y, log:[]}}

      if (server().getFeature<NetworkFeature>().prepared()) {
        auto ret = sendTransaction()
          .thenValue(
            [&](AsyncAgencyCommResult&& rb) {

              if (rb.ok() && rb.statusCode() == arangodb::fuerte::StatusOK) {
                index_t curIndex = 0;
                {
                  std::lock_guard g(_storeLock);
                  curIndex = _commitIndex;
                }
                auto slc = rb.slice();
                wait = 0.;
                TRI_ASSERT(slc.hasKey("result"));
                VPackSlice rs = slc.get("result");
                TRI_ASSERT(rs.hasKey("commitIndex"));
                TRI_ASSERT(rs.get("commitIndex").isNumber());
                TRI_ASSERT(rs.hasKey("firstIndex"));
                TRI_ASSERT(rs.get("firstIndex").isNumber());
                index_t commitIndex = rs.get("commitIndex").getNumber<uint64_t>();
                index_t firstIndex = rs.get("firstIndex").getNumber<uint64_t>();

                if (firstIndex > 0) {
                  // Do incoming logs match our cache's index?
                  if (firstIndex != curIndex + 1) {
                    LOG_TOPIC("a9a09", WARN, Logger::CLUSTER) <<
                      "Logs from poll start with index " << firstIndex <<
                      " we requested logs from and including " << curIndex << " retrying.";
                    LOG_TOPIC("457e9", TRACE, Logger::CLUSTER) << "Incoming: " << rs.toJson();
                    increaseWaitTime();
                  } else {
                    TRI_ASSERT(rs.hasKey("log"));
                    TRI_ASSERT(rs.get("log").isArray());
                    LOG_TOPIC("4579e", TRACE, Logger::CLUSTER) <<
                      "Applying to cache " << rs.get("log").toJson();
                    for (auto const& i : VPackArrayIterator(rs.get("log"))) {
                      {
                        std::lock_guard g(_storeLock);
                        _readDB.applyTransaction(i); // apply logs
                        _commitIndex = i.get("index").getNumber<uint64_t>();
                      }
                      std::unordered_set<std::string> pc, cc;
                      {
                        std::lock_guard g(_callbacksLock);
                        handleCallbacksNoLock(i.get("query"), uniq, toCall, pc, cc);
                      }
                      {
                        std::lock_guard g(_storeLock);
                        for (auto const& i : pc) {
                          _planChanges.emplace(_commitIndex, i);
                        }
                        for (auto const& i : cc) {
                          _currentChanges.emplace(_commitIndex, i);
                        }
                      }
                    }
                  }
                } else {
                  TRI_ASSERT(rs.hasKey("readDB"));
                  std::lock_guard g(_storeLock);
                  LOG_TOPIC("4579f", TRACE, Logger::CLUSTER) <<
                    "Fresh start: overwriting agency cache with " << rs.toJson();
                  _readDB = rs;                  // overwrite
                  _commitIndex = commitIndex;
                }
                triggerWaiting(commitIndex);
                if (firstIndex > 0) {
                  if (!toCall.empty()) {
                    invokeCallbacks(toCall);
                  }
                } else {
                  invokeAllCallbacks();
                }
              } else {
                increaseWaitTime();
                LOG_TOPIC("9a93e", DEBUG, Logger::CLUSTER) <<
                  "Failed to get poll result from agency.";
              }
              return futures::makeFuture();
            })
          .thenError<VPackException>(
            [&increaseWaitTime](VPackException const& e) {
              LOG_TOPIC("9a9f3", ERR, Logger::CLUSTER) <<
                "Failed to parse poll result from agency: " << e.what();
              increaseWaitTime();
            })
          .thenError<std::exception>(
            [&increaseWaitTime](std::exception const& e) {
              LOG_TOPIC("9a9e3", ERR, Logger::CLUSTER) <<
                "Failed to get poll result from agency: " << e.what();
              increaseWaitTime();
            });
        ret.wait();
      } else {
        increaseWaitTime();
        LOG_TOPIC("9393e", DEBUG, Logger::CLUSTER) <<
          "Waiting for network feature to get ready";
      }

    } catch (std::exception const& e) {
      LOG_TOPIC("544da", ERR, Logger::CLUSTER) <<
        "Caught an error while polling agency updates: " << e.what();
      increaseWaitTime();
    } catch (...) {
      LOG_TOPIC("78916", ERR, Logger::CLUSTER) <<
        "Caught an error while polling agency updates";
      increaseWaitTime();
    }

    // off to next round we go...
  }

}

void AgencyCache::triggerWaiting(index_t commitIndex) {

  auto* scheduler = SchedulerFeature::SCHEDULER;
  std::lock_guard w(_waitLock);

  auto pit = _waiting.begin();
  while (pit != _waiting.end()) {
    if (pit->first > commitIndex) {
      break;
    }
    auto pp = std::make_shared<futures::Promise<Result>>(std::move(pit->second));
    if (scheduler && !this->isStopping()) {
      bool queued = scheduler->queue(
        RequestLane::CLUSTER_INTERNAL, [pp] { pp->setValue(Result()); });
      if (!queued) {
        LOG_TOPIC("c6473", WARN, Logger::AGENCY) <<
          "Failed to schedule logsForTrigger running in main thread";
        pp->setValue(Result());
      }
    } else {
      pp->setValue(Result(TRI_ERROR_SHUTTING_DOWN));
    }
    pit = _waiting.erase(pit);
  }

}

/// Register local callback
bool AgencyCache::registerCallback(std::string const& key, uint64_t const& id) {
  std::string const ckey = Store::normalize(AgencyCommHelper::path(key));
  LOG_TOPIC("67bb8", DEBUG, Logger::CLUSTER) << "Registering callback for " << ckey;

  size_t size = 0;
  {
    std::lock_guard g(_callbacksLock);
    _callbacks.emplace(ckey, id);
    size = _callbacks.size();
  }

  LOG_TOPIC("31415", TRACE, Logger::CLUSTER)
    << "Registered callback for " << ckey << " " << size;
  // this method always returns ok.
  return true;
}

/// Unregister local callback
void AgencyCache::unregisterCallback(std::string const& key, uint64_t const& id) {
  std::string const ckey = Store::normalize(AgencyCommHelper::path(key));
  LOG_TOPIC("cc768", DEBUG, Logger::CLUSTER) << "Unregistering callback for " << ckey;

  size_t size = 0;
  {
    std::lock_guard g(_callbacksLock);
    auto range = _callbacks.equal_range(ckey);
    for (auto it = range.first; it != range.second;) {
      if (it->second == id) {
        it = _callbacks.erase(it);
        break;
      } else {
        ++it;
      }
    }
    size = _callbacks.size();
  }

  LOG_TOPIC("034cc", TRACE, Logger::CLUSTER)
    << "Unregistered callback for " << ckey << " " << size;
}

/// Orderly shutdown
void AgencyCache::beginShutdown() {
  LOG_TOPIC("a63ae", TRACE, Logger::CLUSTER) << "Clearing books in agency cache";

  // trigger all waiting for an index
  {
    std::lock_guard g(_waitLock);
    auto pit = _waiting.begin();
    while (pit != _waiting.end()) {
      pit->second.setValue(Result(TRI_ERROR_SHUTTING_DOWN));
      ++pit;
    }
    _waiting.clear();
  }

  // trigger all callbacks
  {
    std::lock_guard g(_callbacksLock);
    for (auto const& i : _callbacks) {
      auto cb = _callbackRegistry.getCallback(i.second);
      if (cb != nullptr) {
        LOG_TOPIC("76bb8", DEBUG, Logger::CLUSTER)
          << "Agency callback " << i << " has been triggered. refetching!";
        try {
          cb->refetchAndUpdate(true, false);
        } catch (arangodb::basics::Exception const& e) {
          LOG_TOPIC("c3111", WARN, Logger::AGENCYCOMM)
            << "Error executing callback: " << e.message();
        }
      }
    }
    _callbacks.clear();
  }

  Thread::beginShutdown();
}

bool AgencyCache::has(std::string const& path) const {
  std::shared_lock_guard g(_storeLock);
  return _readDB.has(AgencyCommHelper::path(path));
}

std::vector<bool> AgencyCache::has(std::vector<std::string> const& paths) const {
  std::vector<bool> ret;
  ret.reserve(paths.size());
  std::shared_lock_guard g(_storeLock);
  for (auto const& i : paths) {
    ret.push_back(_readDB.has(i));
  }
  return ret;
}

void AgencyCache::invokeCallbackNoLock(uint64_t id, std::string const& key) const {
  auto cb = _callbackRegistry.getCallback(id);
  if (cb != nullptr) {
    try {
      cb->refetchAndUpdate(true, false);
      LOG_TOPIC("76aa8", DEBUG, Logger::CLUSTER)
        << "Agency callback " << id << " has been triggered. refetching " << key;
    } catch (arangodb::basics::Exception const& e) {
      LOG_TOPIC("c3091", WARN, Logger::AGENCYCOMM)
        << "Error executing callback: " << e.message();
    }
  }
}

void AgencyCache::invokeCallbacks(std::vector<uint64_t> const& toCall) const {
  for (auto i : toCall) {
    invokeCallbackNoLock(i);
  }
}

void AgencyCache::invokeAllCallbacks() const {
  std::vector<uint64_t> toCall;
  {
    std::lock_guard g(_callbacksLock);
    for (auto i : _callbacks) {
      toCall.push_back(i.second);
    }
  }
  invokeCallbacks(toCall);
}


AgencyCache::change_set_t const AgencyCache::planChangedSince(
  consensus::index_t const& last, std::vector<std::string> const& others) const {

  std::vector<consensus::query_t> db_res, rest_res;
  bool get_rest = false;

  std::vector<std::string> databases;
  for (auto const& db : others) {
    if (std::find(databases.begin(), databases.end(), db) == databases.end()) {
      databases.push_back(db);
    }
  }

  std::shared_lock_guard g(_storeLock);

  std::multimap<consensus::index_t, std::string>::const_iterator it =
    _planChanges.lower_bound(last);
  if (it != _planChanges.end()) {
    for (; it != _planChanges.end(); ++it) {
      if (it->second.empty()) { // Need to get rest of Plan
        get_rest = true;
      } 
      if (std::find(databases.begin(), databases.end(), it->second) == databases.end()) {
        databases.push_back(it->second);
      }
    }
    LOG_TOPIC("d5743", TRACE, Logger::CLUSTER) << "collecting " << databases << " from agency cache";
  } else {
    LOG_TOPIC("d5734", DEBUG, Logger::CLUSTER) << "no changed databases since " << last;
    return std::tuple(std::move(db_res), std::move(rest_res), _commitIndex);
  }

  if (databases.empty()) {
    return std::tuple(std::move(db_res), std::move(rest_res), _commitIndex);
  }

  auto query = std::make_shared<arangodb::velocypack::Builder>();
  {
    VPackArrayBuilder outer(query.get());
    for (auto const& i : databases) {
      VPackArrayBuilder inner(query.get());
      query->add(VPackValue(AgencyCommHelper::path(PLAN_ANALYZERS) + "/" + i));
      query->add(VPackValue(AgencyCommHelper::path(PLAN_COLLECTIONS) + "/" + i));
      query->add(VPackValue(AgencyCommHelper::path(PLAN_DATABASES) + "/" + i));
      query->add(VPackValue(AgencyCommHelper::path(PLAN_VIEWS) + "/" + i));
    }
  }
  if (_commitIndex > 0) { // Databases
    _readDB.read(query, db_res);
  }

  if (get_rest) { // All the rest, i.e. All keys of Plan excluding the usual suspects
    static std::vector<std::string> const exc {
      "Analyzers", "Collections", "Databases", "Views"};
    auto keys = _readDB.nodePtr(AgencyCommHelper::path(PLAN))->keys();
    keys.erase(
      std::remove_if(
        std::begin(keys), std::end(keys),
        [&] (auto x) {
          return std::binary_search(std::begin(exc), std::end(exc),x);}),
      std::end(keys));
    query->clear();
    {
      VPackArrayBuilder outer(query.get());
      for (auto const& i : keys) {
        VPackArrayBuilder inner(query.get());
        query->add(VPackValue(AgencyCommHelper::path(PLAN) + "/" + i));
      }
    }
    if (_commitIndex > 0) { // Databases
      _readDB.read(query, rest_res);
    }
    
  }
  return std::tuple(std::move(db_res), std::move(rest_res), _commitIndex);

}

AgencyCache::change_set_t const AgencyCache::currentChangedSince(
  consensus::index_t const& last, std::vector<std::string> const& others) const {

  std::vector<consensus::query_t> db_res, rest_res;

  std::vector<std::string> databases;
  for (auto const& i : others) {
    auto databasePath = AgencyCommHelper::path(CURRENT_COLLECTIONS + i);
    if (std::find(databases.begin(), databases.end(), databasePath) == databases.end()) {
      databases.push_back(databasePath);
    }
  }

  std::shared_lock_guard g(_storeLock);

  std::multimap<consensus::index_t, std::string>::const_iterator it =
    _currentChanges.lower_bound(last);
  if (it != _currentChanges.end()) {
    for (; it != _currentChanges.end(); it++) {
      auto databasePath = AgencyCommHelper::path(CURRENT_COLLECTIONS + it->second);
      if (std::find(databases.begin(), databases.end(), databasePath) == databases.end()) {
        databases.push_back(databasePath);
      }
    }
    LOG_TOPIC("d5473", TRACE, Logger::CLUSTER) << "collecting " << databases << " from agency cache";
  } else {
    LOG_TOPIC("d5374", DEBUG, Logger::CLUSTER) << "no changed databases since " << last;
    return std::tuple(std::move(db_res), std::move(rest_res), _commitIndex);
  }

  if (databases.empty()) {
    return std::tuple(std::move(db_res), std::move(rest_res), _commitIndex);
  }

  auto query = std::make_shared<arangodb::velocypack::Builder>();
  {
    VPackArrayBuilder outer(query.get());
    VPackArrayBuilder inner(query.get());
    for (auto const& i : databases) {
      query->add(VPackValue(i));
    }
  }
  if (_commitIndex > 0) {
    _readDB.read(query, db_res);
  }


  return std::tuple(std::move(db_res), std::move(rest_res), _commitIndex);
}


std::ostream& operator<<(std::ostream& o, AgencyCache::change_set_t const& c) {
  o << std::get<2>(c);
  return o;
}
