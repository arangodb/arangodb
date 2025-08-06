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

#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "VocBase/vocbase.h"

#include <memory>

namespace arangodb {
class LogicalCollection;

/// @brief RAII guard for safe collection access and automatic resource management
///
/// This class provides a safe way to access and manage LogicalCollection objects
/// with automatic resource management using RAII principles. It ensures that
/// collections are properly acquired from the database, used safely during the
/// guard's lifetime, and automatically released when the guard goes out of scope.
///
/// The CollectionGuard provides:
/// - Safe acquisition of collection references with permission checking
/// - Automatic resource cleanup when the guard goes out of scope
/// - Protection against resource leaks and use-after-free errors
/// - Exception-safe collection management
///
/// @note This class is move-only to prevent accidental copying and resource issues
/// @note The guard automatically throws if collection acquisition fails
/// @note Collections are automatically released when the guard is destroyed
/// @note Permission checking is performed during collection acquisition
class CollectionGuard {
 public:
  /// @brief Deleted copy constructor
  ///
  /// Copy construction is disabled to prevent accidental copying of
  /// collection guards, which could lead to resource management issues
  /// and double-release scenarios.
  CollectionGuard(CollectionGuard const&) = delete;

  /// @brief Deleted copy assignment operator
  ///
  /// Copy assignment is disabled to prevent accidental copying of
  /// collection guards, which could lead to resource management issues
  /// and double-release scenarios.
  CollectionGuard& operator=(CollectionGuard const&) = delete;

  /// @brief Move constructor for transferring collection ownership
  ///
  /// Transfers ownership of the collection from another guard. The source
  /// guard becomes invalid after the move and will not release any collection.
  /// This enables efficient passing of guards between functions.
  ///
  /// @param other Guard to move from
  ///
  /// @note The source guard becomes invalid and will not release the collection
  /// @note This is the preferred way to pass guards between functions
  /// @note No exceptions are thrown during the move operation
  CollectionGuard(CollectionGuard&& other)
      : _vocbase(other._vocbase), _collection(std::move(other._collection)) {
    other._collection.reset();
    other._vocbase = nullptr;
  }

  /// @brief Create the guard by acquiring a collection using its ID
  ///
  /// Creates a CollectionGuard and attempts to acquire the specified collection
  /// from the given database using the collection ID. Permission checking is
  /// performed during acquisition to ensure the current user has access.
  ///
  /// @param vocbase Database from which to acquire the collection
  /// @param cid ID of the collection to acquire
  ///
  /// @note Throws an exception if the collection doesn't exist or cannot be accessed
  /// @note Permission checking is performed during acquisition
  /// @note The collection is automatically released when the guard is destroyed
  CollectionGuard(TRI_vocbase_t* vocbase, DataSourceId cid)
      : _vocbase(vocbase),
        _collection(_vocbase->useCollection(cid, /*checkPermissions*/ true)) {
    // useCollection will throw if the collection does not exist
    TRI_ASSERT(_collection != nullptr);
  }

  /// @brief Create the guard by acquiring a collection using its name or ID
  ///
  /// Creates a CollectionGuard and attempts to acquire the specified collection
  /// from the given database using either the collection name or ID. If the name
  /// starts with a digit, it's treated as a numeric ID; otherwise, it's treated
  /// as a collection name. Permission checking is performed during acquisition.
  ///
  /// @param vocbase Database from which to acquire the collection
  /// @param name Name or string representation of ID of the collection to acquire
  ///
  /// @note Throws an exception if the collection doesn't exist or cannot be accessed
  /// @note Names starting with digits are parsed as collection IDs
  /// @note Permission checking is performed during acquisition
  /// @note The collection is automatically released when the guard is destroyed
  CollectionGuard(TRI_vocbase_t* vocbase, std::string const& name)
      : _vocbase(vocbase), _collection(nullptr) {
    if (!name.empty() && name[0] >= '0' && name[0] <= '9') {
      DataSourceId id{NumberUtils::atoi_zero<DataSourceId::BaseType>(
          name.data(), name.data() + name.size())};
      _collection = _vocbase->useCollection(id, /*checkPermissions*/ true);
    } else {
      _collection = _vocbase->useCollection(name, /*checkPermissions*/ true);
    }
    // useCollection will throw if the collection does not exist
    TRI_ASSERT(_collection != nullptr);
  }

  /// @brief Destructor that automatically releases the collection
  ///
  /// Automatically releases the collection reference when the guard
  /// goes out of scope. This ensures proper resource cleanup and prevents
  /// collection leaks. The release is safe even if the collection is null.
  ///
  /// @note The collection is automatically released if it was successfully acquired
  /// @note No manual cleanup is required
  /// @note Safe to call even if the collection is null (after move)
  ~CollectionGuard() {
    if (_collection != nullptr) {
      _vocbase->releaseCollection(_collection.get());
    }
  }

 public:
  /// @brief Get the collection pointer managed by this guard
  ///
  /// Returns a pointer to the LogicalCollection managed by this guard.
  /// The collection is guaranteed to be valid as long as the guard exists,
  /// since acquisition throws on failure.
  ///
  /// @return Pointer to the LogicalCollection
  ///
  /// @note The collection pointer is valid as long as the guard exists
  /// @note The collection was acquired with permission checking
  /// @note Returns nullptr only if the guard was moved from
  arangodb::LogicalCollection* collection() const { return _collection.get(); }

 private:
  /// @brief Pointer to the database containing the collection
  ///
  /// Reference to the database from which the collection was acquired.
  /// Used for releasing the collection when the guard is destroyed.
  /// Set to nullptr after the guard is moved from.
  TRI_vocbase_t* _vocbase;

  /// @brief Shared pointer to the managed collection
  ///
  /// Holds the reference to the LogicalCollection that this guard manages.
  /// The collection is acquired during guard construction and released
  /// during destruction. Reset to null when the guard is moved from.
  std::shared_ptr<arangodb::LogicalCollection> _collection;
};
}  // namespace arangodb
