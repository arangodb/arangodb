////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_METADATA_H
#define ARANGODB_CACHE_METADATA_H

#include "Basics/ReadWriteSpinLock.h"

#include <atomic>
#include <cstdint>

namespace arangodb {
namespace cache {

class Cache;  // forward declaration

////////////////////////////////////////////////////////////////////////////////
/// @brief Metadata object to facilitate information sharing between individual
/// Cache instances and Manager.
////////////////////////////////////////////////////////////////////////////////
struct Metadata {
  // info about allocated memory
  std::uint64_t fixedSize;
  std::uint64_t tableSize;
  std::uint64_t maxSize;
  std::uint64_t allocatedSize;
  std::uint64_t deservedSize;

  // vital information about memory usage
  std::atomic<std::uint64_t> usage;
  std::uint64_t softUsageLimit;
  std::uint64_t hardUsageLimit;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Default constructor for placeholder objects.
  //////////////////////////////////////////////////////////////////////////////
  Metadata();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes record with given information.
  //////////////////////////////////////////////////////////////////////////////
  Metadata(std::uint64_t usage, std::uint64_t fixed, std::uint64_t table, std::uint64_t max);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes record from an existing record.
  //////////////////////////////////////////////////////////////////////////////
  Metadata(Metadata&& other) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes record from an existing record.
  //////////////////////////////////////////////////////////////////////////////
  Metadata& operator=(Metadata&& other) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the lock for the metadata structure
  //////////////////////////////////////////////////////////////////////////////
  basics::ReadWriteSpinLock& lock() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Adjusts usage by the specified amount if it will not violate
  /// limits. Requires record to be read-locked.
  ///
  /// Returns true if adjusted, false otherwise. Used by caches to check-and-set
  /// in a single operation to determine whether they can afford to store a new
  /// value.
  //////////////////////////////////////////////////////////////////////////////
  bool adjustUsageIfAllowed(std::int64_t usageChange) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the soft and hard usage limits. Requires record to be
  /// write-locked.
  //////////////////////////////////////////////////////////////////////////////
  bool adjustLimits(std::uint64_t softLimit, std::uint64_t hardLimit) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the deserved size. Requires record to be write-locked.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t adjustDeserved(std::uint64_t deserved) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Calculates the new usage limit based on deserved size and other
  /// values. Requires record to be read-locked.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t newLimit() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks feasibility of new table size prior to migration. Requires
  /// record to be read-locked.
  ///
  /// If migrating to table of new size would exceed either deserved or maximum
  /// size, then returns false.
  //////////////////////////////////////////////////////////////////////////////
  bool migrationAllowed(std::uint64_t newTableSize) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the table size after migration. Requires record to be
  /// write-locked.
  //////////////////////////////////////////////////////////////////////////////
  void changeTable(std::uint64_t newTableSize) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks if cache is migrating. Requires record to be read-locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isMigrating() const noexcept { return _migrating; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks if the cache is resizing. Requires record to be read-locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isResizing() const noexcept { return _resizing; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Toggles the migrating flag. Requires record to be write-locked.
  //////////////////////////////////////////////////////////////////////////////
  void toggleMigrating() noexcept { _migrating = !_migrating; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Toggles the resizing flag. Requires record to be write-locked.
  //////////////////////////////////////////////////////////////////////////////
  void toggleResizing() noexcept { _resizing = !_resizing; }

 private:
  mutable basics::ReadWriteSpinLock _lock;
  bool _migrating;
  bool _resizing;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
