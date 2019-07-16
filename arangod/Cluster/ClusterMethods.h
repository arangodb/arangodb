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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_METHODS_H
#define ARANGOD_CLUSTER_CLUSTER_METHODS_H 1

#include "Agency/AgencyComm.h"
#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Rest/GeneralResponse.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace velocypack {
template <typename T>
class Buffer;
class Builder;
}  // namespace velocypack

struct ClusterCommResult;
struct OperationOptions;
class TransactionState;
struct TtlStatistics;

/// @brief convert ClusterComm error into arango error code
int handleGeneralCommErrors(arangodb::ClusterCommResult const* res);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::string> getForwardableRequestHeaders(GeneralRequest*);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two vpack
/// documents
////////////////////////////////////////////////////////////////////////////////

bool shardKeysChanged(LogicalCollection const& collection, VPackSlice const& oldValue,
                      VPackSlice const& newValue, bool isPatch);

/// @brief check if the value of the smartJoinAttribute has changed
bool smartJoinAttributeChanged(LogicalCollection const& collection, VPackSlice const& oldValue,
                               VPackSlice const& newValue, bool isPatch);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int revisionOnCoordinator(std::string const& dbname,
                          std::string const& collname, TRI_voc_rid_t&);

////////////////////////////////////////////////////////////////////////////////
/// @brief Warmup index caches on Shards
////////////////////////////////////////////////////////////////////////////////

int warmupOnCoordinator(std::string const& dbname, std::string const& cid);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int figuresOnCoordinator(std::string const& dbname, std::string const& collname,
                         std::shared_ptr<arangodb::velocypack::Builder>&);

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator, by shard
////////////////////////////////////////////////////////////////////////////////

int countOnCoordinator(transaction::Methods& trx, std::string const& collname,
                       std::vector<std::pair<std::string, uint64_t>>& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the selectivity estimates from DBservers
////////////////////////////////////////////////////////////////////////////////

int selectivityEstimatesOnCoordinator(std::string const& dbname, std::string const& collname,
                                      std::unordered_map<std::string, double>& result,
                                      TRI_voc_tick_t tid = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

Result createDocumentOnCoordinator(transaction::Methods& trx, std::string const& collname,
                                   OperationOptions const& options,
                                   arangodb::velocypack::Slice const& slice,
                                   arangodb::rest::ResponseCode& responseCode,
                                   std::unordered_map<int, size_t>& errorCounters,
                                   std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int deleteDocumentOnCoordinator(transaction::Methods& trx, std::string const& collname,
                                VPackSlice const slice, OperationOptions const& options,
                                arangodb::rest::ResponseCode& responseCode,
                                std::unordered_map<int, size_t>& errorCounters,
                                std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getDocumentOnCoordinator(transaction::Methods& trx, std::string const& collname,
                             VPackSlice slice, OperationOptions const& options,
                             arangodb::rest::ResponseCode& responseCode,
                             std::unordered_map<int, size_t>& errorCounter,
                             std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

/// @brief fetch edges from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        All non-empty and non-cached results
///        of DBServers will be inserted in the
///        datalake. Slices used in the result
///        point to content inside of this lake
///        only and do not run out of scope unless
///        the lake is cleared.
///        TraversalVariant

int fetchEdgesFromEngines(
    std::string const&, std::unordered_map<ServerID, traverser::TraverserEngineID> const*,
    arangodb::velocypack::Slice vertexId, size_t,
    std::unordered_map<arangodb::velocypack::StringRef, arangodb::velocypack::Slice>&,
    std::vector<arangodb::velocypack::Slice>&,
    std::vector<std::shared_ptr<arangodb::velocypack::Builder>>&,
    arangodb::velocypack::Builder&, size_t&, size_t&);

/// @brief fetch edges from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        All non-empty and non-cached results
///        of DBServers will be inserted in the
///        datalake. Slices used in the result
///        point to content inside of this lake
///        only and do not run out of scope unless
///        the lake is cleared.
///        ShortestPathVariant

int fetchEdgesFromEngines(
    std::string const& dbname,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
    arangodb::velocypack::Slice vertexId, bool backward,
    std::unordered_map<arangodb::velocypack::StringRef, arangodb::velocypack::Slice>& cache,
    std::vector<arangodb::velocypack::Slice>& result,
    std::vector<std::shared_ptr<arangodb::velocypack::Builder>>& datalake,
    arangodb::velocypack::Builder& builder, size_t& read);

/// @brief fetch vertices from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        If any server responds with a document
///        it will be inserted into the result.
///        If no server responds with a document
///        a 'null' will be inserted into the result.

void fetchVerticesFromEngines(
    std::string const&, std::unordered_map<ServerID, traverser::TraverserEngineID> const*,
    std::unordered_set<arangodb::velocypack::StringRef>&,
    std::unordered_map<arangodb::velocypack::StringRef, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>&,
    arangodb::velocypack::Builder&);

/// @brief fetch vertices from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        If any server responds with a document
///        it will be inserted into the result.
///        If no server responds with a document
///        a 'null' will be inserted into the result.
///        ShortestPath Variant

void fetchVerticesFromEngines(
    std::string const&, std::unordered_map<ServerID, traverser::TraverserEngineID> const*,
    std::unordered_set<arangodb::velocypack::StringRef>&,
    std::unordered_map<arangodb::velocypack::StringRef, arangodb::velocypack::Slice>& result,
    std::vector<std::shared_ptr<arangodb::velocypack::Builder>>& datalake,
    arangodb::velocypack::Builder&);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a filtered set of edges on Coordinator.
///        Also returns the result in VelocyPack
////////////////////////////////////////////////////////////////////////////////

int getFilteredEdgesOnCoordinator(transaction::Methods const& trx,
                                  std::string const& collname, std::string const& vertex,
                                  TRI_edge_direction_e const& direction,
                                  arangodb::rest::ResponseCode& responseCode,
                                  arangodb::velocypack::Builder& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    transaction::Methods& trx, std::string const& collname,
    arangodb::velocypack::Slice const& slice, OperationOptions const& options,
    bool isPatch, std::unique_ptr<std::unordered_map<std::string, std::string>>& headers,
    arangodb::rest::ResponseCode& responseCode, std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

Result truncateCollectionOnCoordinator(transaction::Methods& trx, std::string const& collname);

////////////////////////////////////////////////////////////////////////////////
/// @brief flush Wal on all DBservers
////////////////////////////////////////////////////////////////////////////////

int flushWalOnAllDBServers(bool waitForSync, bool waitForCollector,
                           double maxWaitTime = -1.0);

/// @brief get TTL statistics from all DBservers and aggregate them
Result getTtlStatisticsFromAllDBServers(TtlStatistics& out);

/// @brief get TTL properties from all DBservers
Result getTtlPropertiesFromAllDBServers(arangodb::velocypack::Builder& out);

/// @brief set TTL properties on all DBservers
Result setTtlPropertiesOnAllDBServers(arangodb::velocypack::Slice const& properties,
                                      arangodb::velocypack::Builder& out);


//////////////////////////////////////////////////////////////////////////////
/// @brief create hotbackup on a coordinator
//////////////////////////////////////////////////////////////////////////////

enum HotBackupMode {CONSISTENT, DIRTY};

/**
 * @brief Create hot backup on coordinators
 * @param mode    Backup mode: consistent, dirty
 * @param timeout Wait for this attempt and bail out if not met
 */
arangodb::Result hotBackupCoordinator(
  VPackSlice const payload, VPackBuilder& report);

/**
 * @brief Restore specific hot backup on coordinators
 * @param mode    Backup mode: consistent, dirty
 * @param timeout Wait for this attempt and bail out if not met
 */
arangodb::Result hotRestoreCoordinator(VPackSlice const payload, VPackBuilder& report);

/**
 * @brief List all
 * @param mode    Backup mode: consistent, dirty
 * @param timeout Wait for this attempt and bail out if not met
 */
arangodb::Result listHotBackupsOnCoordinator(VPackSlice const payload, VPackBuilder& report);

/**
 * @brief Delete specific hot backup
 * @param backupId  BackupId to delete
 */
arangodb::Result deleteHotBackupsOnCoordinator(VPackSlice const payload, VPackBuilder& report);

#ifdef USE_ENTERPRISE
/**
 * @brief Trigger upload of specific hot backup
 * @param backupId  BackupId to delete
 */
arangodb::Result uploadBackupsOnCoordinator(VPackSlice const payload, VPackBuilder& report);

/**
 * @brief Trigger download of specific hot backup
 * @param backupId  BackupId to delete
 */
arangodb::Result downloadBackupsOnCoordinator(VPackSlice const payload, VPackBuilder& report);
#endif

/**
 * @brief match backup servers
 * @param  planDump   Dump of plan from backup
 * @param  dbServers  This cluster's db servers
 * @param  match      Matched db servers
 * @return            Operation's success
 */
arangodb::Result matchBackupServers(
  VPackSlice const planDump, std::vector<ServerID> const& dbServers,
  std::map<std::string,std::string>& match);

arangodb::Result matchBackupServersSlice(VPackSlice const planServers,
                                    std::vector<ServerID> const& dbServers,
                                    std::map<ServerID,ServerID>& match);

/**
 * @brief apply database server matches to plan
 * @param  plan     Plan from hot backup
 * @param  matches  Match backup's server ids to new server ids
 * @param  newPlan  Resulting new plan
 * @return          Operation's result
 */
arangodb::Result applyDBServerMatchesToPlan(
  VPackSlice const plan, std::map<ServerID,ServerID> const& matches,
  VPackBuilder& newPlan);

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the active journals for the collection on all DBservers
////////////////////////////////////////////////////////////////////////////////

int rotateActiveJournalOnAllDBServers(std::string const& dbname, std::string const& collname);

class ClusterMethods {
 public:
  // wrapper Class for static functions.
  // Cannot be instanciated.
  ClusterMethods() = delete;
  ~ClusterMethods() = delete;

  // @brief Create many new collections on coordinator from a Array of VPack
  // parameter Note that this returns a vector of newly allocated objects
  static std::vector<std::shared_ptr<LogicalCollection>> createCollectionOnCoordinator(
      TRI_vocbase_t& vocbase, arangodb::velocypack::Slice parameters,
      bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
      bool enforceReplicationFactor);

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Persist collection in Agency and trigger shard creation process
  ////////////////////////////////////////////////////////////////////////////////

  static std::vector<std::shared_ptr<LogicalCollection>> persistCollectionsInAgency(
      std::vector<std::shared_ptr<LogicalCollection>>& col, bool ignoreDistributeShardsLikeErrors,
      bool waitForSyncReplication, bool enforceReplicationFactor);
};

}  // namespace arangodb

#endif
