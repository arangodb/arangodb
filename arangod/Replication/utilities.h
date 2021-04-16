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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_UTILITIES_H
#define ARANGOD_REPLICATION_UTILITIES_H 1

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Basics/Result.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/ServerId.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace httpclient {
class GeneralClientConnection;
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

class Endpoint;
class ReplicationApplierConfiguration;
struct SyncerId;
class Syncer;

namespace replutils {

/// @brief base url of the replication API
extern std::string const ReplicationUrl;

struct Connection {
  Connection(Syncer* syncer, ReplicationApplierConfiguration const& applierConfig);

  /// @brief determine if the client connection is open and valid
  bool valid() const;

  /// @brief return the endpoint for the connection
  std::string const& endpoint() const;

  /// @brief identifier for local server
  std::string const& localServerId() const;

  /// @brief short informative string about the client
  std::string const& clientInfo() const;

  /// @brief Thread-safe aborted status
  void setAborted(bool value);

  /// @brief Thread-safe check aborted
  bool isAborted() const;

  /// @brief get an exclusive connection
  template <typename F>
  void lease(F&& func) & {
    std::lock_guard<std::mutex> guard(_mutex);
    std::forward<F>(func)(_client.get());
  }

  template <typename F>
  void lease(F&& func) const& {
    std::lock_guard<std::mutex> guard(_mutex);
    std::forward<F>(func)(_client.get());
  }

 private:
  std::string const _endpointString;
  std::string const _localServerId;
  std::string const _clientInfo;

  /// lock to protect client connection
  mutable std::mutex _mutex;
  /// @brief the http client we're using
  std::unique_ptr<httpclient::SimpleHttpClient> _client;
};

struct ProgressInfo {
  /// @brief signature of function to set/handle message
  typedef std::function<void(std::string const& msg)> Setter;

  /// @brief progress message
  std::string message{"not started"};
  /// @brief collections synced
  std::map<DataSourceId, std::string> processedCollections{};  // TODO worker safety

  // @brief constructor to optionally provide a setter/handler for messages
  explicit ProgressInfo(Setter);

  /// @brief set the new message using the setter provided at construction
  // TODO worker safety
  void set(std::string const& msg);

 private:
  std::mutex _mutex;
  Setter _setter;
};

struct LeaderInfo {
 private:
  LeaderInfo() = default; // used only internally

 public:
  std::string endpoint;
  std::string engine;  // storage engine (optional)
  ServerId serverId{0};
  int majorVersion{0};
  int minorVersion{0};
  TRI_voc_tick_t lastLogTick{0}; // only used during initialSync

  explicit LeaderInfo(ReplicationApplierConfiguration const& applierConfig);

  static LeaderInfo createEmpty() { return LeaderInfo(); }

  /// @brief returns major version number * 10000 + minor version number * 100
  uint64_t version() const;

  /// @brief get leader state
  Result getState(Connection& connection, bool isChildSyncer, char const* context);
};

struct BatchInfo {
  static constexpr double DefaultTimeout = 7200.0;

  /// @brief dump batch id
  uint64_t id{0};
  /// @brief ttl for batches
  int ttl{static_cast<int>(DefaultTimeout)};
  /// @brief dump batch last update time
  double updateTime{0};

  /// @brief send a "start batch" command
  /// @param patchCount try to patch count of this collection
  ///        only effective with the incremental sync
  Result start(Connection const& connection, ProgressInfo& progress, LeaderInfo& leader,
               SyncerId const& syncerId, char const* context, 
               std::string const& patchCount = "");

  /// @brief send an "extend batch" command
  Result extend(Connection const& connection, ProgressInfo& progress, SyncerId syncerId);

  /// @brief send a "finish batch" command
  // TODO worker-safety
  Result finish(Connection const& connection, ProgressInfo& progress, SyncerId syncerId);
};

/// @brief generates basic source headers for ClusterComm requests
std::unordered_map<std::string, std::string> createHeaders();

/// @brief whether or not the HTTP result is valid or not
bool hasFailed(httpclient::SimpleHttpResult* response);

/// @brief create an error result from a failed HTTP request/response
Result buildHttpError(httpclient::SimpleHttpResult* response,
                      std::string const& url, Connection const& connection);

/// @brief parse a velocypack response
Result parseResponse(velocypack::Builder&, httpclient::SimpleHttpResult const*);

}  // namespace replutils
}  // namespace arangodb

#endif
