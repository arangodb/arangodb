////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_API_COLLECTIONS_H
#define ARANGOD_VOC_BASE_API_COLLECTIONS_H 1

#include "Basics/Result.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <functional>

namespace arangodb {
class LogicalCollection;
namespace methods {

/// Common code for collection REST handler and v8-collections
struct Collections {
  typedef std::function<void(LogicalCollection*)> const& FuncCallback;

  static void enumerate(TRI_vocbase_t* vocbase, FuncCallback);
  
  /// @brief lookup a collection in vocbase or clusterinfo.
  static Result lookup(TRI_vocbase_t* vocbase, std::string const& collection,
                     FuncCallback);
  /// Create collection, ownership of collection in callback is
  /// transferred to callee
  static Result create(TRI_vocbase_t* vocbase, std::string const& name,
                       TRI_col_type_e collectionType,
                       velocypack::Slice const& properties,
                       bool createWaitsForSyncReplication,
                       bool enforceReplicationFactor, FuncCallback);
  
  static Result load(TRI_vocbase_t* vocbase, LogicalCollection* coll);
  static Result unload(TRI_vocbase_t* vocbase, LogicalCollection* coll);
  
  static Result properties(LogicalCollection* coll, velocypack::Builder&);
  static Result updateProperties(LogicalCollection* coll,
                                 velocypack::Slice const&);
  
  static Result rename(LogicalCollection* coll, std::string const& newName,
                       bool doOverride);
  
  static Result drop(TRI_vocbase_t* vocbase, LogicalCollection* coll,
                     bool allowDropSystem, double timeout, bool updateUsers);
  
  static Result revisionId(TRI_vocbase_t* vocbase, LogicalCollection* coll,
                           TRI_voc_rid_t& rid);
  
  // TOCO move to rocksdb
  static Result warmup(TRI_vocbase_t* vocbase,
                       LogicalCollection* coll);
  
  // TOCO move to rocksdb only code
  static Result recalculateCount(TRI_vocbase_t* vocbase,
                                 LogicalCollection* coll);
};
#ifdef USE_ENTERPRISE
  Result ULColCoordinatorEnterprise(std::string const& databaseName,
                                    std::string const& collectionCID,
                                    TRI_vocbase_col_status_e status);
  
  Result DropColCoordinatorEnterprise(LogicalCollection* collection,
                                      bool allowDropSystem, double timeout);
#endif
}
}
#endif
