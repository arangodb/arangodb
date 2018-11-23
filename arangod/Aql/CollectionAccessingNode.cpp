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

#include "CollectionAccessingNode.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

CollectionAccessingNode::CollectionAccessingNode(aql::Collection const* collection)
    : _collection(collection) {
  TRI_ASSERT(_collection != nullptr);
}

CollectionAccessingNode::CollectionAccessingNode(ExecutionPlan* plan, 
                                                 arangodb::velocypack::Slice slice)
    : _collection(plan->getAst()->query()->collections()->get(
          slice.get("collection").copyString())) {
  TRI_ASSERT(_collection != nullptr);
  
  if (_collection == nullptr) {
    std::string msg("collection '");
    msg.append(slice.get("collection").copyString());
    msg.append("' not found");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }
  
  VPackSlice restrictedTo = slice.get("restrictedTo");

  if (restrictedTo.isString()) {
    _restrictedTo = restrictedTo.copyString();
  }
}
   
TRI_vocbase_t* CollectionAccessingNode::vocbase() const { return _collection->vocbase(); }

/// @brief modify collection after cloning
/// should be used only in smart-graph context!
void CollectionAccessingNode::collection(aql::Collection const* collection) {
  TRI_ASSERT(collection != nullptr);
  _collection = collection;
}
  
void CollectionAccessingNode::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  builder.add("database", VPackValue(_collection->vocbase()->name()));
  builder.add("collection", VPackValue(_collection->name()));
  builder.add("satellite", VPackValue(_collection->isSatellite()));
  
  if (ServerState::instance()->isCoordinator()) {
    builder.add(StaticStrings::NumberOfShards, VPackValue(_collection->numberOfShards()));
  }
  
  if (!_restrictedTo.empty()) {
    builder.add("restrictedTo", VPackValue(_restrictedTo));
  }
}

void CollectionAccessingNode::toVelocyPackHelperPrimaryIndex(arangodb::velocypack::Builder& builder) const {
  auto col = _collection->getCollection();
  builder.add(VPackValue("indexes"));
  col->getIndexesVPack(builder, Index::makeFlags(Index::Serialize::Basics), [](arangodb::Index const* idx) {
      return (idx->type() == arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX);
    });
}
