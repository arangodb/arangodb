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

#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

namespace arangodb {

namespace transaction {

class Context;
}

/// @brief Specialized transaction class for operations on a single collection
///
/// This class provides a streamlined interface for transactions that operate
/// on exactly one collection. It extends the general transaction::Methods
/// class with optimizations and convenience methods specific to single-collection
/// operations, reducing overhead and simplifying the API for common use cases.
///
/// The SingleCollectionTransaction provides:
/// - Optimized performance for single-collection operations
/// - Simplified API compared to multi-collection transactions
/// - Automatic collection resolution and caching
/// - Support for both read and write operations
/// - Runtime collection addition capabilities
///
/// @note This class is final and cannot be subclassed
/// @note Provides better performance than multi-collection transactions for single collection use
/// @note Automatically manages collection lifecycle and caching
/// @note Supports both collection names and data sources for initialization
class SingleCollectionTransaction final : public transaction::Methods {
 public:
  /// @brief Create transaction using a data source reference
  ///
  /// Creates a SingleCollectionTransaction that operates on the specified
  /// logical data source (collection) with the given access mode. This constructor
  /// is used when you already have a reference to the collection object.
  ///
  /// @param ctx Transaction context providing database and user information
  /// @param collection Reference to the logical data source (collection)
  /// @param accessType Access mode for the collection (READ, WRITE, EXCLUSIVE)
  /// @param options Additional transaction options (defaults to empty options)
  ///
  /// @note The collection reference must remain valid during transaction lifetime
  /// @note Access type determines what operations are allowed on the collection
  /// @note Transaction options control behavior like timeouts and isolation levels
  SingleCollectionTransaction(
      std::shared_ptr<transaction::Context> ctx,
      LogicalDataSource const& collection, AccessMode::Type accessType,
      transaction::Options const& options = transaction::Options());

  /// @brief Create transaction using a collection name
  ///
  /// Creates a SingleCollectionTransaction that operates on the collection
  /// with the specified name. The collection is resolved by name from the
  /// database context during transaction initialization.
  ///
  /// @param ctx Transaction context providing database and user information
  /// @param name Name of the collection to operate on
  /// @param accessType Access mode for the collection (READ, WRITE, EXCLUSIVE)
  /// @param options Additional transaction options (defaults to empty options)
  ///
  /// @note The collection name is resolved during transaction initialization
  /// @note Throws an exception if the collection doesn't exist
  /// @note Access type determines what operations are allowed on the collection
  SingleCollectionTransaction(
      std::shared_ptr<transaction::Context> ctx, std::string const& name,
      AccessMode::Type accessType,
      transaction::Options const& options = transaction::Options());

  /// @brief Default destructor
  ///
  /// Cleans up the transaction and releases any resources. The transaction
  /// is automatically committed or aborted based on its current state.
  ///
  /// @note Uses default destruction behavior from base class
  /// @note Proper cleanup is handled by the base transaction::Methods class
  ~SingleCollectionTransaction() = default;

  /// @brief Get the underlying document collection
  ///
  /// Returns a pointer to the LogicalCollection that this transaction operates on.
  /// This provides direct access to collection-specific operations and metadata.
  /// The collection is cached for performance.
  ///
  /// @return Pointer to the LogicalCollection
  ///
  /// @note The collection pointer is cached for performance
  /// @note The collection is guaranteed to be valid during transaction lifetime
  /// @note Used to access collection-specific operations and metadata
  LogicalCollection* documentCollection();

  /// @brief Get the underlying collection's ID
  ///
  /// Returns the unique identifier of the collection that this transaction
  /// operates on. This ID is immutable and uniquely identifies the collection
  /// within the database.
  ///
  /// @return DataSourceId of the collection
  ///
  /// @note The collection ID is immutable and unique within the database
  /// @note Does not throw exceptions
  /// @note Used for collection identification and caching
  DataSourceId cid() const noexcept { return _cid; }

#ifdef USE_ENTERPRISE
  using transaction::Methods::addCollectionAtRuntime;
#endif
  /// @brief Add a collection to the transaction at runtime
  ///
  /// Adds a collection to the transaction during execution, allowing dynamic
  /// collection access. For SingleCollectionTransaction, this can only add
  /// the same collection that the transaction was created for.
  ///
  /// @param name Name of the collection to add
  /// @param type Access type for the collection
  ///
  /// @return Future containing the DataSourceId of the added collection
  ///
  /// @note For SingleCollectionTransaction, can only add the same collection
  /// @note Returns a future that resolves to the collection ID
  /// @note Throws if trying to add a different collection
  /// @note Overrides the base class method with single-collection behavior
  futures::Future<DataSourceId> addCollectionAtRuntime(
      std::string_view name, AccessMode::Type type) override final;

  /// @brief Get the underlying collection's name
  ///
  /// Returns the name of the collection that this transaction operates on.
  /// This name is the human-readable identifier used to reference the
  /// collection in queries and operations.
  ///
  /// @return Name of the collection
  ///
  /// @note The collection name is resolved and cached
  /// @note Used for logging, debugging, and user-facing operations
  /// @note The name corresponds to the collection's current name
  std::string name();

 private:
  /// @brief Get the underlying transaction collection
  ///
  /// Returns the TransactionCollection object that represents this collection
  /// within the transaction context. This provides transaction-specific
  /// collection operations and state management.
  ///
  /// @return Pointer to the TransactionCollection
  ///
  /// @note Used internally for transaction-specific collection operations
  /// @note The TransactionCollection manages collection state within transactions
  /// @note Results are cached for performance
  TransactionCollection* resolveTrxCollection();

  /// @brief Collection ID
  ///
  /// Unique identifier of the collection that this transaction operates on.
  /// This ID is set during transaction initialization and remains constant
  /// throughout the transaction lifetime.
  ///
  /// @note Immutable during transaction lifetime
  /// @note Used for collection identification and caching
  /// @note Unique within the database
  DataSourceId _cid;

  /// @brief Transaction collection cache
  ///
  /// Cached pointer to the TransactionCollection object for performance.
  /// This avoids repeated lookups of the transaction collection during
  /// transaction operations.
  ///
  /// @note Cached for performance optimization
  /// @note Lazily initialized on first access
  /// @note Valid for the duration of the transaction
  TransactionCollection* _trxCollection;

  /// @brief Logical collection cache
  ///
  /// Cached pointer to the LogicalCollection object for performance.
  /// This avoids repeated lookups of the collection during transaction
  /// operations and provides direct access to collection methods.
  ///
  /// @note Cached for performance optimization
  /// @note Lazily initialized on first access
  /// @note Valid for the duration of the transaction
  LogicalCollection* _documentCollection;

  /// @brief Collection access type
  ///
  /// The access mode specified when the transaction was created (READ, WRITE,
  /// or EXCLUSIVE). This determines what operations are allowed on the
  /// collection during the transaction.
  ///
  /// @note Set during transaction initialization
  /// @note Determines allowed operations on the collection
  /// @note Immutable during transaction lifetime
  AccessMode::Type _accessType;
};

}  // namespace arangodb
