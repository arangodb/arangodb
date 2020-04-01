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

AgencyCache::AgencyCache(application_features::ApplicationServer& server)
  : Thread(server, "AgencyCache"), _commitIndex(0),  _readDB(server, nullptr, "raadDB") {}

RestStatus AgencyCache::waitForFuture(futures::Future<futures::Unit>&& f) {
  if (f.isReady()) {             // fast-path out
    f.result().throwIfFailed();  // just throw the error upwards
    return RestStatus::DONE;
  }
  bool done = false;
/*  std::move(f).thenFinal(
    [self = shared_from_this(), &done](futures::Try<T>) -> void {
      auto thisPtr = self.get();
      if (std::this_thread::get_id() == thisPtr->_executionMutexOwner.load()) {
        done = true;
      } else {
        thisPtr->wakeupHandler();
      }
    });
  */
  return done ? RestStatus::DONE : RestStatus::WAITING;
}


// Get Builder from readDB mainly /Plan /Current
query_t const AgencyCache::get(std::string const& path) const {
  std::lock_guard g(_lock);
  query_t builder;
  _readDB.toBuilder(*builder);
  return builder;
}

void AgencyCache::run() {

  _commitIndex = 0;
  _readDB.clear();
  double wait = 0.0;

  using namespace std::chrono;

  TRI_ASSERT(AsyncAgencyCommManager::INSTANCE != nullptr);

  auto sendTransaction =
    [&](VPackBuilder& r) {
      index_t commitIndex = 0;
      {
        std::lock_guard g(_lock);
        commitIndex = _commitIndex;
      }
      auto ret = AsyncAgencyComm().poll(60s, commitIndex);
      //r.add(ret.value());
      return ret;
    };
  //auto self(shared_from_this());
  VPackBuilder rb;


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
    std::this_thread::sleep_for(std::chrono::duration<double>(wait));

    auto ret = waitForFuture(
      sendTransaction(rb)
      .thenValue(
        [&](AsyncAgencyCommResult&& rb) {
          if (rb.ok() && rb.statusCode() == 200) {
            wait = 0.0;
            TRI_ASSERT(rb.slice().hasKey("result"));
            VPackSlice rs = rb.slice().get("result");
            TRI_ASSERT(rs.hasKey("commitIndex"));
            TRI_ASSERT(rs.get("commitIndex").isNumber());
            TRI_ASSERT(rs.hasKey("firstIndex"));
            TRI_ASSERT(rs.get("firstIndex").isNumber());
            consensus::index_t commitIndex = rs.get("commitIndex").getNumber<uint64_t>();
            consensus::index_t firstIndex = rs.get("firstIndex").getNumber<uint64_t>();
            std::lock_guard g(_lock);
            if (firstIndex > 0) {
              TRI_ASSERT(rs.hasKey("log"));
              TRI_ASSERT(rs.get("log").isArray());
              for (auto const& i : VPackArrayIterator(rs.get("log"))) {
                _readDB.applyTransaction(i); // apply logs
              }
              _commitIndex = commitIndex; 
            } else {
              TRI_ASSERT(rs.hasKey("readDB"));
              _readDB = rs;                  // overwrite
            }
          } else {
            wait += 0.1;
            LOG_TOPIC("9a9e3", DEBUG, Logger::CLUSTER) <<
              "Failed to get poll result from agency.";
          }
          return futures::makeFuture();
        })
      .thenError<VPackException>(
        [&wait](VPackException const& e) {
          LOG_TOPIC("9a9e3", DEBUG, Logger::CLUSTER) <<
            "Failed to parse poll result from agency: " << e.what();
          wait += 0.1;
        })
      .thenError<std::exception>(
        [&wait](std::exception const& e) {
          LOG_TOPIC("9a9e3", DEBUG, Logger::CLUSTER) <<
            "Failed to get poll result from agency: " << e.what();
          wait += 0.1;
        }));

    LOG_TOPIC("a4a4a", DEBUG, Logger::CLUSTER) << (int)ret;

  }

}
