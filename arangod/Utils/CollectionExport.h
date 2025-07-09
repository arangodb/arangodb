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

#include <unordered_set>

#include "Basics/debugging.h"

namespace arangodb {

/// @brief Utility structure for collection export operations with field filtering
///
/// This structure provides utilities for exporting collections with support
/// for field-level filtering. It allows including or excluding specific fields
/// from documents during export operations, providing fine-grained control
/// over the exported data.
///
/// The CollectionExport provides:
/// - Field-level inclusion and exclusion filtering
/// - Flexible restriction types for different export scenarios
/// - Efficient field lookup using hash sets
/// - Static utility functions for field filtering decisions
///
/// @note This structure contains only static utilities and nested types
/// @note Used for controlling which fields are exported from collections
/// @note Supports both whitelist (include) and blacklist (exclude) approaches
/// @note Optimized for performance with hash-based field lookups
struct CollectionExport {
 public:
  /// @brief Configuration structure for field restrictions during export
  ///
  /// This structure defines how field filtering should be applied during
  /// collection export operations. It supports different restriction types
  /// to control which fields are included in the exported data.
  ///
  /// @note The fields set is used differently depending on the restriction type
  /// @note INCLUDE type treats fields as a whitelist of allowed fields
  /// @note EXCLUDE type treats fields as a blacklist of forbidden fields
  /// @note Uses hash set for efficient field lookups
  struct Restrictions {
    /// @brief Types of field restrictions for export operations
    ///
    /// Defines the different modes of field filtering that can be applied
    /// during collection export. Each type determines how the fields set
    /// should be interpreted and applied.
    enum Type { 
      /// @brief No field restrictions - all fields are exported
      RESTRICTION_NONE,
      /// @brief Include only specified fields - whitelist approach
      RESTRICTION_INCLUDE,
      /// @brief Exclude specified fields - blacklist approach  
      RESTRICTION_EXCLUDE 
    };

    /// @brief Default constructor
    ///
    /// Creates a Restrictions object with no field restrictions, meaning
    /// all fields will be included in the export by default.
    ///
    /// @note Initializes with RESTRICTION_NONE type
    /// @note Fields set starts empty
    /// @note Safe to use immediately after construction
    Restrictions() : fields(), type(RESTRICTION_NONE) {}

    /// @brief Set of field names to include or exclude
    ///
    /// Hash set containing the names of fields that should be included or
    /// excluded from the export, depending on the restriction type. Used
    /// for efficient field lookups during export processing.
    ///
    /// @note Used as whitelist when type is RESTRICTION_INCLUDE
    /// @note Used as blacklist when type is RESTRICTION_EXCLUDE
    /// @note Ignored when type is RESTRICTION_NONE
    /// @note Hash set provides O(1) lookup performance
    std::unordered_set<std::string> fields;

    /// @brief Type of field restriction to apply
    ///
    /// Determines how the fields set should be interpreted during export.
    /// Controls whether fields are treated as an inclusion list, exclusion
    /// list, or ignored entirely.
    ///
    /// @note RESTRICTION_NONE means export all fields
    /// @note RESTRICTION_INCLUDE means export only fields in the set
    /// @note RESTRICTION_EXCLUDE means export all fields except those in the set
    Type type;
  };

  /// @brief Determine if a field should be included in the export
  ///
  /// This static utility function determines whether a specific field should
  /// be included in the export based on the configured restrictions. It
  /// implements the logic for include/exclude filtering based on the
  /// restriction type and field set.
  ///
  /// @param restrictionType Type of restriction to apply (NONE, INCLUDE, EXCLUDE)
  /// @param fields Set of field names used for filtering
  /// @param key Name of the field to check for inclusion
  ///
  /// @return true if the field should be included, false if it should be excluded
  ///
  /// @note Returns true for RESTRICTION_NONE (no filtering)
  /// @note For RESTRICTION_INCLUDE, returns true only if key is in fields set
  /// @note For RESTRICTION_EXCLUDE, returns false if key is in fields set
  /// @note Uses efficient hash set lookup for field checking
  /// @note Thread-safe as it only reads from the parameters
  static bool IncludeAttribute(
      CollectionExport::Restrictions::Type const restrictionType,
      std::unordered_set<std::string> const& fields, std::string const& key) {
    if (restrictionType ==
            CollectionExport::Restrictions::RESTRICTION_INCLUDE ||
        restrictionType ==
            CollectionExport::Restrictions::RESTRICTION_EXCLUDE) {
      bool const keyContainedInRestrictions =
          (fields.find(key) != fields.end());
      if ((restrictionType ==
               CollectionExport::Restrictions::RESTRICTION_INCLUDE &&
           !keyContainedInRestrictions) ||
          (restrictionType ==
               CollectionExport::Restrictions::RESTRICTION_EXCLUDE &&
           keyContainedInRestrictions)) {
        // exclude the field
        return false;
      }
      // include the field
      return true;
    } else {
      // no restrictions
      TRI_ASSERT(restrictionType ==
                 CollectionExport::Restrictions::RESTRICTION_NONE);
      return true;
    }
    return true;
  }
};
}  // namespace arangodb
