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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_METHODS_H
#define ARANGOD_CLUSTER_CLUSTER_METHODS_H 1

#include "Basics/Common.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Rest/HttpResponse.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace velocypack {
template <typename T>
class Buffer;
class Builder;
class Slice;
}

namespace traverser {
class TraverserExpression;
}

struct OperationOptions;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::string> getForwardableRequestHeaders(
    GeneralRequest*);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two vpack
/// documents
////////////////////////////////////////////////////////////////////////////////

bool shardKeysChanged(std::string const& dbname, std::string const& collname,
                      VPackSlice const& oldValue, VPackSlice const& newValue,
                      bool isPatch);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int revisionOnCoordinator(std::string const& dbname,
                          std::string const& collname, TRI_voc_rid_t&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int figuresOnCoordinator(std::string const& dbname, std::string const& collname,
                         std::shared_ptr<arangodb::velocypack::Builder>&);

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

int countOnCoordinator(std::string const& dbname, std::string const& collname,
                       uint64_t& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    OperationOptions const& options, arangodb::velocypack::Slice const& slice,
    arangodb::rest::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounters,
    std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int deleteDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    VPackSlice const slice, OperationOptions const& options,
    arangodb::rest::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounters,
    std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    VPackSlice const slice, OperationOptions const& options,
    std::unique_ptr<std::unordered_map<std::string, std::string>>& headers,
    arangodb::rest::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a list of filtered documents in a coordinator
///        All found documents will be inserted into result.
///        After execution documentIds will contain all id's of documents
///        that could not be found.
////////////////////////////////////////////////////////////////////////////////

int getFilteredDocumentsOnCoordinator(
    std::string const& dbname,
    std::vector<traverser::TraverserExpression*> const& expressions,
    std::unordered_set<std::string>& documentIds,
    std::unordered_map<std::string,
                       std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>&
        result);

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

int fetchEdgesFromEngines(
    std::string const&,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const*,
    arangodb::velocypack::Slice const, size_t,
    std::unordered_map<arangodb::velocypack::Slice,
                       arangodb::velocypack::Slice>&,
    std::vector<arangodb::velocypack::Slice>&,
    std::vector<std::shared_ptr<arangodb::velocypack::Builder>>&,
    arangodb::velocypack::Builder&, size_t&, size_t&);

/// @brief fetch vertices from TraverserEngines
///        Contacts all TraverserEngines placed
///        on the DBServers for the given list
///        of vertex _id's.
///        If any server responds with a document
///        it will be inserted into the result.
///        If no server responds with a document
///        a 'null' will be inserted into the result.

void fetchVerticesFromEngines(
    std::string const&,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const*,
    std::unordered_set<arangodb::velocypack::Slice>&,
    std::unordered_map<arangodb::velocypack::Slice,
                       std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>&,
    arangodb::velocypack::Builder&);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a filtered set of edges on Coordinator.
///        Also returns the result in VelocyPack
////////////////////////////////////////////////////////////////////////////////

int getFilteredEdgesOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& vertex, TRI_edge_direction_e const& direction,
    std::vector<traverser::TraverserExpression*> const& expressions,
    arangodb::rest::ResponseCode& responseCode,
    arangodb::velocypack::Builder& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    arangodb::velocypack::Slice const& slice, OperationOptions const& options,
    bool isPatch,
    std::unique_ptr<std::unordered_map<std::string, std::string>>& headers,
    arangodb::rest::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<arangodb::velocypack::Builder>& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

int truncateCollectionOnCoordinator(std::string const& dbname,
                                    std::string const& collname);

////////////////////////////////////////////////////////////////////////////////
/// @brief flush Wal on all DBservers
////////////////////////////////////////////////////////////////////////////////

int flushWalOnAllDBServers(bool, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a shard distribution for a new collection, the list
/// dbServers must be a list of DBserver ids to distribute across. 
/// If this list is empty, the complete current list of DBservers is
/// fetched from ClusterInfo. If shuffle is true, a few random shuffles
/// are performed before the list is taken. Thus modifies the list.
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::vector<std::string>> distributeShards(
    uint64_t numberOfShards,
    uint64_t replicationFactor,
    std::vector<std::string>& dbServers);

}  // namespace arangodb

#endif
