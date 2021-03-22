////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCache.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/AgencyStrings.h"
#include "Agency/Node.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/application-exit.h"
#include "GeneralServer/RestHandler.h"
#include "RestServer/MetricsFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::consensus;

DECLARE_GAUGE(arangodb_agency_cache_callback_number, uint64_t, "Current number of entries in agency cache callbacks table");

AgencyCache::AgencyCache(application_features::ApplicationServer& server,
                         AgencyCallbackRegistry& callbackRegistry, ErrorCode shutdownCode)
    : Thread(server, "AgencyCache"),
    _commitIndex(0), 
    _readDB(server, nullptr, "readDB"),
    _shutdownCode(shutdownCode),
    _initialized(false), 
    _callbackRegistry(callbackRegistry), 
    _lastSnapshot(0),
    _callbacksCount(
      _server.getFeature<MetricsFeature>().add(arangodb_agency_cache_callback_number{})) {

#ifdef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_shutdownCode == TRI_ERROR_NO_ERROR || _shutdownCode == TRI_ERROR_SHUTTING_DOWN);
#else
  TRI_ASSERT(_shutdownCode == TRI_ERROR_SHUTTING_DOWN);
#endif
}

AgencyCache::~AgencyCache() {
  try {
    beginShutdown();
  } catch (...) {
    // unfortunately there is not much we can do here
  }
  shutdown();
}

bool AgencyCache::isSystem() const { return true; }

/// Start all agent thread
bool AgencyCache::start() {
  LOG_TOPIC("9a90f", DEBUG, Logger::AGENCY) << "Starting agency cache worker";
  Thread::start();
  return true;
}

// Fill existing Builder from readDB, mainly /Plan /Current
index_t AgencyCache::get(VPackBuilder& result, std::string const& path) const {
  result.clear();
  std::shared_lock g(_storeLock);
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
    std::shared_lock g(_storeLock);
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
  std::shared_lock g(_storeLock);

  if (_commitIndex > 0) {
    _readDB.read(query, result);
  }
  return std::tuple(std::move(result), _commitIndex);
}

futures::Future<arangodb::Result> AgencyCache::waitFor(index_t index) {
  std::shared_lock s(_storeLock);
  if (index <= _commitIndex) {
    return futures::makeFuture(arangodb::Result());
  }
  // intentionally don't release _storeLock here until we have inserted the promise
  std::lock_guard w(_waitLock);
  return _waiting.emplace(index, futures::Promise<arangodb::Result>())->second.getFuture();
}

index_t AgencyCache::index() const {
  std::shared_lock g(_storeLock);
  return _commitIndex;
}

// If a "arango/Plan/Databases" key is set, all databases in the Plan are completely
// replaced. This means that loadPlan and the maintenance thread have to revisit everything.
// In particular, we have to visit all databases in the new Plan as well as all currently
// existing databases locally! Therefore we fake all of these databases as if they were
// changed in this raft index.
std::unordered_set<std::string> AgencyCache::reInitPlan() {
  std::unordered_set<std::string> planChanges =
      server().getFeature<ClusterFeature>().allDatabases();  // local databases
  // And everything under /arango/Plan/Databases:
  auto keys = _readDB.nodePtr(AgencyCommHelper::path(PLAN) + "/" + DATABASES)->keys();
  for (auto const& dbname : keys) {
    planChanges.emplace(dbname);
  }
  planChanges.emplace();  // and the rest
  return planChanges;
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

    // Paths are normalized. We omit the first 8 characters for "/arango" + "/"
    const size_t offset = AgencyCommHelper::path(std::string()).size() + 1;
    if (k.size() > offset) {
      std::string_view r(k.c_str() + offset, k.size() - offset);
      auto rs = r.size();
      if (rs > strlen(PLAN) && r.compare(0, strlen(PLAN), PLAN) == 0) {
        if (rs >= strlen(PLAN_VERSION) &&                 // Plan/Version -> ignore
            r.compare(0, strlen(PLAN_VERSION), PLAN_VERSION) == 0) {
          continue;
        } else if (rs > strlen(PLAN_COLLECTIONS) &&       // Plan/Collections
                   r.compare(0, strlen(PLAN_COLLECTIONS), PLAN_COLLECTIONS) == 0) {
          auto tmp = r.substr(strlen(PLAN_COLLECTIONS));
          planChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (rs > strlen(PLAN_DATABASES) &&         // Plan/Databases
                   r.compare(0, strlen(PLAN_DATABASES), PLAN_DATABASES) == 0) {
          auto tmp = r.substr(strlen(PLAN_DATABASES));
          planChanges.emplace(tmp);
        } else if (rs > strlen(PLAN_VIEWS) &&             // Plan/Views
                   r.compare(0, strlen(PLAN_VIEWS), (PLAN_VIEWS)) == 0) {
          auto tmp = r.substr(strlen(PLAN_VIEWS));
          planChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (rs > strlen(PLAN_ANALYZERS) &&         // Plan/Analyzers
                   r.compare(0, strlen(PLAN_ANALYZERS), PLAN_ANALYZERS)==0) {
          auto tmp = r.substr(strlen(PLAN_ANALYZERS));
          planChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (r == "Plan/Databases" || r == "Plan/Collections" ||
                   r == "Plan/Views" || r == "Plan/Analyzers" ||
                   r == "Plan") {
          // !! Check documentation of the function before making changes here !!
          planChanges = reInitPlan();
        } else {
          planChanges.emplace();             // "" to indicate non database
        }
      } else if (rs > strlen(CURRENT) && r.compare(0, strlen(CURRENT), CURRENT) == 0) {
        if (rs >= strlen(CURRENT_VERSION) &&              // Current/Version is ignored
            r.compare(0, strlen(CURRENT_VERSION), CURRENT_VERSION) == 0) {
          continue;
        } else if (rs > strlen(CURRENT_COLLECTIONS) &&    // Current/Collections
                   r.compare(0, strlen(CURRENT_COLLECTIONS), CURRENT_COLLECTIONS) == 0) {
          auto tmp = r.substr(strlen(CURRENT_COLLECTIONS));
          currentChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else if (rs > strlen(CURRENT_DATABASES) &&      // Current/Databases
                   r.compare(0, strlen(CURRENT_DATABASES), CURRENT_DATABASES) == 0) {
          auto tmp = r.substr(strlen(CURRENT_DATABASES));
          currentChanges.emplace(tmp.substr(0,tmp.find(SLASH)));
        } else {
          currentChanges.emplace();          // "" to indicate non database
        }
      }
    }
  }
}


void AgencyCache::run() {
  using namespace std::chrono;
  
  TRI_ASSERT(AsyncAgencyCommManager::INSTANCE != nullptr);

  {
    std::shared_lock g(_storeLock);
    // technically it is not necessary to acquire the lock here,
    // as there shouldn't be any concurrency when we get here.
    // however, the code may change later, so let's play nice
    // and always access _commitIndex and _readDB under the lock,
    // as all other places in this file do.
    _commitIndex = 0;
    _readDB.clear();
  }

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
        std::shared_lock g(_storeLock);
        if (_commitIndex > 0) {
          // in the normal case, we will already have a commitIndex != 0.
          // however, on the first call, we need to call with commitIndex 0
          // and not 1 in order to get a full snapshot from the agency.
          // if we poll with a commitIndex value of 1, we will not get a
          // snapshot but only the changes since commitIndex 1, so we may
          // be missing data!
          commitIndex = _commitIndex + 1;
        }
      }
      LOG_TOPIC("afede", TRACE, Logger::CLUSTER)
          << "AgencyCache: poll polls: waiting for commitIndex " << commitIndex;
      // This is intentionally 61s timeout to avoid a client timeout, since
      // the server returns after 60s by default. This avoids broken
      // connections.
      return AsyncAgencyComm().poll(61s, commitIndex);
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
  std::unordered_set<std::string> pc;  // Plan changes
  std::unordered_set<std::string> cc;  // Current changes
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
      // * No change at all (server timeout)
      //   {..., result:{commitIndex:X, log:[]}}

      if (server().getFeature<NetworkFeature>().prepared()) {
        auto ret = sendTransaction()
          .thenValue(
            [&](AsyncAgencyCommResult&& rb) {
              if (!rb.ok() || rb.statusCode() != arangodb::fuerte::StatusOK) {
                // Error response, this includes client timeout
                increaseWaitTime();
                LOG_TOPIC("9a93e", DEBUG, Logger::CLUSTER) <<
                  "Failed to get poll result from agency.";
                  return futures::makeFuture();
              }
              // Correct response:
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
              index_t commitIndex = rs.get("commitIndex").getNumber<uint64_t>();
              VPackSlice firstIndexSlice = rs.get("firstIndex");
              if (!firstIndexSlice.isNumber()) {
                // Nothing happened at all, server timeout
                return futures::makeFuture();
              }
              index_t firstIndex = firstIndexSlice.getNumber<uint64_t>();
              if (firstIndex > 0) {
                // No snapshot, this is actual some log continuation
                TRI_ASSERT(_initialized);
                // Do incoming logs match our cache's index?
                if (firstIndex != curIndex + 1) {
                  LOG_TOPIC("a9a09", WARN, Logger::CLUSTER)
                    << "Logs from poll start with index " << firstIndex
                    << " we requested logs from and including " << curIndex
                    << " retrying.";
                  LOG_TOPIC("457e9", TRACE, Logger::CLUSTER)
                    << "Incoming: " << rs.toJson();
                  increaseWaitTime();
                  return futures::makeFuture();
                }
                TRI_ASSERT(rs.hasKey("log"));
                TRI_ASSERT(rs.get("log").isArray());
                LOG_TOPIC("4579e", TRACE, Logger::CLUSTER) <<
                  "Applying to cache " << rs.get("log").toJson();
                for (auto const& i : VPackArrayIterator(rs.get("log"))) {
                  pc.clear();
                  cc.clear();
                  {
                    std::lock_guard g(_storeLock);
                    _readDB.applyTransaction(i); // apply logs
                    _commitIndex = i.get("index").getNumber<uint64_t>();

                   {
                      std::lock_guard g(_callbacksLock);
                      handleCallbacksNoLock(i.get("query"), uniq, toCall, pc, cc);
                    }

                    for (auto const& i : pc) {
                      _planChanges.emplace(_commitIndex, i);
                    }
                    for (auto const& i : cc) {
                      _currentChanges.emplace(_commitIndex, i);
                    }
                  }
                }
              } else {
                // firstIndex == 0, we got a snapshot:
                TRI_ASSERT(rs.hasKey("readDB"));
                std::lock_guard g(_storeLock);
                LOG_TOPIC("4579f", TRACE, Logger::CLUSTER) <<
                  "Fresh start: overwriting agency cache with " << rs.toJson();
                _readDB = rs;                  // overwrite
                std::unordered_set<std::string> pc = reInitPlan();
                for (auto const& i : pc) {
                  _planChanges.emplace(_commitIndex, i);
                }
                // !! Check documentation of the function before making changes here !!
                _commitIndex = commitIndex;
                _lastSnapshot = commitIndex;
                _initialized.store(true, std::memory_order_relaxed);
              }
              triggerWaiting(commitIndex);
              if (firstIndex > 0) {
                if (!toCall.empty()) {
                  invokeCallbacks(toCall);
                }
              } else {
                invokeAllCallbacks();
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
      pp->setValue(Result(_shutdownCode));
    }
    pit = _waiting.erase(pit);
  }

}

/// Register local callback
Result AgencyCache::registerCallback(std::string const& key, uint64_t const& id) {
  std::string const ckey = Store::normalize(AgencyCommHelper::path(key));
  LOG_TOPIC("67bb8", DEBUG, Logger::CLUSTER) << "Registering callback for " << ckey;

  size_t size = 0;
  {
    std::lock_guard g(_callbacksLock);
    // insertion into the multimap should always succeed, except in case of OOM
    _callbacks.emplace(ckey, id);
    size = _callbacks.size();
    _callbacksCount = uint64_t(size);
  }


  LOG_TOPIC("31415", TRACE, Logger::CLUSTER)
    << "Registered callback for key " << ckey << " with id " << id << ", callbacks: " << size;
  // this method always returns ok, except in case of OOM (in this case it will throw).
  // to keep some API compatibility with AgencyCallbackRegistry::registerCallback(...), 
  // we make this function return a Result, too, even though it is not really needed here
  return {};
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
  _callbacksCount = uint64_t(size);
  }


  LOG_TOPIC("034cc", TRACE, Logger::CLUSTER)
    << "Unregistered callback for key " << ckey << " with id " << id << ", callbacks: " << size;
}

/// Orderly shutdown
void AgencyCache::beginShutdown() {
  LOG_TOPIC("a63ae", TRACE, Logger::CLUSTER) << "Clearing books in agency cache";

  // trigger all waiting for an index
  {
    std::lock_guard g(_waitLock);
    auto pit = _waiting.begin();
    while (pit != _waiting.end()) {
      pit->second.setValue(Result(_shutdownCode));
      ++pit;
    }
    _waiting.clear();
  }

  // trigger all callbacks
  while (true) {
    // this is a bit complicated... we pop one callback from the list of
    // available calls while holding the _callbacksLock, but we release the
    // lock afterwards, so that we can call into the _callbackRegistry without
    // holding the _callbacksLock
    uint64_t callbackId;
    {
      std::lock_guard g(_callbacksLock);
      if (_callbacks.empty()) {
        // we are intentionally _not_ setting the metric to 0 here, as the metrics
        // may already be unavailable. As we are in shutdown anyway here, this should
        // not cause major issues.
        // _callbacksCount = 0;
        break;
      }
      auto it = _callbacks.begin();
      TRI_ASSERT(it != _callbacks.end());
      callbackId = it->second;
      _callbacks.erase(it);
      // we are intentionally _not_ setting the metric to 0 here, as the metrics
      // may already be unavailable. As we are in shutdown anyway here, this should
      // not cause major issues.
      // _callbacksCount -= 1;
    } // release _callbacksLock

    auto cb = _callbackRegistry.getCallback(callbackId);
    if (cb != nullptr) {
      LOG_TOPIC("76bb8", DEBUG, Logger::CLUSTER)
        << "Agency callback " << callbackId << " has been triggered. refetching!";
      try {
        cb->refetchAndUpdate(true, false);
      } catch (arangodb::basics::Exception const& e) {
        LOG_TOPIC("c3111", WARN, Logger::AGENCYCOMM)
          << "Error executing callback: " << e.message();
      }
    }
  }

  Thread::beginShutdown();
}

bool AgencyCache::has(std::string const& path) const {
  std::shared_lock g(_storeLock);
  return _readDB.has(AgencyCommHelper::path(path));
}

std::vector<bool> AgencyCache::has(std::vector<std::string> const& paths) const {
  std::vector<bool> ret;
  ret.reserve(paths.size());
  std::shared_lock g(_storeLock);
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


void AgencyCache::clearChanged(std::string const& what, consensus::index_t const& doneIndex) {
  std::shared_lock g(_storeLock);
  decltype(_planChanges)& changes = (what == PLAN) ? _planChanges : _currentChanges;
  if (!changes.empty()) {
    changes.erase(changes.begin(), changes.upper_bound(doneIndex));
  }
}

AgencyCache::change_set_t AgencyCache::changedSince(
  std::string const& what, consensus::index_t const& last) const {

  static std::vector<std::string> const planGoodies ({
      AgencyCommHelper::path(PLAN_ANALYZERS) + "/",
      AgencyCommHelper::path(PLAN_COLLECTIONS) + "/",
      AgencyCommHelper::path(PLAN_DATABASES) + "/",
      AgencyCommHelper::path(PLAN_VIEWS) + "/"});
  static std::vector<std::string> const currentGoodies ({
      AgencyCommHelper::path(CURRENT_COLLECTIONS) + "/",
      AgencyCommHelper::path(CURRENT_DATABASES) + "/"});

  bool get_rest = false;
  std::unordered_map<std::string, query_t> db_res;
  query_t rest_res = nullptr;

  std::vector<std::string> const& goodies = (what == PLAN) ? planGoodies : currentGoodies;
  std::unordered_set<std::string> databases;

  std::shared_lock g(_storeLock);

  decltype(_planChanges) const& changes = (what == PLAN) ? _planChanges : _currentChanges;

  auto tmp = _readDB.nodePtr()->hasAsUInt(AgencyCommHelper::path(what) + "/" + VERSION);
  uint64_t version = tmp.second ? tmp.first : 0;

  if (last < _lastSnapshot || last == 0) {
    // we will get here when we call this with a "last" index value that is less than
    // the index of the last snapshot we got. in this case our changeset needs to
    // contain all databases.
    // in addition, in case the "last" value is 0, this is the initial call,
    // and we need to handle this in a special way, too.
    get_rest = true;
    auto keys = _readDB.nodePtr(AgencyCommHelper::path(what) + "/" + DATABASES)->keys();
    databases.reserve(keys.size());
    for (auto& i : keys) {
      databases.emplace(std::move(i));
    }
  } else {
    TRI_ASSERT(last != 0);
    auto it = changes.lower_bound(last + 1);
    if (it != changes.end()) {
      for (; it != changes.end(); ++it) {
        if (it->second.empty()) { // Need to get rest
          get_rest = true;
        }
        databases.emplace(it->second);
      }
      LOG_TOPIC("d5743", TRACE, Logger::CLUSTER)
        << "collecting " << databases << " from agency cache";
    } else {
      LOG_TOPIC("d5734", DEBUG, Logger::CLUSTER) << "no changed databases since " << last;
      return change_set_t(_commitIndex, version, std::move(db_res), std::move(rest_res));
    }
  }

  if (databases.empty()) {
    return change_set_t(_commitIndex, version, std::move(db_res), std::move(rest_res));
  }

  auto query = std::make_shared<arangodb::velocypack::Builder>();
  {
    for (auto const& i : databases) {
      if (!i.empty()) { // actual database
        { VPackArrayBuilder outer(query.get());
          { VPackArrayBuilder inner(query.get());
            for (auto const& g : goodies) { // Get goodies for database
              query->add(VPackValue(g + i));
            }
          }}
        auto [entry,created] = db_res.try_emplace(i, std::make_shared<VPackBuilder>());
        if (created) {
          _readDB.read(query, entry->second);
        } else {
          LOG_TOPIC("31ae3", ERR, Logger::CLUSTER)
            << "Failed to communicate updated database " << i
            << " in AgencyCache with maintenance.";
          FATAL_ERROR_EXIT();
        }
      }
      query->clear();
    }
  }

  if (get_rest) { // All the rest, i.e. All keys excluding the usual suspects
    static std::vector<std::string> const exc {
      "Analyzers", "Collections", "Databases", "Views"};
    auto keys = _readDB.nodePtr(AgencyCommHelper::path(what))->keys();
    keys.erase(
      std::remove_if(
        std::begin(keys), std::end(keys),
        [&] (auto const& x) {
          return std::find(std::begin(exc), std::end(exc), x) != std::end(exc);}),
      std::end(keys));
    {
      VPackArrayBuilder outer(query.get());
      for (auto const& i : keys) {
        VPackArrayBuilder inner(query.get());
        query->add(VPackValue(AgencyCommHelper::path(what) + "/" + i));
      }
    }
    if (_commitIndex > 0) { // Databases
      rest_res = std::make_shared<VPackBuilder>();
      _readDB.read(query, rest_res);
    }
  }

  return change_set_t(_commitIndex, version, std::move(db_res), std::move(rest_res));

}

namespace std {
ostream& operator<<(ostream& o, AgencyCache::change_set_t const& c) {
  o << c.ind;
  return o;
}

ostream& operator<<(ostream& o, AgencyCache::databases_t const& dbs) {
  o << "{";
  bool first = true;
  for (auto const& db : dbs) {
    if (!first) {
      o << ", ";
    } else {
      first = false;
    }
    o << '"' << db.first << "\": " << db.second->toJson();
  }
  o << "}";
  return o;
}
}
