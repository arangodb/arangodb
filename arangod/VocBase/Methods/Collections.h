////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Transaction/Hints.h"
#include "Utils/OperationResult.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <functional>

namespace arangodb {
class ClusterFeature;
class LogicalCollection;
struct CollectionCreationInfo;
class CollectionNameResolver;
struct CreateCollectionBody;
struct ShardID;

namespace transaction {
class Methods;
}

struct CollectionDropOptions {
  // allow dropping system collection
  bool allowDropSystem = false;
  // flag if we want to keep access rights in-place
  bool keepUserRights = false;
  // allow dropping collections that are part of a graph
  bool allowDropGraphCollection = false;
};

namespace methods {

/// Common code for collection REST handler and v8-collections
struct Collections {
  struct Context {
    Context(Context const&) = delete;
    Context& operator=(Context const&) = delete;

    explicit Context(std::shared_ptr<LogicalCollection> coll);
    Context(std::shared_ptr<LogicalCollection> coll, transaction::Methods* trx);

    ~Context();

    futures::Future<transaction::Methods*> trx(AccessMode::Type const& type,
                                               bool embeddable);

    std::shared_ptr<LogicalCollection> coll() const;

   private:
    std::shared_ptr<LogicalCollection> _coll;
    transaction::Methods* _trx;
    bool const _responsibleForTrx;
  };

  /// @brief check if a name belongs to a collection
  static bool hasName(CollectionNameResolver const& resolver,
                      LogicalCollection const& collection,
                      std::string const& collectionName);

  /// @brief returns all collections, sorted by names
  static std::vector<std::shared_ptr<LogicalCollection>> sorted(
      TRI_vocbase_t& vocbase);

  static void enumerate(
      TRI_vocbase_t* vocbase,
      std::function<void(std::shared_ptr<LogicalCollection> const&)> const&);

  /// @brief lookup a collection in vocbase or clusterinfo.
  static Result lookup(              // find collection
      TRI_vocbase_t const& vocbase,  // vocbase to search
      std::string const& name,       // collection name
      std::shared_ptr<LogicalCollection>& ret);

  /// Create collection, ownership of collection in callback is
  /// transferred to callee
  [[nodiscard]] static arangodb::ResultT<
      std::vector<std::shared_ptr<LogicalCollection>>>
  create(                      // create collection
      TRI_vocbase_t& vocbase,  // collection vocbase
      OperationOptions const& options,
      std::vector<CreateCollectionBody> collections,  // Collections to create
      bool createWaitsForSyncReplication,             // replication wait flag
      bool enforceReplicationFactor,                  // replication factor flag
      bool isNewDatabase, bool allowEnterpriseCollectionsOnSingleServer = false,
      bool isRestore = false);  // whether this is being called
                                // during restore

  /// Create shard, can only be used on DBServers.
  /// Should only be called by Maintenance.
  [[nodiscard]] static arangodb::Result createShard(
      TRI_vocbase_t& vocbase,  // shard database
      OperationOptions const& options,
      ShardID const& name,                     // shard name
      TRI_col_type_e collectionType,           // shard type
      velocypack::Slice properties,            // shard properties
      std::shared_ptr<LogicalCollection>& ret  // ReturnValue: created Shard
  );

  static Result createSystem(TRI_vocbase_t& vocbase, OperationOptions const&,
                             std::string const& name, bool isNewDatabase,
                             std::shared_ptr<LogicalCollection>& ret);
  static void createSystemCollectionProperties(
      std::string const& collectionName, VPackBuilder& builder,
      TRI_vocbase_t const&);

  static void applySystemCollectionProperties(
      CreateCollectionBody& col, TRI_vocbase_t const& vocbase,
      DatabaseConfiguration const& config, bool isLegacyDatabase);

  static futures::Future<Result> properties(Context& ctxt,
                                            velocypack::Builder&);
  static futures::Future<Result> updateProperties(
      LogicalCollection& collection, velocypack::Slice props,
      OperationOptions const& options);

  static Result rename(LogicalCollection& collection,
                       std::string const& newName, bool doOverride);

  static arangodb::Result drop(           // drop collection
      arangodb::LogicalCollection& coll,  // collection to drop
      arangodb::CollectionDropOptions options);

  static futures::Future<Result> warmup(TRI_vocbase_t& vocbase,
                                        LogicalCollection const& coll);

  static futures::Future<OperationResult> revisionId(
      Context& ctxt, OperationOptions const& options);

  static futures::Future<Result> checksum(LogicalCollection& collection,
                                          bool withRevisions, bool withData,
                                          uint64_t& checksum,
                                          RevisionId& revId);

  /// @brief filters properties for collection creation
  static arangodb::velocypack::Builder filterInput(
      arangodb::velocypack::Slice slice, bool allowDC2DCAttributes);

 private:
  static void appendSmartEdgeCollections(
      CreateCollectionBody& collection,
      std::vector<CreateCollectionBody>& collectionList,
      std::function<DataSourceId()> const&);
};

#ifdef USE_ENTERPRISE
Result DropColEnterprise(LogicalCollection* collection, bool allowDropSystem);
#endif
}  // namespace methods
}  // namespace arangodb
