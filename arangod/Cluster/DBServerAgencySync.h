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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_DB_SERVER_AGENCY_SYNC_H
#define ARANGOD_CLUSTER_DB_SERVER_AGENCY_SYNC_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class HeartbeatThread;

struct DBServerAgencySyncResult {
  bool success;
  std::string errorMessage;
  uint64_t planIndex;
  uint64_t currentIndex;

  DBServerAgencySyncResult()
    : success(false), planIndex(0), currentIndex(0) {}

  DBServerAgencySyncResult(bool s, uint64_t pi, uint64_t ci)
    : success(s), planIndex(pi), currentIndex(ci) {}

  DBServerAgencySyncResult(bool s, std::string const& e, uint64_t pi, uint64_t ci)
    : success(s), errorMessage(e), planIndex(pi), currentIndex(ci) {}
};

class DBServerAgencySync {
  DBServerAgencySync(DBServerAgencySync const&) = delete;
  DBServerAgencySync& operator=(DBServerAgencySync const&) = delete;

 public:
  explicit DBServerAgencySync(application_features::ApplicationServer& server,
                              HeartbeatThread* heartbeat);

 public:
  void work();

  /**
   * @brief Get copy of current local state
   * @param  collections  Builder to fill to
   */
  arangodb::Result getLocalCollections(
    std::unordered_set<std::string> const& dirty,
    std::unordered_map<std::string, std::shared_ptr<VPackBuilder>>& collections);

 private:
  DBServerAgencySyncResult execute();

 private:
  application_features::ApplicationServer& _server;
  HeartbeatThread* _heartbeat;

};
}  // namespace arangodb

#endif
