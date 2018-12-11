////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
class HeartbeatThread;

struct DBServerAgencySyncResult {
  bool success;
  std::string errorMessage;
  uint64_t planVersion;
  uint64_t currentVersion;

  DBServerAgencySyncResult()
      : success(false), planVersion(0), currentVersion(0) {}

  DBServerAgencySyncResult(bool s, uint64_t p, uint64_t c)
      : success(s), planVersion(p), currentVersion(c) {}

  DBServerAgencySyncResult(bool s, std::string const& e, uint64_t p, uint64_t c)
      : success(s), errorMessage(e), planVersion(p), currentVersion(c) {}

  DBServerAgencySyncResult(const DBServerAgencySyncResult& other)
      : success(other.success),
        planVersion(other.planVersion),
        currentVersion(other.currentVersion) {}
};

class DBServerAgencySync {
  DBServerAgencySync(DBServerAgencySync const&) = delete;
  DBServerAgencySync& operator=(DBServerAgencySync const&) = delete;

 public:
  explicit DBServerAgencySync(HeartbeatThread* heartbeat);

 public:
  void work();

  /**
   * @brief Get copy of current local state
   * @param  collections  Builder to fill to
   */
  static arangodb::Result getLocalCollections(VPackBuilder& collections);
  
 private:
  DBServerAgencySyncResult execute();

 private:
  HeartbeatThread* _heartbeat;
};
}

#endif
