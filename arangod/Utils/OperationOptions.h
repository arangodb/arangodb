////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"
#include "Utils/ExecContext.h"

#include <string>

namespace arangodb {
namespace velocypack {
class StringRef;
}
class ExecContext;

/// @brief Indicates whether we want to observe writes performed within the
/// current (sub) transaction. This is only relevant for AQL queries.
/// AQL queries are performed transcationally, i.e., either all changes are
/// visible or none (ignoring intermediate commits). A query should observe
/// (only) the state of the db/transaction at the time the query was started,
/// e.g., documents that are inserted as part of the current query should not
/// be visible, otherwise we could easily produce endless loops:
///   FOR doc IN col INSERT doc INTO col
/// However, some operations still need to observe these writes. For example,
/// the internal subquery for an UPSERT must see documents that a previous
/// UPSERT has inserted. Likewise, modification operations also need to observe
/// all changes in order to perform unique constraint checks. Therefore, every
/// read operation must specify whether writes performed within the same (sub)
/// transaction should be visible or not.
/// A standalone AQL query represents a single transaction; an AQL query which
/// is executed inside a streaming transaction is a kind of _sub-transaction_,
/// i.e., it should observe the changes performed within the transaction so far,
/// but not the changes performed by the query itself. For more details see
/// RocksDBTrxMethods.
enum class ReadOwnWrites : bool { no, yes, };

/// @brief: mode to signal how operation should behave
enum class IndexOperationMode : uint8_t { normal, internal, rollback };

// a struct for keeping document modification operations in transactions
#if defined(__GNUC__) && (__GNUC__ > 9 || (__GNUC__ == 9 && __GNUC_MINOR__ >= 2))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

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
  
  OperationOptions() = default;
  explicit OperationOptions(ExecContext const&);
  
// The following code does not work with VisualStudio 2019's `cl`
// Lets keep it for debugging on linux.
#ifndef _WIN32
  friend std::ostream& operator<<(std::ostream& os, OperationOptions const& ops);
#endif

  bool isOverwriteModeSet() const noexcept {
    return (overwriteMode != OverwriteMode::Unknown);
  }
  
  bool isOverwriteModeUpdateReplace() const noexcept {
    return (overwriteMode == OverwriteMode::Update || overwriteMode == OverwriteMode::Replace);
  }

  /// @brief stringifies the index operation mode
  static char const* stringifyIndexOperationMode(IndexOperationMode mode) noexcept;
  
  /// @brief stringifies the overwrite mode
  static char const* stringifyOverwriteMode(OperationOptions::OverwriteMode mode) noexcept;

  /// @brief determine the overwrite mode from the string value
  static OverwriteMode determineOverwriteMode(velocypack::StringRef value);
  
 public:
  
  // for synchronous replication operations, we have to mark them such that
  // we can deny them if we are a (new) leader, and that we can deny other
  // operation if we are merely a follower. Finally, we must deny replications
  // from the wrong leader.
  std::string isSynchronousReplicationFrom;
 
  IndexOperationMode indexOperationMode = IndexOperationMode::normal;

  // INSERT ... OPTIONS { overwrite: true } behavior: 
  // - replace an existing document, update an existing document, or do nothing.
  OverwriteMode overwriteMode = OverwriteMode::Unknown;

  // wait until the operation has been synced.
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool waitForSync = false;

  // apply document vaidators if there are any available
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool validate = true;

  // keep null values on update (=true) or remove them (=false). only used for
  // update operations
  bool keepNull = true;

  // merge objects. only used for update operations
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool mergeObjects = true;

  // be silent. this will build smaller results and thus may speed up operations
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool silent = false;

  // ignore _rev attributes given in documents (for replace and update)
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool ignoreRevs = true;

  // returnOld: for replace, update and remove return previous value
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool returnOld = false;

  // returnNew: for insert, replace and update return complete new value
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool returnNew = false;

  // for insert operations: use _key value even when this is normally prohibited
  // for the end user this option is there to ensure _key values once set can be
  // restored by replication and arangorestore
  // NOTE: if this default value is ever changed, please make sure to check if 
  // function `addRequestOptionParameter` in ClusterMethods.cpp and its callers 
  // also need adjustment. 
  bool isRestore = false;

  // for replication; only set true if case insert/replace should have a read-only
  // preflight phase, in which it checks whether a document can actually be inserted
  // before carrying out the actual insert/replace.
  // separating the check phase from the actual insert/replace allows running the
  // preflight check without modifying the transaction's underlying WriteBatch object,
  // so in case a unique constraint violation is detected, it does not need to be
  // rebuilt (this would be _very_ expensive).
  bool checkUniqueConstraintsInPreflight = false;

  // when truncating - should we also run the compaction?
  bool truncateCompact = true;

  // whether or not this request is a DOCUMENT() call from inside AQL. only set
  // for exactly this case on a coordinator, in order to make it set a special
  // header when putting together the requests for DB servers.
  bool documentCallFromAql = false;

  // whether or not indexing can be disabed. We must not disable indexing if we have to ensure
  // that writes become visible to the current query.
  // This is necessary for UPSERTS where the subquery relies on a non-unique secondary index.
  bool canDisableIndexing = true;

  // default values for operation options, created at program start.
  // these are used by function `addRequestOptionParameter` and callers to determine
  // if one of the options is at its default value or not.
  static OperationOptions const defaultValues;

  // get associated execution context
  ExecContext const& context() const;

 private:
  ExecContext const* _context = nullptr;
};

#if defined(__GNUC__) && (__GNUC__ > 9 || (__GNUC__ == 9 && __GNUC_MINOR__ >= 2))
#pragma GCC diagnostic pop
#endif

}  // namespace arangodb

