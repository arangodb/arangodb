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

#include "AgencyCache.cpp"

AgencyCache::AgencyCache(application_features::ApplicationServer& server)
  : _agency(server), _commitIndex(0),  _readDB(server, nullptr, "raadDB") {}

// Get Builder from readDB mainly /Plan /Current
VPackBuider const get(std::string const& path) const {
  std::shared_lock(_lock);

  // _readDB.toBuilder(path);
}

void run() {

  _commitIndex = 0;
  _readDB.clear();
  double wait = 0.0;

  using std::chrono;

  TRI_ASSERT(AsyncAgencyCommManager::INSTANCE != nullptr);

  steady_clock::time_point last =
  auto arangoPath = arangodb::cluster::paths::root()->arango();
  auto sendTransaction =
    [&](VPackBuilder& r) {
      auto ret = AsyncAgencyComm().getValue(60s, arangoPath);
      r.add(ret.value());
      return ret;
    };
  auto self(shared_from_this());
  VPackBuilder result;

  while (!this->isStopping()) {

    builder.clear();
    std::this_thread::sleep_for(std::chrono::duration<double>(wait));

    auto ret = waitForFuture(
      sendTransaction(builder)
      .thenValue(
        [this, wantToActive](AsyncAgencyCommResult&& result) {
          if (result.ok() && result.statusCode() == 200) {
            _readDB.applyTransactions(result.get(logs));
            wait = 0.0;
          } else {
            wait += 0.1;
            LOG_TOPIC("9a9e3", DEBUG, Logger::Cluster) <<
              "Failed to get poll result from agency: " << e.what();
          }
          return futures::makeFuture();
        })
      .thenError<VPackException>(
        [this](VPackException const& e) {
          LOG_TOPIC("9a9e3", DEBUG, Logger::Cluster) <<
            "Failed to parse poll result from agency: " << e.what();
          wait += 0.1;
        })
      .thenError<std::exception>(
        [this](std::exception const& e) {
          LOG_TOPIC("9a9e3", DEBUG, Logger::Cluster) <<
            "Failed to get poll result from agency: " << e.what();
          wait += 0.1;
        }));

  }

}
