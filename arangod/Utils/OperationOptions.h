////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Indexes/Index.h"

namespace arangodb {
// a struct for keeping document modification operations in transactions
struct OperationOptions {
  OperationOptions()
      : indexOperationMode(Index::OperationMode::normal),
        waitForSync(false),
        validate(true),
        keepNull(true),
        mergeObjects(true),
        silent(false),
        ignoreRevs(true),
        returnOld(false),
        returnNew(false),
        isRestore(false),
        overwrite(false),
        ignoreUniqueConstraints(false),
        overwriteModeUpdate(false)
        {}

// The following code does not work with VisualStudi 2019's `cl`
// Lets keep it for debugging on linux.
#ifndef _WIN32
  friend std::ostream& operator<<(std::ostream& os, OperationOptions const& ops) {
    // clang-format off
    os << "OperationOptions : " << std::boolalpha
       << "{ indexOperationMode : " << ops.indexOperationMode
       << ", waitForSync : " << ops.waitForSync
       << ", validate : " << ops.validate
       << ", keepNull : " << ops.keepNull
       << ", mergeObjects : " << ops.mergeObjects
       << ", silent : " << ops.silent
       << ", ignoreRevs : " << ops.ignoreRevs
       << ", returnOld :" << ops.returnOld
       << ", returnNew : "  << ops.returnNew
       << ", isRestore : " << ops.isRestore
       << ", overwrite : " << ops.overwrite
       << ", overwriteModeUpdate : " << ops.overwriteModeUpdate
       << " }" << std::endl;
    // clang-format on
    return os;
  }
#endif

  Index::OperationMode indexOperationMode;

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

  // for insert operations: do not fail if _key exists but replace the document
  bool overwrite;

  // for replication; only set true if you an guarantee that any conflicts have
  // already been removed, and are simply not reflected in the transaction read
  bool ignoreUniqueConstraints;
  
  // above replace becomes an update
  bool overwriteModeUpdate;

  // for synchronous replication operations, we have to mark them such that
  // we can deny them if we are a (new) leader, and that we can deny other
  // operation if we are merely a follower. Finally, we must deny replications
  // from the wrong leader.
  std::string isSynchronousReplicationFrom;
};


}  // namespace arangodb

#endif
