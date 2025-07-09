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

#include <mutex>
#include <unordered_map>

#include "Metrics/Fwd.h"
#include "Utils/Cursor.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {

namespace velocypack {
class Builder;
}

namespace aql {
class Query;
struct QueryResult;
}  // namespace aql

namespace transaction {
struct OperationOrigin;
}

/// @brief Repository for managing database cursors with lifecycle management
///
/// This class provides a centralized repository for managing database cursors
/// throughout their lifecycle in a specific database. It handles cursor creation,
/// storage, retrieval, and cleanup operations. The repository ensures thread-safe
/// access to cursors and provides automatic cleanup of expired cursors.
///
/// The cursor repository manages:
/// - Creation of cursors from query results or streaming queries
/// - Storage and retrieval of active cursors by ID with usage tracking
/// - Thread-safe access to cursor data structures via mutex protection
/// - Automatic cleanup of expired or unused cursors via garbage collection
/// - Integration with metrics system for monitoring cursor usage
///
/// @note This class is thread-safe and can be accessed from multiple threads
/// @note Cursors are automatically cleaned up when they expire or are removed
/// @note The repository uses mutex protection for thread-safe access
/// @note Integrates with the metrics system to track cursor count and memory usage
class CursorRepository {
  /// @brief Deleted copy constructor
  ///
  /// Copy construction is disabled to prevent accidental duplication of
  /// the cursor repository, which could lead to inconsistent state.
  CursorRepository(CursorRepository const&) = delete;

  /// @brief Deleted copy assignment operator
  ///
  /// Copy assignment is disabled to prevent accidental duplication of
  /// the cursor repository, which could lead to inconsistent state.
  CursorRepository& operator=(CursorRepository const&) = delete;

 public:
  /// @brief Construct a new cursor repository for a specific database
  ///
  /// Creates a new cursor repository associated with the specified database.
  /// The repository integrates with the metrics system to track cursor usage
  /// and memory consumption.
  ///
  /// @param vocbase Database this repository manages cursors for
  /// @param numberOfCursorsMetric Metric to track the number of active cursors
  /// @param memoryUsageMetric Metric to track memory usage of cursors
  ///
  /// @note The repository starts empty and cursors must be added via creation methods
  /// @note Metrics can be nullptr in test environments
  explicit CursorRepository(TRI_vocbase_t& vocbase,
                            metrics::Gauge<uint64_t>* numberOfCursorsMetric,
                            metrics::Gauge<uint64_t>* memoryUsageMetric);

  /// @brief Destructor that cleans up all cursors and metrics
  ///
  /// Destroys the cursor repository and automatically cleans up all
  /// remaining cursors. This ensures no cursor resources are leaked
  /// when the repository is destroyed.
  ///
  /// @note All cursors are automatically cleaned up during destruction
  /// @note Metrics are updated to reflect the cleanup
  ~CursorRepository();

  /// @brief Create a cursor from a completed query result
  ///
  /// Creates a cursor from an already-completed query result and stores it
  /// in the repository. The cursor takes ownership of the QueryResult and
  /// provides batched access to the result data.
  ///
  /// @param result Query result containing the data to cursor over
  /// @param batchSize Maximum number of documents to return per batch
  /// @param ttl Time-to-live for the cursor in seconds
  /// @param hasCount Whether the cursor should provide result count information
  /// @param isRetriable Whether the cursor can be retried on failure
  /// @return Pointer to the created cursor with usage flag set to true
  ///
  /// @note The cursor will be returned with the usage flag set to true
  /// @note The cursor must be returned later using release()
  /// @note The cursor takes ownership and retains the entire QueryResult object
  Cursor* createFromQueryResult(aql::QueryResult&& result, size_t batchSize,
                                double ttl, bool hasCount, bool isRetriable);

  /// @brief Create a streaming cursor from an active query
  ///
  /// Creates a streaming cursor that executes a query incrementally and
  /// returns results in batches. The cursor maintains the query object
  /// internally and executes it as needed to produce results.
  ///
  /// @param q Shared pointer to the query to execute
  /// @param batchSize Maximum number of documents to return per batch
  /// @param ttl Time-to-live for the cursor in seconds
  /// @param isRetriable Whether the cursor can be retried on failure
  /// @param operationOrigin Origin information for the operation
  /// @return Pointer to the created cursor with usage flag set to true
  ///
  /// @note The cursor will be returned with the usage flag set to true
  /// @note The cursor must be returned later using release()
  /// @note The cursor creates a query internally and retains it until deleted
  Cursor* createQueryStream(std::shared_ptr<arangodb::aql::Query> q,
                            size_t batchSize, double ttl, bool isRetriable,
                            transaction::OperationOrigin operationOrigin);

  /// @brief Remove a cursor from the repository by ID
  ///
  /// Removes and destroys the cursor with the specified ID from the repository.
  /// After removal, the cursor is no longer available and all its resources
  /// are cleaned up.
  ///
  /// @param id Unique identifier of the cursor to remove
  /// @return true if the cursor was found and removed, false otherwise
  ///
  /// @note If the cursor is not found, false is returned
  /// @note The cursor is immediately destroyed and cannot be used after removal
  /// @note Metrics are updated to reflect the removal
  bool remove(CursorId id);

  /// @brief Find and acquire a cursor by ID
  ///
  /// Searches for a cursor with the specified ID and returns it if found.
  /// The cursor's usage flag is set to true, indicating it is in use.
  /// The cursor remains in the repository and continues to be managed by it.
  ///
  /// @param id Unique identifier of the cursor to find
  /// @param found Reference to boolean that will be set to true if cursor is found
  /// @return Pointer to the found cursor, or nullptr if not found
  ///
  /// @note If the cursor is found, the usage flag is set to true
  /// @note The cursor must be returned later using release()
  /// @note The found parameter indicates whether the cursor was located
  Cursor* find(CursorId id, bool& found);

  /// @brief Release a cursor back to the repository
  ///
  /// Releases a cursor that was previously acquired via find() or create methods.
  /// This clears the usage flag and makes the cursor available for garbage
  /// collection if it has expired.
  ///
  /// @param cursor Pointer to the cursor to release
  ///
  /// @note The cursor must have been acquired via find() or create methods
  /// @note The cursor remains in the repository after release
  /// @note The cursor may be garbage collected if it has expired
  void release(Cursor*);

  /// @brief Check if the repository contains any used cursors
  ///
  /// Determines whether there are any cursors currently in use (with usage
  /// flag set to true). This is useful for shutdown procedures to ensure
  /// no cursors are abandoned.
  ///
  /// @return true if there are cursors in use, false otherwise
  ///
  /// @note This operation is thread-safe
  /// @note Used cursors are those acquired via find() or create methods
  bool containsUsedCursor() const;

  /// @brief Get the number of cursors in the repository
  ///
  /// Returns the current number of cursors stored in the repository,
  /// including both used and unused cursors.
  ///
  /// @return Number of cursors in the repository
  ///
  /// @note This operation is thread-safe
  /// @note The count may change immediately after this call due to concurrent operations
  size_t count() const;

  /// @brief Perform garbage collection on expired cursors
  ///
  /// Removes expired cursors from the repository to prevent memory leaks
  /// and resource exhaustion. Cursors are considered expired based on their
  /// TTL and last access time.
  ///
  /// @param force If true, removes all cursors regardless of expiration status
  /// @return true if any cursors were removed, false otherwise
  ///
  /// @note This operation is automatically performed periodically
  /// @note Force cleanup removes all cursors, useful during shutdown
  /// @note A maximum of maxCollectCount cursors are processed per call
  bool garbageCollect(bool force);

 private:
  /// @brief Store a cursor in the repository
  ///
  /// Internal method that stores a cursor in the repository and returns
  /// a pointer to it. The repository takes ownership of the cursor.
  ///
  /// @param cursor Unique pointer to the cursor to store
  /// @return Pointer to the stored cursor
  ///
  /// @note The repository takes ownership of the cursor
  /// @note The cursor is assigned a unique ID and stored in the internal map
  /// @note Metrics are updated to reflect the addition
  Cursor* addCursor(std::unique_ptr<Cursor> cursor);

  /// @brief Increase the number of cursors metric
  ///
  /// Internal method to update the cursor count metric when cursors are added.
  ///
  /// @param value Number to add to the cursor count metric
  ///
  /// @note This method is safe to call even if the metric is nullptr
  void increaseNumberOfCursorsMetric(size_t value) noexcept;

  /// @brief Decrease the number of cursors metric
  ///
  /// Internal method to update the cursor count metric when cursors are removed.
  ///
  /// @param value Number to subtract from the cursor count metric
  ///
  /// @note This method is safe to call even if the metric is nullptr
  void decreaseNumberOfCursorsMetric(size_t value) noexcept;

  /// @brief Increase the memory usage metric
  ///
  /// Internal method to update the memory usage metric when cursors are added.
  ///
  /// @param value Memory usage to add to the metric
  ///
  /// @note This method is safe to call even if the metric is nullptr
  /// @note Only used for non-streaming cursors
  void increaseMemoryUsageMetric(size_t value) noexcept;

  /// @brief Decrease the memory usage metric
  ///
  /// Internal method to update the memory usage metric when cursors are removed.
  ///
  /// @param value Memory usage to subtract from the metric
  ///
  /// @note This method is safe to call even if the metric is nullptr
  /// @note Only used for non-streaming cursors
  void decreaseMemoryUsageMetric(size_t value) noexcept;

  /// @brief Maximum number of cursors to garbage-collect in one go
  ///
  /// Limits the number of cursors processed during a single garbage collection
  /// cycle to prevent blocking other operations for too long.
  static constexpr size_t maxCollectCount = 1024;

  /// @brief Database this repository manages cursors for
  ///
  /// Reference to the database that this cursor repository is associated with.
  /// All cursors in this repository belong to this database.
  TRI_vocbase_t& _vocbase;

  /// @brief Mutex for thread-safe access to cursor data structures
  ///
  /// Protects access to all cursor data structures including the cursor map,
  /// ensuring thread-safe operations across all public methods.
  ///
  /// @note Marked mutable to allow const methods to acquire locks
  std::mutex mutable _lock;

  /// @brief Map storing all active cursors by ID
  ///
  /// Hash map that stores cursors by their unique ID along with additional
  /// metadata. Each cursor is paired with a string (likely for tracking purposes).
  ///
  /// @note Key is the cursor ID, value is a pair of cursor pointer and string
  /// @note Protected by _lock for thread-safe access
  std::unordered_map<CursorId, std::pair<Cursor*, std::string>> _cursors;

  /// @brief Flag indicating if soft shutdown is ongoing
  ///
  /// Used for the soft shutdown feature in coordinators to prevent new
  /// cursor operations during shutdown. In non-coordinator instances,
  /// this pointer is nullptr and not used.
  ///
  /// @note This is used for the soft shutdown feature in coordinators
  /// @note In all other instance types this pointer is nullptr and not used
  std::atomic<bool> const* _softShutdownOngoing;

  /// @brief Metric to track the number of active cursors
  ///
  /// Gauge metric that tracks the current number of cursors in the repository.
  /// This is used for monitoring and alerting on cursor usage.
  ///
  /// @note Can be nullptr in test environments
  /// @note Updated whenever cursors are added or removed
  metrics::Gauge<uint64_t>* _numberOfCursorsMetric;

  /// @brief Metric to track memory usage of cursors
  ///
  /// Gauge metric that tracks the memory usage of cursors in the repository.
  /// This is only used for non-streaming cursors as streaming cursors
  /// already count their memory against other AQL metrics.
  ///
  /// @note Can be nullptr in test environments
  /// @note Only used for non-streaming cursors
  /// @note Streaming cursors already count their memory against other AQL metrics
  metrics::Gauge<uint64_t>* _memoryUsageMetric;
};

}  // namespace arangodb
