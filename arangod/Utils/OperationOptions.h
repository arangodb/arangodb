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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_OPERATION_OPTIONS_H
#define ARANGOD_UTILS_OPERATION_OPTIONS_H 1

#include "Basics/Common.h"
#include "Utils/ExecContext.h"

#include <string>

namespace arangodb {
namespace velocypack {
class StringRef;
}
class ExecContext;

/// @brief: mode to signal how operation should behave
enum class IndexOperationMode : uint8_t { normal, internal, rollback };

// a struct for keeping document modification operations in transactions
struct OperationOptions {

  /// @brief behavior when inserting a document by _key using INSERT with overwrite
  /// when the target document already exists
  enum class OverwriteMode {
    Unknown,  // undefined/not set
    Conflict, // fail with unique constraint violation
    Replace,  // replace the target document
    Update,   // (partially) update the target document
    Ignore    // keep the target document unmodified (no writes)
  };
  
  OperationOptions();
  explicit OperationOptions(ExecContext const&);
  
// The following code does not work with VisualStudi 2019's `cl`
// Lets keep it for debugging on linux.
#ifndef _WIN32
  friend std::ostream& operator<<(std::ostream& os, OperationOptions const& ops);
#endif

  bool isOverwriteModeSet() const {
    return (overwriteMode != OverwriteMode::Unknown);
  }
  
  bool isOverwriteModeUpdateReplace() const {
    return (overwriteMode == OverwriteMode::Update || overwriteMode == OverwriteMode::Replace);
  }
  
  /// @brief stringifies the overwrite mode
  static char const* stringifyOverwriteMode(OperationOptions::OverwriteMode mode);

  /// @brief determine the overwrite mode from the string value
  static OverwriteMode determineOverwriteMode(velocypack::StringRef value);
  
 public:
  
  // for synchronous replication operations, we have to mark them such that
  // we can deny them if we are a (new) leader, and that we can deny other
  // operation if we are merely a follower. Finally, we must deny replications
  // from the wrong leader.
  std::string isSynchronousReplicationFrom;
 
  IndexOperationMode indexOperationMode;

  // INSERT ... OPTIONS { overwrite: true } behavior: 
  // - replace an existing document, update an existing document, or do nothing
  OverwriteMode overwriteMode;

  // wait until the operation has been synced
  bool waitForSync;

  // apply document vaidators if there are any available
  bool validate;

  // keep null values on update (=true) or remove them (=false). only used for
  // update operations
  bool keepNull;

  // merge objects. only used for update operations
  bool mergeObjects;

  // be silent. this will build smaller results and thus may speed up operations
  bool silent;

  // ignore _rev attributes given in documents (for replace and update)
  bool ignoreRevs;

  // returnOld: for replace, update and remove return previous value
  bool returnOld;

  // returnNew: for insert, replace and update return complete new value
  bool returnNew;

  // for insert operations: use _key value even when this is normally prohibited
  // for the end user this option is there to ensure _key values once set can be
  // restored by replicated and arangorestore
  bool isRestore;

  // for replication; only set true if you an guarantee that any conflicts have
  // already been removed, and are simply not reflected in the transaction read
  bool ignoreUniqueConstraints;

  // when truncating - should we also run the compaction?
  // defaults to true.
  bool truncateCompact;

  // get associated execution context
  ExecContext const& context() const;

private:
  ExecContext const* _context;
};

}  // namespace arangodb

#endif
