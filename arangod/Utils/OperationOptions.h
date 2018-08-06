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
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"

namespace arangodb {
// a struct for keeping document modification operations in transactions
struct OperationOptions {
  OperationOptions()
      : recoveryData(nullptr), waitForSync(false), keepNull(true),
        mergeObjects(true), silent(false), ignoreRevs(true),
        returnOld(false), returnNew(false), isRestore(false), overwrite(false),
        oneTransactionPerDocument(false),checkGraphs(false),
        indexOperationMode(Index::OperationMode::normal) {}

  // original marker, set by an engine's recovery procedure only!
  void* recoveryData;

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

  // for insert operations: do not fail if _key exists but replace the document
  bool overwrite;

  bool oneTransactionPerDocument; // for batch operations
  bool checkGraphs; // for batch operations

  // for synchronous replication operations, we have to mark them such that
  // we can deny them if we are a (new) leader, and that we can deny other
  // operation if we are merely a follower. Finally, we must deny replications
  // from the wrong leader.
  std::string isSynchronousReplicationFrom;
  std::string graphName; // for batch operations
  Index::OperationMode indexOperationMode;

};

inline void toVelocyPack(VPackBuilder* builder ,OperationOptions const& options){
  TRI_ASSERT(builder->isOpenObject());
  //builder->add("recoveryData" ,VPackValue("how long might the data be?"));
  builder->add("waitForSync" , VPackValue(options.waitForSync));
  builder->add("keepNull"    , VPackValue(options.keepNull));
  builder->add("mergeObjects", VPackValue(options.mergeObjects));
  builder->add("silent"      , VPackValue(options.silent));
  builder->add("ignoreRevs"  , VPackValue(options.ignoreRevs));
  builder->add("returnOld"   , VPackValue(options.returnOld));
  builder->add("returnNew"   , VPackValue(options.returnNew));
  builder->add("isRestore"   , VPackValue(options.isRestore));
  builder->add("overwrite"   , VPackValue(options.overwrite));

  builder->add("indexOperationMode" , VPackValue(options.indexOperationMode));
  builder->add("isSynchronousReplicationFrom" , VPackValue(options.isSynchronousReplicationFrom));

  builder->add("oneTransactionPerDocument" , VPackValue(options.oneTransactionPerDocument));
  builder->add("checkGraphs" , VPackValue(options.checkGraphs));
  builder->add("graphName" , VPackValue(options.graphName));
};

inline std::unique_ptr<VPackBuilder> toVelocyPack(OperationOptions const& options){
  auto builder = std::make_unique<VPackBuilder>();
  builder->openObject();
  toVelocyPack(builder.get(), options);
  builder->close();
  return builder;
};

inline OperationOptions createOperationOptions(VPackSlice const& slice, void* data = nullptr){
  TRI_ASSERT(slice.isObject());
  OperationOptions options;

  options.recoveryData = data;
  options.waitForSync  = basics::VelocyPackHelper::getBooleanValue(slice, "waitForSync", options.waitForSync);
  options.keepNull     = basics::VelocyPackHelper::getBooleanValue(slice, "keepNull", options.keepNull);
  options.mergeObjects = basics::VelocyPackHelper::getBooleanValue(slice, "mergeObjects", options.mergeObjects);
  options.silent       = basics::VelocyPackHelper::getBooleanValue(slice, "silent", options.silent);
  options.ignoreRevs   = basics::VelocyPackHelper::getBooleanValue(slice, "ignoreRevs", options.ignoreRevs);
  options.returnOld    = basics::VelocyPackHelper::getBooleanValue(slice, "returnOld", options.returnOld);
  options.returnNew    = basics::VelocyPackHelper::getBooleanValue(slice, "returnNew", options.returnNew);
  options.isRestore    = basics::VelocyPackHelper::getBooleanValue(slice, "isRestore", options.isRestore);
  options.overwrite    = basics::VelocyPackHelper::getBooleanValue(slice, "overwrite", options.overwrite);
  options.overwrite    = basics::VelocyPackHelper::getBooleanValue(slice, "overwrite", options.overwrite);

  options.indexOperationMode = basics::VelocyPackHelper::getNumericValue(slice, "indexOperationMode", options.indexOperationMode);
  options.isSynchronousReplicationFrom = basics::VelocyPackHelper::getStringValue(slice, "isSynchronousReplicationFrom", options.isSynchronousReplicationFrom);

  options.oneTransactionPerDocument = basics::VelocyPackHelper::getBooleanValue(slice, "oneTransactionPerDocument", options.oneTransactionPerDocument); ;
  options.checkGraphs = basics::VelocyPackHelper::getBooleanValue(slice, "checkGraphs", options.checkGraphs);
  options.graphName = basics::VelocyPackHelper::getStringValue(slice, "graphName", options.graphName);
  return options;
}

}

#endif
