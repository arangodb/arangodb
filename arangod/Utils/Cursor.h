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

#include "Aql/ExecutionState.h"
#include "Basics/Result.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace arangodb {

namespace transaction {
class Context;
}

namespace velocypack {
template<typename T>
class Buffer;
class Builder;
class Slice;
}  // namespace velocypack

/// @brief Type alias for cursor identifiers
///
/// Defines the type used for unique cursor identification. Based on
/// TRI_voc_tick_t to ensure uniqueness across the system.
using CursorId = TRI_voc_tick_t;

/// @brief Abstract base class for database cursors with lifecycle management
///
/// This class provides the foundation for implementing database cursors that
/// manage query results and provide batch-based access to data. It handles
/// cursor lifecycle, batch management, expiration, and provides a framework
/// for different cursor implementations.
///
/// The Cursor provides:
/// - Unique cursor identification and lifecycle management
/// - Batch-based result delivery with configurable batch sizes
/// - Time-to-live (TTL) support with automatic expiration
/// - Thread-safe usage tracking and concurrency control
/// - Support for retriable operations and error handling
/// - Memory usage tracking and resource management
///
/// @note This is an abstract base class that must be subclassed
/// @note The class is non-copyable to prevent resource management issues
/// @note Thread-safe operations for concurrent cursor access
/// @note Supports both synchronous and asynchronous result dumping
class Cursor {
 public:
  /// @brief Deleted copy constructor
  ///
  /// Copy construction is disabled to prevent accidental copying of
  /// cursor objects, which could lead to resource management issues
  /// and inconsistent state.
  Cursor(Cursor const&) = delete;

  /// @brief Deleted copy assignment operator
  ///
  /// Copy assignment is disabled to prevent accidental copying of
  /// cursor objects, which could lead to resource management issues
  /// and inconsistent state.
  Cursor& operator=(Cursor const&) = delete;

  /// @brief Create a new cursor with specified configuration
  ///
  /// Creates a cursor with the given configuration parameters. The cursor
  /// will be ready for use and will automatically handle expiration based
  /// on the TTL value.
  ///
  /// @param id Unique identifier for the cursor
  /// @param batchSize Maximum number of results to return in each batch
  /// @param ttl Time-to-live in seconds for cursor expiration
  /// @param hasCount Whether the cursor provides total result count
  /// @param isRetriable Whether operations on this cursor can be retried
  ///
  /// @note The cursor ID must be unique across the system
  /// @note Batch size controls memory usage and network efficiency
  /// @note TTL of 0 means the cursor never expires
  /// @note Count availability depends on the underlying query type
  Cursor(CursorId id, size_t batchSize, double ttl, bool hasCount,
         bool isRetriable);

  /// @brief Virtual destructor for proper cleanup
  ///
  /// Ensures that derived classes can properly clean up their resources
  /// when destroyed through a base class pointer. This is essential for
  /// polymorphic behavior and proper resource management.
  ///
  /// @note Allows safe destruction through base class pointers
  /// @note Derived classes should handle any necessary cleanup in their destructors
  virtual ~Cursor();

 public:
  /// @brief Get the unique identifier of this cursor
  ///
  /// Returns the unique identifier assigned to this cursor when it was created.
  /// This ID is used for cursor lookup and management operations.
  ///
  /// @return Unique cursor identifier
  ///
  /// @note The ID is immutable and unique across the system
  /// @note Used for cursor lookup and reference operations
  /// @note Does not throw exceptions
  CursorId id() const noexcept { return _id; }

  /// @brief Get the batch size for this cursor
  ///
  /// Returns the maximum number of results that will be returned in each
  /// batch when dumping cursor results. This affects memory usage and
  /// network efficiency.
  ///
  /// @return Maximum batch size
  ///
  /// @note The batch size is set during cursor creation and is immutable
  /// @note Controls memory usage and network efficiency
  /// @note Does not throw exceptions
  size_t batchSize() const noexcept { return _batchSize; }

  /// @brief Check if this cursor provides result count information
  ///
  /// Returns whether this cursor can provide the total number of results.
  /// Not all cursor types can provide this information efficiently.
  ///
  /// @return true if count information is available, false otherwise
  ///
  /// @note Count availability depends on the underlying query type
  /// @note Some queries cannot provide count information without full execution
  /// @note Does not throw exceptions
  bool hasCount() const noexcept { return _hasCount; }

  /// @brief Check if operations on this cursor are retriable
  ///
  /// Returns whether operations on this cursor can be retried in case of
  /// transient failures. This depends on the cursor type and underlying
  /// query characteristics.
  ///
  /// @return true if operations are retriable, false otherwise
  ///
  /// @note Retriability depends on cursor type and query characteristics
  /// @note Non-retriable cursors must be handled more carefully
  /// @note Does not throw exceptions
  bool isRetriable() const noexcept { return _isRetriable; }

  /// @brief Get the time-to-live for this cursor
  ///
  /// Returns the time-to-live value in seconds that was set when the cursor
  /// was created. A value of 0 means the cursor never expires.
  ///
  /// @return Time-to-live in seconds
  ///
  /// @note TTL is set during cursor creation and is immutable
  /// @note A value of 0 means the cursor never expires
  /// @note Does not throw exceptions
  double ttl() const noexcept { return _ttl; }

  /// @brief Get the absolute expiration time for this cursor
  ///
  /// Returns the absolute timestamp when this cursor will expire.
  /// The cursor should be cleaned up after this time.
  ///
  /// @return Absolute expiration timestamp
  ///
  /// @note Returns the absolute time when the cursor expires
  /// @note Used for cursor cleanup and garbage collection
  /// @note Thread-safe due to atomic storage
  double expires() const noexcept;

  /// @brief Check if the cursor is currently being used
  ///
  /// Returns whether the cursor is currently being used by an operation.
  /// This is used for concurrency control and resource management.
  ///
  /// @return true if the cursor is in use, false otherwise
  ///
  /// @note Thread-safe due to atomic storage
  /// @note Used for concurrency control
  /// @note Does not throw exceptions
  bool isUsed() const noexcept;

  /// @brief Check if the cursor has been marked for deletion
  ///
  /// Returns whether the cursor has been marked for deletion and should
  /// be cleaned up. Deleted cursors should not be used for operations.
  ///
  /// @return true if the cursor is deleted, false otherwise
  ///
  /// @note Deleted cursors should not be used for operations
  /// @note Used for cursor lifecycle management
  /// @note Does not throw exceptions
  bool isDeleted() const noexcept;

  /// @brief Mark the cursor for deletion
  ///
  /// Marks the cursor for deletion, indicating that it should be cleaned up
  /// and should not be used for further operations.
  ///
  /// @note This is a one-way operation - cursors cannot be undeleted
  /// @note Deleted cursors should not be used for operations
  /// @note Does not throw exceptions
  void setDeleted() noexcept;

  /// @brief Check if the given ID matches the current batch ID
  ///
  /// Returns whether the provided ID matches the current batch ID of the cursor.
  /// This is used for batch consistency checking.
  ///
  /// @param id Batch ID to check
  ///
  /// @return true if ID matches current batch, false otherwise
  ///
  /// @note Used for batch consistency checking
  /// @note Helps ensure clients are working with the correct batch
  /// @note Does not throw exceptions
  bool isCurrentBatchId(uint64_t id) const noexcept;

  /// @brief Check if the given ID matches the next batch ID
  ///
  /// Returns whether the provided ID matches the next expected batch ID.
  /// This is used for batch sequence validation.
  ///
  /// @param id Batch ID to check
  ///
  /// @return true if ID matches next batch, false otherwise
  ///
  /// @note Used for batch sequence validation
  /// @note Helps ensure correct batch ordering
  /// @note May throw exceptions in some implementations
  bool isNextBatchId(uint64_t id) const;

  /// @brief Store the last query batch result
  ///
  /// Stores the result of the last query batch for potential retrieval.
  /// This enables batch caching and retrieval scenarios.
  ///
  /// @param buffer Buffer containing the serialized batch result
  ///
  /// @note The buffer is stored as a shared pointer for efficient sharing
  /// @note Used for batch caching and retrieval
  /// @note Does not throw exceptions
  void setLastQueryBatchObject(
      std::shared_ptr<velocypack::Buffer<uint8_t>> buffer) noexcept;

  /// @brief Get the last stored batch result
  ///
  /// Returns the buffer containing the last stored batch result.
  /// May return nullptr if no batch has been stored.
  ///
  /// @return Shared pointer to the last batch buffer, or nullptr
  ///
  /// @note May return nullptr if no batch is stored
  /// @note Used for batch retrieval and caching
  /// @note Thread-safe due to shared pointer semantics
  std::shared_ptr<velocypack::Buffer<uint8_t>> getLastBatch() const;

  /// @brief Get the ID of the stored batch
  ///
  /// Returns the ID of the currently stored batch result.
  /// This ID corresponds to the batch stored by setLastQueryBatchObject.
  ///
  /// @return ID of the stored batch
  ///
  /// @note Corresponds to the batch stored by setLastQueryBatchObject
  /// @note Used for batch identification and validation
  /// @note May throw exceptions in some implementations
  uint64_t storedBatchId() const;

  /// @brief Handle next batch ID value in query results
  ///
  /// Processes the next batch ID value and updates the query results
  /// accordingly. This is used for batch sequence management.
  ///
  /// @param builder VelocyPack builder to write batch information to
  /// @param hasMore Whether there are more results available
  ///
  /// @note Updates internal batch tracking state
  /// @note Writes batch information to the provided builder
  /// @note Used for batch sequence management
  void handleNextBatchIdValue(velocypack::Builder& builder, bool hasMore);

  /// @brief Mark the cursor as being used
  ///
  /// Atomically marks the cursor as being used, preventing concurrent
  /// access. This should be called before performing operations on the cursor.
  ///
  /// @note This is atomic and thread-safe
  /// @note Should be paired with a corresponding release() call
  /// @note Used for concurrency control
  /// @note Does not throw exceptions
  void use() noexcept;

  /// @brief Release the cursor from use
  ///
  /// Atomically marks the cursor as not being used, allowing other
  /// operations to proceed. This should be called after completing
  /// operations on the cursor.
  ///
  /// @note This is atomic and thread-safe
  /// @note Should be paired with a corresponding use() call
  /// @note Used for concurrency control
  /// @note Does not throw exceptions
  void release() noexcept;

  /// @brief Kill the cursor and clean up resources
  ///
  /// Virtual method for killing the cursor and cleaning up any associated
  /// resources. Default implementation does nothing - derived classes should
  /// override this to provide specific cleanup behavior.
  ///
  /// @note Default implementation does nothing
  /// @note Derived classes should override for specific cleanup
  /// @note Used for forceful cursor termination
  virtual void kill() {}

  /// @brief Debug method to kill a query at a specific position
  ///
  /// Debug utility method to kill a query at a specific position during
  /// execution. This is used for testing and debugging purposes and
  /// internally verifies that the query is visible through other APIs.
  ///
  /// @note This is a debug utility method
  /// @note Internally asserts that the query is visible through other APIs
  /// @note Used for testing and debugging purposes
  /// @note Default implementation does nothing
  virtual void debugKillQuery() {}

  /// @brief Get the memory usage of this cursor
  ///
  /// Pure virtual method that must be implemented by derived classes to
  /// return the approximate memory usage of the cursor in bytes.
  ///
  /// @return Approximate memory usage in bytes
  ///
  /// @note Pure virtual method that must be implemented by derived classes
  /// @note Should return approximate memory usage in bytes
  /// @note Used for memory monitoring and management
  /// @note Does not throw exceptions
  virtual uint64_t memoryUsage() const noexcept = 0;

  /// @brief Get the total number of results
  ///
  /// Pure virtual method that must be implemented by derived classes to
  /// return the total number of results available through this cursor.
  ///
  /// @return Total number of results
  ///
  /// @note Pure virtual method that must be implemented by derived classes
  /// @note Only available if hasCount() returns true
  /// @note May require full query execution to determine
  virtual size_t count() const = 0;

  /// @brief Get the transaction context for this cursor
  ///
  /// Pure virtual method that must be implemented by derived classes to
  /// return the transaction context associated with this cursor.
  ///
  /// @return Shared pointer to the transaction context
  ///
  /// @note Pure virtual method that must be implemented by derived classes
  /// @note The transaction context provides database and user information
  /// @note Used for permission checking and transaction management
  virtual std::shared_ptr<transaction::Context> context() const = 0;

  /// @brief Dump cursor results asynchronously
  ///
  /// Pure virtual method for dumping cursor results asynchronously. This
  /// method may return WAITING state if the operation needs to be suspended
  /// and continued later.
  ///
  /// @param result VelocyPack builder to write results to
  ///
  /// @return Pair of execution state and result status
  ///
  /// @note Pure virtual method that must be implemented by derived classes
  /// @note May return WAITING state for asynchronous execution
  /// @note On WAITING, the operation must be continued later
  /// @note On DONE, the result contains the final status
  virtual std::pair<aql::ExecutionState, Result> dump(
      velocypack::Builder& result) = 0;

  /// @brief Dump cursor results synchronously
  ///
  /// Pure virtual method for dumping cursor results synchronously. This
  /// method is guaranteed to return the result in the current thread without
  /// suspending execution.
  ///
  /// @param result VelocyPack builder to write results to
  ///
  /// @return Result status of the operation
  ///
  /// @note Pure virtual method that must be implemented by derived classes
  /// @note Guaranteed to return results in the current thread
  /// @note Does not suspend execution like the async version
  /// @note Used when synchronous execution is required
  virtual Result dumpSync(velocypack::Builder& result) = 0;

  /// @brief Set wakeup handler for streaming cursors
  ///
  /// Virtual method for setting a wakeup handler on streaming cursors.
  /// The handler is called when more data becomes available.
  ///
  /// @param cb Callback function to call when data becomes available
  ///
  /// @note Default implementation does nothing
  /// @note Used for streaming cursors to handle data availability
  /// @note The callback should return true to continue, false to stop
  virtual void setWakeupHandler(std::function<bool()> const& cb) {}

  /// @brief Reset the wakeup handler for streaming cursors
  ///
  /// Virtual method for resetting the wakeup handler on streaming cursors.
  /// This removes any previously set wakeup handler.
  ///
  /// @note Default implementation does nothing
  /// @note Used for streaming cursors to remove wakeup handlers
  /// @note Should be called when the handler is no longer needed
  virtual void resetWakeupHandler() {}

  /// @brief Check if dirty reads are allowed for this cursor
  ///
  /// Virtual method that returns whether dirty reads are allowed for this
  /// cursor. Dirty reads may see uncommitted data from other transactions.
  ///
  /// @return true if dirty reads are allowed, false otherwise
  ///
  /// @note Default implementation returns false (dirty reads not allowed)
  /// @note Dirty reads may see uncommitted data from other transactions
  /// @note Used for read consistency control
  /// @note Does not throw exceptions
  virtual bool allowDirtyReads() const noexcept { return false; }

 protected:
  /// @brief Unique cursor identifier
  ///
  /// Immutable identifier assigned to this cursor when it was created.
  /// Used for cursor lookup and management operations.
  ///
  /// @note Immutable and unique across the system
  /// @note Used for cursor lookup and reference operations
  CursorId const _id;

  /// @brief Maximum batch size for result delivery
  ///
  /// Immutable value that controls the maximum number of results returned
  /// in each batch. This affects memory usage and network efficiency.
  ///
  /// @note Immutable and set during cursor creation
  /// @note Controls memory usage and network efficiency
  size_t const _batchSize;

  /// @brief Current batch identifier
  ///
  /// Tracks the current batch ID for batch consistency checking and
  /// sequence validation.
  ///
  /// @note Used for batch consistency checking
  /// @note Updated as batches are processed
  size_t _currentBatchId;

  /// @brief Last available batch identifier
  ///
  /// Tracks the last available batch ID for batch sequence management
  /// and availability checking.
  ///
  /// @note Used for batch sequence management
  /// @note Updated as new batches become available
  size_t _lastAvailableBatchId;

  /// @brief Time-to-live value in seconds
  ///
  /// Immutable value that specifies how long the cursor should remain
  /// valid. A value of 0 means the cursor never expires.
  ///
  /// @note Immutable and set during cursor creation
  /// @note A value of 0 means the cursor never expires
  double const _ttl;

  /// @brief Absolute expiration timestamp
  ///
  /// Atomic timestamp indicating when the cursor will expire.
  /// Used for cursor cleanup and garbage collection.
  ///
  /// @note Atomic for thread-safe access
  /// @note Used for cursor cleanup and garbage collection
  std::atomic<double> _expires;

  /// @brief Whether the cursor provides count information
  ///
  /// Immutable flag indicating whether this cursor can provide the total
  /// number of results efficiently.
  ///
  /// @note Immutable and set during cursor creation
  /// @note Depends on the underlying query type
  bool const _hasCount;

  /// @brief Whether operations on this cursor are retriable
  ///
  /// Immutable flag indicating whether operations on this cursor can be
  /// retried in case of transient failures.
  ///
  /// @note Immutable and set during cursor creation
  /// @note Depends on cursor type and query characteristics
  bool const _isRetriable;

  /// @brief Whether the cursor has been marked for deletion
  ///
  /// Flag indicating whether the cursor has been marked for deletion
  /// and should be cleaned up.
  ///
  /// @note Used for cursor lifecycle management
  /// @note Deleted cursors should not be used for operations
  bool _isDeleted;

  /// @brief Whether the cursor is currently being used
  ///
  /// Atomic flag indicating whether the cursor is currently being used
  /// by an operation. Used for concurrency control.
  ///
  /// @note Atomic for thread-safe access
  /// @note Used for concurrency control
  std::atomic<bool> _isUsed;

  /// @brief Current batch result storage
  ///
  /// Stores the current batch result as a pair of batch ID and buffer.
  /// Used for batch caching and retrieval scenarios.
  ///
  /// @note Pair of batch ID and buffer
  /// @note Used for batch caching and retrieval
  std::pair<uint64_t, std::shared_ptr<velocypack::Buffer<uint8_t>>>
      _currentBatchResult;
};
}  // namespace arangodb
