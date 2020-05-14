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
#include "Agency/Node.h"
#include "GeneralServer/RestHandler.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::consensus;

AgencyCache::AgencyCache(
  application_features::ApplicationServer& server,
  AgencyCallbackRegistry& callbackRegistry)
  : Thread(server, "AgencyCache"), _commitIndex(0),
    _readDB(server, nullptr, "readDB"), _callbackRegistry(callbackRegistry) {}


AgencyCache::~AgencyCache() {
  beginShutdown();
}

/// Start all agent thread
bool AgencyCache::start() {
  LOG_TOPIC("9a90f", DEBUG, Logger::AGENCY) << "Starting agency cache worker.";
  Thread::start();
  return true;
}

// Get Builder from readDB mainly /Plan /Current
std::tuple <query_t, index_t> const AgencyCache::get(
  std::string const& path) const {
  auto ret = std::make_shared<VPackBuilder>();
  std::lock_guard g(_storeLock);
  if (_commitIndex > 0) {
    _readDB.get("arango/" + path).toBuilder(*ret);
  }
  return std::tuple(ret, _commitIndex);
}

// Get Builder from readDB mainly /Plan /Current
query_t const AgencyCache::dump() const {
  auto ret = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder o(ret.get());
    std::lock_guard g(_storeLock);
    ret->add("index", VPackValue(_commitIndex));
    ret->add(VPackValue("cache"));
    auto query = std::make_shared<arangodb::velocypack::Builder>();
    {
      VPackArrayBuilder outer(query.get());
      VPackArrayBuilder inner(query.get());
      query->add(VPackValue("/"));
    }
    _readDB.read(query, ret);
  }
  return ret;
}

// Get Builder from readDB mainly /Plan /Current
std::tuple <query_t, index_t> const AgencyCache::read(
  std::vector<std::string> const& paths) const {
  auto result = std::make_shared<arangodb::velocypack::Builder>();
  std::lock_guard g(_storeLock);

  if (_commitIndex > 0) {
    auto query = std::make_shared<arangodb::velocypack::Builder>();
    {
      VPackArrayBuilder outer(query.get());
      VPackArrayBuilder inner(query.get());
      for (auto const& i : paths) {
        query->add(VPackValue(i));
      }
    }
    _readDB.read(query, result);
  }
  return std::tuple(result, _commitIndex);
}

futures::Future<arangodb::Result> AgencyCache::waitFor(index_t index) {
  std::lock_guard s(_storeLock);
  if (index <= _commitIndex) {
    return futures::makeFuture(arangodb::Result());
  }
  // intentionally don't release _storeLock here until we have inserted the promise
  std::lock_guard w(_waitLock);
  return _waiting.emplace(index, futures::Promise<arangodb::Result>())->second.getFuture();
}

index_t AgencyCache::index() const {
  std::lock_guard g(_storeLock);
  return _commitIndex;
}


void AgencyCache::handleCallbacksNoLock(VPackSlice slice, std::unordered_set<uint64_t>& uniq, std::vector<uint64_t>& toCall) {

  if (!slice.isObject()) {
    LOG_TOPIC("31514", DEBUG, Logger::CLUSTER) <<
      "Cannot handle callback on non-object " << slice.toJson();
    return;
  }

  // Collect and normalize keys
  std::vector<std::string> keys;
  keys.reserve(slice.length());
  for (auto const& i : VPackObjectIterator(slice)) {
    keys.push_back(Node::normalize(i.key.copyString()));
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
  // Find keys, which are a prefix of a callback:
  for (auto const& k : keys) {
    auto it = _callbacks.lower_bound(k);
    while (it != _callbacks.end() && it->first.compare(0, k.size(), k) == 0) {
      if (uniq.emplace(it->second).second) {
        toCall.push_back(it->second);
      }
      ++it;
    }
  }
}

void AgencyCache::run() {

  using namespace std::chrono;
  TRI_ASSERT(AsyncAgencyCommManager::INSTANCE != nullptr);

  _commitIndex = 0;
  _readDB.clear();
  double wait = 0.0;

  // Long poll to agency
  auto sendTransaction =
    [&]() {
      index_t commitIndex = 0;
      {
        std::lock_guard g(_storeLock);
        commitIndex = _commitIndex;
      }
      LOG_TOPIC("afede", TRACE, Logger::CLUSTER)
          << "AgencyCache: poll polls: waiting for commitIndex " << commitIndex + 1;
      return AsyncAgencyComm().poll(60s, commitIndex + 1);
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

    uniq.clear();
    toCall.clear();
    std::this_thread::sleep_for(std::chrono::duration<double>(wait));

    // rb holds receives either
    // * A complete overwrite (firstIndex == 0)
    //   {..., result:{commitIndex:X, firstIndex:0, readDB:{...}}}
    // * Incremental change to cache (firstIndex != 0)
    //   {..., result:{commitIndex:X, firstIndex:Y, log:[]}}

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
              if (firstIndex != curIndex+1) {
                LOG_TOPIC("a9a09", WARN, Logger::CLUSTER) <<
                  "Logs from poll start with index " << firstIndex <<
                  " we requested logs from and including " << curIndex << " retrying.";
                LOG_TOPIC("457e9", TRACE, Logger::CLUSTER) << "Incoming: " << rs.toJson();
                if (wait <= 1.9) {
                  wait += 0.1;
                }
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
                  {
                    std::lock_guard g(_callbacksLock);
                    handleCallbacksNoLock(i.get("query"), uniq, toCall);
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
            if (wait <= 1.9) {
              wait += 0.1;
            }
            LOG_TOPIC("9a93e", DEBUG, Logger::CLUSTER) <<
              "Failed to get poll result from agency.";
          }
          return futures::makeFuture();
        })
      .thenError<VPackException>(
        [&wait](VPackException const& e) {
          LOG_TOPIC("9a9f3", ERR, Logger::CLUSTER) <<
            "Failed to parse poll result from agency: " << e.what();
          if (wait <= 1.9) {
            wait += 0.1;
          }
        })
      .thenError<std::exception>(
        [&wait](std::exception const& e) {
          LOG_TOPIC("9a9e3", ERR, Logger::CLUSTER) <<
            "Failed to get poll result from agency: " << e.what();
          if (wait <= 1.9) {
            wait += 0.1;
          }
        });
    ret.wait();

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
    if (!this->isStopping()) {
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


/// Register local call back
bool AgencyCache::registerCallback(std::string const& key, uint64_t const& id) {
  std::string const ckey = Node::normalize(AgencyCommHelper::path(key));
  LOG_TOPIC("67bb8", DEBUG, Logger::CLUSTER) << "Registering callback for " << ckey;
  std::lock_guard g(_callbacksLock);
  _callbacks.emplace(ckey,id);
  LOG_TOPIC("31415", TRACE, Logger::CLUSTER)
    << "Registered callback for " << ckey << " " << _callbacks.size();
  return true;
}

/// Register local call back
bool AgencyCache::unregisterCallback(std::string const& key, uint64_t const& id) {
  std::string const ckey = Node::normalize(AgencyCommHelper::path(key));
  LOG_TOPIC("cc768", DEBUG, Logger::CLUSTER) << "Unregistering callback for " << ckey;
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
  LOG_TOPIC("034cc", TRACE, Logger::CLUSTER)
    << "Unregistered callback for " << ckey << " " << _callbacks.size();
  return true;
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
      if (cb.get() != nullptr) {
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
  std::lock_guard g(_storeLock);
  return _readDB.has(AgencyCommHelper::path(path));
}

std::vector<bool> AgencyCache::has(std::vector<std::string> const& paths) const {
  std::vector<bool> ret;
  ret.reserve(paths.size());
  std::lock_guard g(_storeLock);
  for (auto const& i : paths) {
    ret.push_back(_readDB.has(i));
  }
  return ret;
}


void AgencyCache::invokeCallbackNoLock(uint64_t id, std::string const& key) const {
  auto cb = _callbackRegistry.getCallback(id);
  if (cb.get() != nullptr) {
    try {
      cb->refetchAndUpdate(true, false);
      LOG_TOPIC("76aa8", DEBUG, Logger::CLUSTER)
        << "Agency callback " << id << " has been triggered. refetching " + key;
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
