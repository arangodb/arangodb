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

namespace arangodb {
namespace wal {
class Marker;
}

// a struct for keeping document modification operations in transactions
struct OperationOptions {
  OperationOptions() 
      : recoveryMarker(nullptr), waitForSync(false), keepNull(true),
        mergeObjects(true), silent(false), ignoreRevs(true),
        returnOld(false), returnNew(false), isRestore(false) {}

  // original marker, set by the recovery procedure only!
  arangodb::wal::Marker* recoveryMarker;

  // wait until the operation has been synced
  bool waitForSync;

  // keep null values on update (=true) or remove them (=false). only used for update operations
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

  // for insert operations: use _key value even when this is normally prohibited for the end user
  // this option is there to ensure _key values once set can be restored by replicated and arangorestore
  bool isRestore;
};

}

#endif
