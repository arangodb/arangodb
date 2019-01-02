////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REPLICATION_SYNCER_H
#define ARANGOD_REPLICATION_SYNCER_H 1

#include "Basics/Common.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/common-defines.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/ticks.h"

struct TRI_vocbase_t;

namespace arangodb {
class Endpoint;
class LogicalCollection;

namespace httpclient {
class GeneralClientConnection;
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

class Syncer {
 public:
  struct MasterInfo {
    std::string _endpoint;
    TRI_server_id_t _serverId;
    int _majorVersion;
    int _minorVersion;
    TRI_voc_tick_t _lastLogTick;
    bool _active;

    MasterInfo()
        : _serverId(0), _majorVersion(0), _minorVersion(0), _lastLogTick(0), _active(false) {}
  };

  Syncer(Syncer const&) = delete;
  Syncer& operator=(Syncer const&) = delete;

  explicit Syncer(ReplicationApplierConfiguration const&);

  virtual ~Syncer();

  /// @brief sleeps (nanoseconds)
  void sleep(uint64_t time) { usleep(static_cast<TRI_usleep_t>(time)); }

  /// @brief request location rewriter (injects database name)
  static std::string rewriteLocation(void*, std::string const&);

  /// @brief steal the barrier id from the syncer
  TRI_voc_tick_t stealBarrier();

  void setLeaderId(std::string const& leaderId) { _leaderId = leaderId; }

  /// @brief send a "remove barrier" command
  Result sendRemoveBarrier();

  void setAborted(bool value);

  virtual bool isAborted() const;

 protected:
  /// @brief reload all users
  void reloadUsers();

  /// @brief parse a velocypack response
  Result parseResponse(arangodb::velocypack::Builder&,
                       arangodb::httpclient::SimpleHttpResult const*) const;

  /// @brief send a "create barrier" command
  Result sendCreateBarrier(TRI_voc_tick_t);

  /// @brief send an "extend barrier" command
  Result sendExtendBarrier(TRI_voc_tick_t = 0);

  /// @brief apply a single marker from the collection dump
  Result applyCollectionDumpMarker(transaction::Methods&, LogicalCollection* coll,
                                   TRI_replication_operation_e,
                                   arangodb::velocypack::Slice const&,
                                   arangodb::velocypack::Slice const&);

  /// @brief creates a collection, based on the VelocyPack provided
  Result createCollection(TRI_vocbase_t* vocbase, arangodb::velocypack::Slice const&,
                          arangodb::LogicalCollection**);

  /// @brief drops a collection, based on the VelocyPack provided
  Result dropCollection(arangodb::velocypack::Slice const&, bool reportError);

  /// @brief creates an index, based on the VelocyPack provided
  Result createIndex(arangodb::velocypack::Slice const&);

  /// @brief drops an index, based on the VelocyPack provided
  Result dropIndex(arangodb::velocypack::Slice const&);

  /// @brief get master state
  Result getMasterState();

  /// @brief handle the state response of the master
  Result handleStateResponse(arangodb::velocypack::Slice const&);

  virtual TRI_vocbase_t* resolveVocbase(velocypack::Slice const&);

  LogicalCollection* resolveCollection(TRI_vocbase_t*,
                                       arangodb::velocypack::Slice const& slice);

  std::unordered_map<std::string, DatabaseGuard> const& vocbases() const {
    return _vocbases;
  }

  /// @brief whether or not the HTTP result is valid or not
  bool hasFailed(arangodb::httpclient::SimpleHttpResult* response) const;

  /// @brief create an error result from a failed HTTP request/response
  Result buildHttpError(arangodb::httpclient::SimpleHttpResult* response,
                        std::string const& url) const;

  /// we need to act like a 3.2 client
  bool simulate32Client() const;

 private:
  /// @brief extract the collection by either id or name, may return nullptr!
  LogicalCollection* getCollectionByIdOrName(TRI_vocbase_t*, TRI_voc_cid_t,
                                             std::string const&);

  /// @brief apply a single marker from the collection dump
  Result applyCollectionDumpMarkerInternal(transaction::Methods&, LogicalCollection* coll,
                                           TRI_replication_operation_e,
                                           arangodb::velocypack::Slice const&,
                                           arangodb::velocypack::Slice const&);

  /// @brief extract the collection id from VelocyPack
  TRI_voc_cid_t getCid(velocypack::Slice const&) const;

  /// @brief extract the collection name from VelocyPack
  std::string getCName(arangodb::velocypack::Slice const&) const;

 protected:
  /// @brief lazy loaded list of vocbases
  std::unordered_map<std::string, DatabaseGuard> _vocbases;

  /// @brief configuration
  ReplicationApplierConfiguration _configuration;

  /// @brief information about the master state
  MasterInfo _masterInfo;

  /// @brief the endpoint (master) we're connected to
  Endpoint* _endpoint;

  /// @brief the connection to the master
  httpclient::GeneralClientConnection* _connection;

  /// @brief a mutex for assigning and freeing the _client object
  mutable Mutex _clientMutex;

  /// @brief the http client we're using
  httpclient::SimpleHttpClient* _client;

  /// @brief database name
  std::string _databaseName;

  /// @brief local server id
  std::string _localServerIdString;

  /// @brief local server id
  TRI_server_id_t _localServerId;

  /// @brief WAL barrier id
  uint64_t _barrierId;

  /// @brief ttl for WAL barrier
  int _barrierTtl;

  /// @brief WAL barrier last update time
  double _barrierUpdateTime;

  /// Is this syncer allowed to handle its own batch
  bool _isChildSyncer;

  /// @brief leaderId, this is used in the cluster to the unique ID of the
  /// source server (the shard leader in this case). We need this information
  /// to apply the changes locally to a shard, which is configured as a
  /// follower and thus only accepts modifications that are replications
  /// from the leader. Leave empty if there is no concept of a "leader".
  std::string _leaderId;

  /// @brief base url of the replication API
  static std::string const ReplicationUrl;
};
}  // namespace arangodb

#endif
