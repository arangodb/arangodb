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

#include <atomic>
#include <cstdint>

namespace arangodb {

/// @brief Thread-safe global version tracker for DDL operations
///
/// This class provides a thread-safe mechanism for tracking a global version
/// number that is incremented on every DDL (Data Definition Language) operation.
/// The version number serves as a change indicator that can be used to notify
/// other components or external systems (like the agency) about schema changes.
///
/// The VersionTracker provides:
/// - Atomic increment operations for thread safety
/// - Global version tracking across all DDL operations
/// - Integration with agency notifications for distributed systems
/// - Lightweight operation with minimal overhead
///
/// @note This class is thread-safe and can be used from multiple threads
/// @note The version number starts at 0 and is incremented for each DDL operation
/// @note Used for notifying the agency and other listeners about DDL changes
/// @note DDL operations include creating, dropping, or modifying collections/indexes
class VersionTracker {
 public:
  /// @brief Create a new version tracker
  ///
  /// Initializes the version tracker with an initial version number of 0.
  /// The tracker is ready to track DDL operations immediately after construction.
  ///
  /// @note The initial version number is 0
  /// @note The tracker is thread-safe from the moment of construction
  /// @note No setup or initialization is required after construction
  VersionTracker() : _value(0) {}

  /// @brief Track a DDL operation and increment the version
  ///
  /// Records a DDL operation by incrementing the global version number.
  /// This method is called whenever a DDL operation occurs to ensure
  /// that the version number reflects the current state of the schema.
  ///
  /// @param msg Description of the DDL operation (currently unused but available for future tracing)
  ///
  /// @note This method is thread-safe and can be called from multiple threads
  /// @note The version number is atomically incremented
  /// @note The message parameter is reserved for future logging/tracing features
  /// @note Called for all DDL operations like collection creation, index modifications, etc.
  void track(char const* msg) {
    ++_value;
    // can enable this for tracking things later
    // LOG_TOPIC("08a33", TRACE, Logger::FIXME) << "version updated by " << msg;
  }

  /// @brief Get the current version number
  ///
  /// Returns the current global version number, which represents the number
  /// of DDL operations that have been performed since the tracker was created.
  /// This value is used to determine if schema changes have occurred.
  ///
  /// @return Current version number
  ///
  /// @note This method is thread-safe and can be called from multiple threads
  /// @note The returned value represents the number of DDL operations performed
  /// @note Used for comparison to detect schema changes
  /// @note The value increases monotonically (never decreases)
  uint64_t current() const { return _value; }

 private:
  /// @brief Atomic version counter
  ///
  /// Thread-safe counter that tracks the number of DDL operations performed.
  /// Uses atomic operations to ensure thread safety when multiple threads
  /// are performing DDL operations simultaneously.
  ///
  /// @note Uses atomic operations for thread safety
  /// @note Starts at 0 and increases with each DDL operation
  /// @note Never decreases during the lifetime of the tracker
  std::atomic<uint64_t> _value;
};
}  // namespace arangodb
