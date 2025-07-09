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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/ServerState.h"
#include "Containers/FlatHashMap.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/voc-types.h"

#include <shared_mutex>

namespace arangodb {

class LogicalCollection;
class LogicalDataSource;
class LogicalView;

/// @brief Data-source ID/name resolver and cache for single-server and cluster environments
///
/// This class provides a comprehensive solution for resolving collection names to IDs
/// and vice versa, with support for both single-server and cluster deployments. It
/// handles the complexities of cluster-wide vs. local collection resolution and
/// provides efficient caching to minimize lookup overhead.
///
/// The CollectionNameResolver provides:
/// - Bidirectional name/ID resolution for collections, data sources, and views
/// - Cluster-aware resolution with automatic mode detection
/// - Efficient caching with thread-safe access patterns
/// - Support for both local and cluster-wide lookups
/// - Unified interface for different server roles (coordinator, DB server, single server)
///
/// @note This class is NOT thread-safe - external synchronization is required
/// @note Maintains internal caches for performance optimization
/// @note Handles both collections and views through unified data source interface
/// @note Automatically adapts behavior based on server role in cluster
class CollectionNameResolver {
 public:
  /// @brief Create a resolver for the specified database
  ///
  /// Creates a CollectionNameResolver instance bound to the specified database.
  /// The resolver will automatically detect the server role and adapt its
  /// behavior accordingly (single server, coordinator, or DB server).
  ///
  /// @param vocbase Database instance to resolve names/IDs for
  ///
  /// @note The resolver is bound to a specific database
  /// @note Server role detection is automatic based on current configuration
  /// @note Caches are initialized empty and populated on demand
  explicit CollectionNameResolver(TRI_vocbase_t& vocbase);

  /// @brief Default destructor
  ///
  /// Cleans up the resolver and releases any cached resources.
  /// The destructor is safe to call and will properly clean up all caches.
  ~CollectionNameResolver() = default;

  /// @brief Copy constructor for creating resolver copies
  ///
  /// Creates a copy of an existing resolver with the same database binding
  /// and server role. The caches are not copied - the new resolver starts
  /// with empty caches that are populated on demand.
  ///
  /// @param other Existing resolver to copy from
  ///
  /// @note Caches are not copied - new resolver starts with empty caches
  /// @note Database binding and server role are copied
  /// @note Both resolvers operate independently after copying
  CollectionNameResolver(CollectionNameResolver const& other);

  /// @brief Deleted copy assignment operator
  ///
  /// Copy assignment is disabled to prevent accidental copying which
  /// could lead to cache inconsistencies and resource management issues.
  CollectionNameResolver& operator=(CollectionNameResolver const& other) =
      delete;

  /// @brief Deleted move constructor
  ///
  /// Move construction is disabled to prevent resource management issues
  /// and maintain clear ownership semantics.
  CollectionNameResolver(CollectionNameResolver&& other) = delete;

  /// @brief Deleted move assignment operator
  ///
  /// Move assignment is disabled to prevent resource management issues
  /// and maintain clear ownership semantics.
  CollectionNameResolver& operator=(CollectionNameResolver&& other) = delete;

  /// @brief Look up a collection by ID
  ///
  /// Retrieves a collection object for the specified ID. On single servers
  /// and DB servers, this returns the local collection. On coordinators,
  /// this returns the cluster collection definition.
  ///
  /// @param id Collection ID to look up
  ///
  /// @return Shared pointer to the collection, or nullptr if not found
  ///
  /// @note Returns local collection on DB server/standalone
  /// @note Returns cluster collection on coordinator
  /// @note Result is cached for performance
  /// @note Thread-safe due to internal locking
  std::shared_ptr<LogicalCollection> getCollection(DataSourceId id) const;

  /// @brief Look up a collection by name or stringified ID
  ///
  /// Retrieves a collection object for the specified name or stringified ID.
  /// The method automatically detects whether the input is a name or ID string
  /// and performs the appropriate lookup.
  ///
  /// @param nameOrId Collection name or stringified ID to look up
  ///
  /// @return Shared pointer to the collection, or nullptr if not found
  ///
  /// @note Automatically detects names vs stringified IDs
  /// @note Returns local collection on DB server/standalone
  /// @note Returns cluster collection on coordinator
  /// @note UUIDs are supported for DB server/standalone
  std::shared_ptr<LogicalCollection> getCollection(
      std::string_view nameOrId) const;

  /// @brief Look up a local collection ID by name
  ///
  /// Retrieves the local collection ID for the specified name. Use this method
  /// when you know you're on a single server or DB server and need to look up
  /// a local collection name or shard name.
  ///
  /// @param name Local collection name to look up
  ///
  /// @return Local collection ID, or invalid ID if not found
  ///
  /// @note Use only on single server or DB server
  /// @note Performs local lookup only
  /// @note Does not perform cluster-wide resolution
  /// @note Suitable for shard name lookups on DB servers
  DataSourceId getCollectionIdLocal(std::string_view name) const;

  /// @brief Look up a cluster collection ID by name
  ///
  /// Retrieves the cluster-wide collection ID for the specified cluster
  /// collection name. Use this method in cluster mode on coordinators or
  /// DB servers when you need cluster-wide resolution.
  ///
  /// @param name Cluster collection name to look up
  ///
  /// @return Cluster-wide collection ID, or invalid ID if not found
  ///
  /// @note Use only in cluster mode
  /// @note Performs cluster-wide resolution
  /// @note Works on both coordinators and DB servers
  /// @note Returns cluster-wide collection ID
  DataSourceId getCollectionIdCluster(std::string_view name) const;

  /// @brief Get cluster collection structure by name
  ///
  /// Retrieves the cluster collection structure for the specified name.
  /// This method is used internally for cluster-wide collection resolution.
  ///
  /// @param name Cluster collection name to look up
  ///
  /// @return Shared pointer to cluster collection, or nullptr if not found
  ///
  /// @note Used internally for cluster collection resolution
  /// @note Returns cluster collection structure
  /// @note Works in cluster mode only
  std::shared_ptr<LogicalCollection> getCollectionStructCluster(
      std::string_view name) const;

  /// @brief Look up a collection ID by name (default method)
  ///
  /// Retrieves the collection ID for the specified name using the appropriate
  /// resolution method based on server role. This is the default method to use
  /// and will automatically do the right thing in different deployment scenarios.
  ///
  /// @param name Collection name to look up
  ///
  /// @return Collection ID, or invalid ID if not found
  ///
  /// @note This is the default method to use for ID resolution
  /// @note Uses local lookup on single server/DB server
  /// @note Uses cluster lookup on coordinator
  /// @note Automatically adapts to server role
  DataSourceId getCollectionId(std::string_view name) const;

  /// @brief Look up a collection name by ID
  ///
  /// Retrieves the collection name for the specified ID. In cluster mode,
  /// this method implements special logic: a DB server will automatically
  /// translate the local collection ID into a cluster-wide collection name.
  ///
  /// @param cid Collection ID to look up
  ///
  /// @return Collection name, or empty string if not found
  ///
  /// @note Implements cluster magic for DB servers
  /// @note DB servers translate local ID to cluster name
  /// @note Coordinators return cluster names directly
  /// @note Single servers return local names
  std::string getCollectionName(DataSourceId cid) const;

  /// @brief Look up a cluster collection name by cluster ID
  ///
  /// Retrieves the cluster-wide collection name for the specified cluster-wide
  /// collection ID. This method is used for explicit cluster-wide name resolution.
  ///
  /// @param cid Cluster-wide collection ID to look up
  ///
  /// @return Cluster-wide collection name, or empty string if not found
  ///
  /// @note Used for explicit cluster-wide name resolution
  /// @note Works with cluster-wide collection IDs
  /// @note Returns cluster-wide collection names
  std::string getCollectionNameCluster(DataSourceId cid) const;

  /// @brief Get collection name from name or ID string
  ///
  /// Returns the collection name if the input string is either a collection name
  /// or a string containing the numerical collection ID. In DB server mode,
  /// this returns the cluster-wide collection name.
  ///
  /// @param nameOrId Collection name or stringified ID
  ///
  /// @return Collection name, or empty string if not found
  ///
  /// @note Handles both names and stringified IDs
  /// @note DB servers return cluster-wide names
  /// @note Automatically detects input type
  std::string getCollectionName(std::string_view nameOrId) const;

  /// @brief Look up a data source by ID
  ///
  /// Retrieves a data source object (collection or view) for the specified ID.
  /// On single servers and DB servers, this returns the local data source.
  /// On coordinators, this returns the cluster data source.
  ///
  /// @param id Data source ID to look up
  ///
  /// @return Shared pointer to the data source, or nullptr if not found
  ///
  /// @note Returns local data source on DB server/standalone
  /// @note Returns cluster data source on coordinator
  /// @note Supports both collections and views
  /// @note Result is cached for performance
  std::shared_ptr<LogicalDataSource> getDataSource(DataSourceId id) const;

  /// @brief Look up a data source by name or stringified ID
  ///
  /// Retrieves a data source object (collection or view) for the specified name
  /// or stringified ID. The method automatically detects the input type.
  ///
  /// @param nameOrId Data source name or stringified ID to look up
  ///
  /// @return Shared pointer to the data source, or nullptr if not found
  ///
  /// @note Automatically detects names vs stringified IDs
  /// @note Returns local data source on DB server/standalone
  /// @note Returns cluster data source on coordinator
  /// @note Supports both collections and views
  /// @note UUIDs are supported for DB server/standalone
  std::shared_ptr<LogicalDataSource> getDataSource(
      std::string_view nameOrId) const;

  /// @brief Look up a view by ID
  ///
  /// Retrieves a view object for the specified ID. On single servers and
  /// DB servers, this returns the local view. On coordinators, this returns
  /// the cluster view definition.
  ///
  /// @param id View ID to look up
  ///
  /// @return Shared pointer to the view, or nullptr if not found
  ///
  /// @note Returns local view on DB server/standalone
  /// @note Returns cluster view on coordinator
  /// @note Result is cached for performance
  /// @note Thread-safe due to internal locking
  std::shared_ptr<LogicalView> getView(DataSourceId id) const;

  /// @brief Look up a view by name or stringified ID
  ///
  /// Retrieves a view object for the specified name or stringified ID.
  /// The method automatically detects whether the input is a name or ID string.
  ///
  /// @param nameOrId View name or stringified ID to look up
  ///
  /// @return Shared pointer to the view, or nullptr if not found
  ///
  /// @note Automatically detects names vs stringified IDs
  /// @note Returns local view on DB server/standalone
  /// @note Returns cluster view on coordinator
  /// @note UUIDs are supported for DB server/standalone
  std::shared_ptr<LogicalView> getView(std::string_view nameOrId) const;

  /// @brief Get the database instance this resolver uses
  ///
  /// Returns a reference to the database instance that this resolver is
  /// bound to. This database is used for all resolution operations.
  ///
  /// @return Reference to the database instance
  ///
  /// @note The database binding is immutable
  /// @note All resolution operations use this database
  /// @note Does not throw exceptions
  TRI_vocbase_t& vocbase() const noexcept { return _vocbase; }

  /// @brief Visit all collections that map to the specified ID
  ///
  /// Invokes the provided visitor function on all collections that map to
  /// the specified ID. This is useful for operations that need to work with
  /// all collections related to a particular ID.
  ///
  /// @param visitor Function to call for each matching collection
  /// @param id Data source ID to find collections for
  ///
  /// @return true if visitation was successful, false otherwise
  ///
  /// @note The visitor function should return true to continue, false to stop
  /// @note Visitation stops early if visitor returns false
  /// @note Used for operations that need to work with all related collections
  bool visitCollections(std::function<bool(LogicalCollection&)> const& visitor,
                        DataSourceId id) const;

 private:
  /// @brief Look up a name for the specified collection ID
  ///
  /// Internal method for looking up collection names by ID. This method
  /// handles the low-level name resolution logic.
  ///
  /// @param cid Collection ID to look up
  ///
  /// @return Collection name, or empty string if not found
  ///
  /// @note Internal method for name resolution
  /// @note Used by public name lookup methods
  /// @note Handles caching and lookup logic
  std::string lookupName(DataSourceId cid) const;

  /// @brief Database instance this resolver is bound to
  ///
  /// Reference to the database instance that this resolver uses for all
  /// resolution operations. This binding is immutable and set during construction.
  ///
  /// @note Immutable database binding
  /// @note All resolution operations use this database
  TRI_vocbase_t& _vocbase;

  /// @brief Server role in cluster
  ///
  /// Cached server role that determines the behavior of resolution methods.
  /// This is set during construction and influences whether local or cluster-wide
  /// resolution is used.
  ///
  /// @note Immutable and set during construction
  /// @note Influences resolution behavior
  /// @note Used to adapt to different deployment scenarios
  ServerState::RoleEnum const _serverRole;

  /// @brief Lock protecting internal caches
  ///
  /// Shared mutex that protects the internal caches from concurrent access.
  /// This enables thread-safe caching while allowing multiple concurrent readers.
  ///
  /// @note Enables thread-safe cache access
  /// @note Allows multiple concurrent readers
  /// @note Exclusive access for cache updates
  mutable std::shared_mutex _lock;

  /// @brief Cache mapping collection IDs to names
  ///
  /// Internal cache that maps collection IDs to their resolved names.
  /// This cache is populated on demand and protected by the shared mutex.
  ///
  /// @note Populated on demand for performance
  /// @note Protected by shared mutex
  /// @note Reduces lookup overhead
  mutable containers::FlatHashMap<DataSourceId, std::string> _resolvedIds;

  /// @brief Cache of data sources by ID
  ///
  /// Internal cache that maps data source IDs to their resolved objects.
  /// This cache is populated on demand and protected by the shared mutex.
  ///
  /// @note Populated on demand for performance
  /// @note Protected by shared mutex
  /// @note Stores both collections and views
  mutable containers::FlatHashMap<DataSourceId,
                                  std::shared_ptr<LogicalDataSource>>
      _dataSourceById;

  /// @brief Cache of data sources by name
  ///
  /// Internal cache that maps data source names to their resolved objects.
  /// This cache is populated on demand and protected by the shared mutex.
  ///
  /// @note Populated on demand for performance
  /// @note Protected by shared mutex
  /// @note Stores both collections and views
  mutable containers::FlatHashMap<std::string,
                                  std::shared_ptr<LogicalDataSource>>
      _dataSourceByName;
};

}  // namespace arangodb
