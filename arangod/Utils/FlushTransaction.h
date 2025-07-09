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

#include "Basics/Result.h"

namespace arangodb {

/// @brief Abstract base class for managing flush transactions
///
/// This class provides the foundation for implementing flush transactions that
/// handle the commitment of data to persistent storage. Flush transactions are
/// used to ensure data durability and consistency by coordinating the writing
/// of data from memory to disk in a transactional manner.
///
/// The FlushTransaction provides:
/// - A common interface for different types of flush operations
/// - Named transactions for better error reporting and diagnostics
/// - Virtual commit mechanism for polymorphic behavior
/// - Support for transaction rollback and error handling
///
/// @note This is an abstract base class that must be subclassed
/// @note Implementations must provide the commit() method
/// @note The class is non-copyable to prevent resource management issues
/// @note Used for ensuring data durability and consistency
class FlushTransaction {
 public:
  /// @brief Deleted copy constructor
  ///
  /// Copy construction is disabled to prevent accidental copying of
  /// flush transactions, which could lead to resource management issues
  /// and inconsistent state during flush operations.
  FlushTransaction(FlushTransaction const&) = delete;

  /// @brief Deleted copy assignment operator
  ///
  /// Copy assignment is disabled to prevent accidental copying of
  /// flush transactions, which could lead to resource management issues
  /// and inconsistent state during flush operations.
  FlushTransaction& operator=(FlushTransaction const&) = delete;

  /// @brief Create a named flush transaction
  ///
  /// Creates a flush transaction with a descriptive name that will be used
  /// for error logging and diagnostics. The name helps identify which type
  /// of flush operation failed when errors occur.
  ///
  /// @param name Descriptive name for the flush transaction
  ///
  /// @note The name is used for error logging and diagnostics
  /// @note Choose meaningful names that describe the flush operation
  /// @note The name is stored as a const reference for efficiency
  explicit FlushTransaction(std::string const& name) : _name(name) {}

  /// @brief Virtual destructor for proper cleanup
  ///
  /// Ensures that derived classes can properly clean up their resources
  /// when destroyed through a base class pointer. This is essential for
  /// polymorphic behavior and proper resource management.
  ///
  /// @note Allows safe destruction through base class pointers
  /// @note Derived classes should handle any necessary cleanup in their destructors
  virtual ~FlushTransaction() = default;

  /// @brief Get the name of this flush transaction
  ///
  /// Returns the descriptive name of this flush transaction. This name is
  /// used for error logging and diagnostics to help identify which type of
  /// flush operation encountered problems.
  ///
  /// @return The name of the flush transaction
  ///
  /// @note Used for error logging when flush commits fail
  /// @note Helps users understand what operation went wrong
  /// @note Returns a const reference for efficiency
  std::string const& name() { return _name; }

  /// @brief Commit the prepared flush transaction
  ///
  /// This pure virtual method must be implemented by derived classes to
  /// perform the actual commitment of the flush transaction. The implementation
  /// should handle writing data to persistent storage and ensuring durability.
  ///
  /// @return Result indicating success or failure of the commit operation
  ///
  /// @note Pure virtual method that must be implemented by derived classes
  /// @note Should handle all aspects of committing the flush transaction
  /// @note Must return a Result indicating success or failure
  /// @note Implementations should be atomic where possible
  /// @note May throw exceptions for unrecoverable errors
  virtual Result commit() = 0;

 private:
  /// @brief The name of the flush transaction
  ///
  /// Descriptive name used for error logging and diagnostics. When flush
  /// commits fail, this name helps identify which type of operation
  /// encountered problems, making debugging and error reporting more effective.
  ///
  /// @note Used for error logging and diagnostics
  /// @note Helps identify the type of flush operation that failed
  /// @note Stored as const to prevent accidental modification
  std::string const _name;
};

}  // namespace arangodb
