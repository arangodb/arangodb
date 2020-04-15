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
#include "GeneralServer/RestHandler.h"

using namespace arangodb;
using namespace arangodb::consensus;

AgencyCache::AgencyCache(
  application_features::ApplicationServer& server,
  AgencyCallbackRegistry& callbackRegistry)
  : Thread(server, "AgencyCache"), _commitIndex(0),
    _readDB(server, nullptr, "raadDB"), _callbackRegistry(callbackRegistry) {}


AgencyCache::~AgencyCache() {}

/// Start all agent thread
bool AgencyCache::start() {
  LOG_TOPIC("9a90f", DEBUG, Logger::AGENCY) << "Starting agency cache worker.";
  Thread::start();
  return true;
}

// Get Builder from readDB mainly /Plan /Current
std::tuple <query_t, index_t> const AgencyCache::get(
  std::string const& path) const {
  std::lock_guard g(_storeLock);
  auto ret = std::make_shared<VPackBuilder>();
  if (_commitIndex > 0) {
    _readDB.get("/arango/" + path).toBuilder(*ret);
  }
  return std::tuple(ret, _commitIndex);
}

// Get Builder from readDB mainly /Plan /Current
std::tuple <query_t, index_t> const AgencyCache::get(
  std::vector<std::string> const& paths) const {
  std::lock_guard g(_storeLock);

  auto result = std::make_shared<arangodb::velocypack::Builder>();
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

index_t AgencyCache::index() const {
  std::lock_guard g(_storeLock);
  return _commitIndex;
}

void AgencyCache::run() {

  using namespace std::chrono;
  TRI_ASSERT(AsyncAgencyCommManager::INSTANCE != nullptr);

  _commitIndex = 0;
  _readDB.clear();
  double wait = 0.0;
  VPackBuilder rb;
  std::vector<uint32_t> toCall;
  // Long poll to agency
  auto sendTransaction =
    [&](VPackBuilder& r) {
      index_t commitIndex = 0;
      {
        std::lock_guard g(_storeLock);
        commitIndex = _commitIndex;
      }
      auto ret = AsyncAgencyComm().poll(60s, commitIndex+1);
      //r.add(ret.value());
      return ret;
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

  while (!this->isStopping()) {
    
    rb.clear();
    toCall.clear();
    std::this_thread::sleep_for(std::chrono::duration<double>(wait));
    auto ret = sendTransaction(rb)
      .thenValue(
        [&](AsyncAgencyCommResult&& rb) {
          if (rb.ok() && rb.statusCode() == arangodb::fuerte::StatusOK) {
            auto tmp = VPackParser::fromJson(rb.payloadAsString());
            auto slc = tmp->slice();
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
              TRI_ASSERT(rs.hasKey("log"));
              TRI_ASSERT(rs.get("log").isArray());
              std::lock_guard g(_storeLock);
              for (auto const& i : VPackArrayIterator(rs.get("log"))) {
                _readDB.applyTransaction(i); // apply logs
                for (auto const& q : VPackObjectIterator(i.get("query"))) {
                  auto const& key = q.key.copyString();
                  std::lock_guard g(_callbacksLock);
                  for (auto i : _callbacks) {
                    if (key.rfind(i.first, 0) == 0) {
                      toCall.push_back(i.second);
                    }
                  }
                }
              }
              _commitIndex = commitIndex;
            } else {
              TRI_ASSERT(rs.hasKey("readDB"));
              _readDB = rs;                  // overwrite
              _commitIndex = commitIndex;
            }
            for (auto i : toCall) {
              auto cb = _callbackRegistry.getCallback(i);
              if (cb.get() != nullptr) {
                LOG_TOPIC("76aa8", DEBUG, Logger::CLUSTER)
                  << "Agency callback " << i << " has been triggered. refetching!";
                try {
                  cb->refetchAndUpdate2(true, false);
                } catch (arangodb::basics::Exception const& e) {
                  LOG_TOPIC("c3910", WARN, Logger::AGENCYCOMM)
                    << "Error executing callback: " << e.message();
                }
              }
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


/// Register local call back
bool AgencyCache::registerCallback(std::string const& key, uint32_t const& id) {
  LOG_TOPIC("67bb8", DEBUG, Logger::CLUSTER)
    << "Registering callback for " <<  AgencyCommManager::path(key);
  std::lock_guard g(_callbacksLock);
  _callbacks.try_emplace(AgencyCommManager::path(key),id);
  return true;
}

/// Register local call back
bool AgencyCache::unregisterCallback(std::string const& key) {
  LOG_TOPIC("cc768", DEBUG, Logger::CLUSTER)
    << "Registering callback for " <<  AgencyCommManager::path(key);
  std::lock_guard g(_callbacksLock);
  _callbacks.erase(AgencyCommManager::path(key));
  return true;
}

/// Orderly shutdown
void AgencyCache::beginShutdown() {
  Thread::beginShutdown();
}
