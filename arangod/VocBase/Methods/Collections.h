////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Utils/OperationResult.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "VocBase/Properties/CollectionCreateOptions.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <functional>

namespace arangodb {
struct Database;
class ClusterFeature;
class LogicalCollection;
struct CollectionCreationInfo;
class CollectionNameResolver;
struct CollectionDescriptor;
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
      Database& vocbase);

  static void enumerate(
      Database* vocbase,
      std::function<bool(std::shared_ptr<LogicalCollection> const&)> const&);

  static std::vector<std::shared_ptr<LogicalCollection>> getNotDeleted(
      const Database& vocbase);

  /// @brief lookup a collection in vocbase or clusterinfo.
  static Result lookup(         // find collection
      Database const& vocbase,  // vocbase to search
      std::string const& name,  // collection name
      std::shared_ptr<LogicalCollection>& ret);

  /// Create collection, ownership of collection in callback is
  /// transferred to callee
  [[nodiscard]] static arangodb::ResultT<
      std::vector<std::shared_ptr<LogicalCollection>>>
  create(                 // create collection
      Database& vocbase,  // collection vocbase
      OperationOptions const& options,
      std::vector<CollectionDescriptor> collections,  // Collections to create
      CollectionCreateOptions const& createOptions = {});

  /// Create shard, can only be used on DBServers.
  /// Should only be called by Maintenance.
  [[nodiscard]] static arangodb::Result createShard(
      Database& vocbase,  // shard database
      OperationOptions const& options,
      ShardID const& name,                     // shard name
      TRI_col_type_e collectionType,           // shard type
      velocypack::Slice properties,            // shard properties
      std::shared_ptr<LogicalCollection>& ret  // ReturnValue: created Shard
  );

  static Result createSystem(Database& vocbase, OperationOptions const&,
                             std::string const& name, bool isNewDatabase,
                             std::shared_ptr<LogicalCollection>& ret);
  static void createSystemCollectionProperties(
      std::string const& collectionName, VPackBuilder& builder,
      Database const&);

  static void applySystemCollectionProperties(
      CollectionDescriptor& col, Database const& vocbase,
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

  static futures::Future<Result> warmup(Database& vocbase,
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
      CollectionDescriptor& collection,
      std::vector<CollectionDescriptor>& collectionList,
      std::function<DataSourceId()> const&);
};

#ifdef USE_ENTERPRISE
Result DropColEnterprise(LogicalCollection* collection, bool allowDropSystem);
#endif
}  // namespace methods
}  // namespace arangodb
