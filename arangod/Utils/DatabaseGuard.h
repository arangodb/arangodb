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

#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
class DatabaseFeature;

/// @brief Interface for database guard implementations
///
/// This interface defines the contract for database guard implementations
/// that provide safe access to database objects. Guards implementing this
/// interface ensure that databases remain accessible during the guard's
/// lifetime and handle proper resource management.
///
/// @note This interface uses virtual methods for polymorphic access
/// @note Implementations must ensure the database remains valid during guard lifetime
/// @note The database() method is marked as noexcept for performance
struct IDatabaseGuard {
  /// @brief Virtual destructor for proper cleanup
  ///
  /// Ensures that derived guard classes can properly clean up their resources
  /// when destroyed through a base class pointer.
  virtual ~IDatabaseGuard() = default;

  /// @brief Get the database managed by this guard
  ///
  /// Returns a reference to the database managed by this guard. The database
  /// is guaranteed to be valid as long as the guard exists.
  ///
  /// @return Reference to the TRI_vocbase_t database
  ///
  /// @note The database reference is valid for the lifetime of the guard
  /// @note This method is noexcept for performance reasons
  /// @note Implementations must ensure the database remains accessible
  [[nodiscard]] virtual TRI_vocbase_t& database() const noexcept = 0;
};

/// @brief Custom deleter for TRI_vocbase_t objects
///
/// This deleter provides proper cleanup for TRI_vocbase_t objects when used
/// with smart pointers. It handles the specific release semantics required
/// for database objects in the ArangoDB system.
///
/// @note This deleter integrates with the database feature's reference counting
/// @note Uses noexcept to ensure safe use in smart pointer destructors
/// @note Handles null pointers safely
struct VocbaseReleaser {
  /// @brief Release a database object
  ///
  /// Performs the appropriate cleanup for a TRI_vocbase_t object, typically
  /// involving decrementing reference counts or notifying the database feature
  /// that the database is no longer in use.
  ///
  /// @param vocbase Database object to release (may be nullptr)
  ///
  /// @note Safe to call with nullptr
  /// @note Integrates with the database feature's lifecycle management
  /// @note Does not throw exceptions to ensure safe destruction
  void operator()(TRI_vocbase_t* vocbase) const noexcept;
};

/// @brief Smart pointer type for database objects with custom deleter
///
/// This type alias provides a convenient way to manage TRI_vocbase_t objects
/// with automatic resource management. The VocbaseReleaser ensures proper
/// cleanup when the pointer goes out of scope.
///
/// @note Uses VocbaseReleaser for proper database cleanup
/// @note Provides RAII semantics for database objects
/// @note Safe to use across function boundaries
using VocbasePtr = std::unique_ptr<TRI_vocbase_t, VocbaseReleaser>;

/// @brief RAII scope guard for database access with automatic lifecycle management
///
/// This class provides a safe way to access and manage TRI_vocbase_t (database)
/// objects with automatic resource management using RAII principles. It ensures
/// that databases are not dropped while still being used and handles proper
/// reference counting and cleanup.
///
/// The DatabaseGuard provides:
/// - Protection against database dropping during usage
/// - Automatic reference counting and lifecycle management
/// - Multiple construction options for different use cases
/// - Integration with the database feature system
///
/// @note This class implements the IDatabaseGuard interface
/// @note The guard ensures databases remain accessible during its lifetime
/// @note Proper cleanup is handled automatically when the guard is destroyed
/// @note Thread-safe access to database objects
class DatabaseGuard final : public IDatabaseGuard {
 public:
  /// @brief Create guard for an existing database reference
  ///
  /// Creates a DatabaseGuard that protects an existing database object.
  /// This is used when you already have a database reference and want to
  /// ensure it remains valid during a specific scope.
  ///
  /// @param vocbase Existing database reference to protect
  ///
  /// @note The database must already be properly acquired before creating the guard
  /// @note The guard will prevent the database from being dropped
  /// @note Use this when you have a database reference from elsewhere
  explicit DatabaseGuard(TRI_vocbase_t& vocbase);

  /// @brief Create guard from an existing VocbasePtr
  ///
  /// Creates a DatabaseGuard by taking ownership of an existing VocbasePtr.
  /// This transfers ownership of the database object to the guard.
  ///
  /// @param vocbase Smart pointer to the database to manage
  ///
  /// @note Takes ownership of the VocbasePtr
  /// @note The guard becomes responsible for the database's lifecycle
  /// @note Use this when transferring ownership from a VocbasePtr
  explicit DatabaseGuard(VocbasePtr vocbase);

  /// @brief Create guard by acquiring a database using its ID
  ///
  /// Creates a DatabaseGuard and attempts to acquire the specified database
  /// from the database feature using the database ID. The guard will manage
  /// the database's lifecycle once acquired.
  ///
  /// @param feature Database feature from which to acquire the database
  /// @param id ID of the database to acquire
  ///
  /// @note Throws an exception if the database doesn't exist or cannot be accessed
  /// @note The database is automatically managed by the guard
  /// @note The guard handles proper reference counting
  DatabaseGuard(DatabaseFeature& feature, TRI_voc_tick_t id);

  /// @brief Create guard by acquiring a database using its name
  ///
  /// Creates a DatabaseGuard and attempts to acquire the specified database
  /// from the database feature using the database name. The guard will manage
  /// the database's lifecycle once acquired.
  ///
  /// @param feature Database feature from which to acquire the database
  /// @param name Name of the database to acquire
  ///
  /// @note Throws an exception if the database doesn't exist or cannot be accessed
  /// @note The database is automatically managed by the guard
  /// @note String view parameter allows efficient string handling
  DatabaseGuard(DatabaseFeature& feature, std::string_view name);

  /// @brief Get the database managed by this guard
  ///
  /// Returns a reference to the database managed by this guard. The database
  /// is guaranteed to be valid as long as the guard exists. This method
  /// implements the IDatabaseGuard interface.
  ///
  /// @return Reference to the TRI_vocbase_t database
  ///
  /// @note The database reference is valid for the lifetime of the guard
  /// @note This method is noexcept for performance reasons
  /// @note Implements the IDatabaseGuard interface contract
  TRI_vocbase_t& database() const noexcept final { return *_vocbase; }

  /// @brief Arrow operator for const access to database methods
  ///
  /// Provides direct const access to database methods through the arrow operator.
  /// The database is guaranteed to be valid since the guard manages its lifecycle.
  ///
  /// @return Const pointer to the TRI_vocbase_t database
  ///
  /// @note The database is guaranteed to be valid
  /// @note Enables direct const method calls on the database object
  /// @note Does not throw exceptions
  TRI_vocbase_t const* operator->() const noexcept { return _vocbase.get(); }

  /// @brief Arrow operator for mutable access to database methods
  ///
  /// Provides direct mutable access to database methods through the arrow operator.
  /// The database is guaranteed to be valid since the guard manages its lifecycle.
  ///
  /// @return Mutable pointer to the TRI_vocbase_t database
  ///
  /// @note The database is guaranteed to be valid
  /// @note Enables direct mutable method calls on the database object
  /// @note Does not throw exceptions
  TRI_vocbase_t* operator->() noexcept { return _vocbase.get(); }

 private:
  /// @brief Smart pointer to the managed database
  ///
  /// Holds the database object using a VocbasePtr, which provides automatic
  /// resource management through the VocbaseReleaser. The database is properly
  /// cleaned up when the guard is destroyed.
  ///
  /// @note Uses VocbaseReleaser for proper cleanup
  /// @note Automatically handles database lifecycle management
  /// @note Ensures the database remains valid during guard lifetime
  VocbasePtr _vocbase;
};

}  // namespace arangodb
