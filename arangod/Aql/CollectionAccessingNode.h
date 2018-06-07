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

#ifndef ARANGOD_AQL_COLLECTION_ACCESSING_NODE_H
#define ARANGOD_AQL_COLLECTION_ACCESSING_NODE_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
struct Collection;
class ExecutionPlan;

class CollectionAccessingNode {
 public:
  explicit CollectionAccessingNode(aql::Collection const* collection);
  CollectionAccessingNode(ExecutionPlan* plan, arangodb::velocypack::Slice slice);

 public:
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  
  /// @brief return the database
  TRI_vocbase_t* vocbase() const;

  /// @brief return the collection
  aql::Collection const* collection() const { return _collection; }

  /// @brief modify collection after cloning
  /// should be used only in smart-graph context!
  void collection(aql::Collection const* collection); 

 protected:
  aql::Collection const* _collection;
};

}
}

#endif
