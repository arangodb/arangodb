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
#include "Replication/utilities.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/ticks.h"

struct TRI_vocbase_t;

namespace arangodb {
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

class Endpoint;
class LogicalCollection;

class Syncer {
 public:
  struct SyncerState {
    /// @brief configuration
    ReplicationApplierConfiguration applier;

    /// @brief information about the replication barrier
    replutils::BarrierInfo barrier{};

    /// @brief object holding the HTTP client and all connection machinery
    replutils::Connection connection;

    /// @brief database name
    std::string databaseName{};

    /// Is this syncer allowed to handle its own batch
    bool isChildSyncer{false};

    /// @brief leaderId, this is used in the cluster to the unique ID of the
    /// source server (the shard leader in this case). We need this information
    /// to apply the changes locally to a shard, which is configured as a
    /// follower and thus only accepts modifications that are replications
    /// from the leader. Leave empty if there is no concept of a "leader".
    std::string leaderId{};

    /// @brief local server id
    TRI_server_id_t localServerId{0};

    /// @brief local server id
    std::string localServerIdString{};

    /// @brief information about the master state
    replutils::MasterInfo master;

    /// @brief lazy loaded list of vocbases
    std::unordered_map<std::string, DatabaseGuard> vocbases{};

    SyncerState(Syncer*, ReplicationApplierConfiguration const&);
  };

  Syncer(Syncer const&) = delete;
  Syncer& operator=(Syncer const&) = delete;

  explicit Syncer(ReplicationApplierConfiguration const&);

  virtual ~Syncer();

  /// @brief request location rewriter (injects database name)
  static std::string rewriteLocation(void*, std::string const&);

  /// @brief steal the barrier id from the syncer
  TRI_voc_tick_t stealBarrier();

  void setLeaderId(std::string const& leaderId) { _state.leaderId = leaderId; }

  /// @brief send a "remove barrier" command
  Result sendRemoveBarrier();

  // TODO worker-safety
  void setAborted(bool value);

  // TODO worker-safety
  virtual bool isAborted() const;

 public:
 protected:
  /// @brief reload all users
  // TODO worker safety
  void reloadUsers();

  /// @brief apply a single marker from the collection dump
  // TODO worker-safety
  Result applyCollectionDumpMarker(transaction::Methods&,
                                   LogicalCollection* coll,
                                   TRI_replication_operation_e,
                                   arangodb::velocypack::Slice const&);

  /// @brief creates a collection, based on the VelocyPack provided
  // TODO worker safety - create/drop phase
  Result createCollection(TRI_vocbase_t& vocbase,
                          arangodb::velocypack::Slice const& slice,
                          arangodb::LogicalCollection** dst);

  /// @brief drops a collection, based on the VelocyPack provided
  // TODO worker safety - create/drop phase
  Result dropCollection(arangodb::velocypack::Slice const&, bool reportError);

  /// @brief creates an index, based on the VelocyPack provided
  Result createIndex(arangodb::velocypack::Slice const&);

  /// @brief drops an index, based on the VelocyPack provided
  Result dropIndex(arangodb::velocypack::Slice const&);

  // TODO worker safety
  virtual TRI_vocbase_t* resolveVocbase(velocypack::Slice const&);

  // TODO worker safety
  std::shared_ptr<LogicalCollection> resolveCollection(
      TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& slice);

  // TODO worker safety
  std::unordered_map<std::string, DatabaseGuard> const& vocbases() const {
    return _state.vocbases;
  }

 protected:
  /// @brief state information for the syncer
  SyncerState _state;
};

}  // namespace arangodb

#endif
