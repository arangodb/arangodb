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

#include "CollectionAccessingNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

CollectionAccessingNode::CollectionAccessingNode(aql::Collection const* collection)
    : _collectionAccess(collection) {
  TRI_ASSERT(_collectionAccess.collection() != nullptr);
  TRI_ASSERT(_usedShard.empty());
}

CollectionAccessingNode::CollectionAccessingNode(ExecutionPlan* plan,
                                                 arangodb::velocypack::Slice slice) {
  aql::QueryContext& query = plan->getAst()->query();
  auto colName = slice.get("collection").copyString();
  auto typeId = basics::VelocyPackHelper::getNumericValue<int>(slice, "typeID", 0);
  if (typeId == ExecutionNode::DISTRIBUTE) {
    // This is a special case, the distribute node can inject a collection
    // that is NOT yet known to the plan
    query.collections().add(colName, AccessMode::Type::NONE, aql::Collection::Hint::Collection);
  }
  // After we optionally added the collection for distribute we can create
  // the CollectionAccess:
  _collectionAccess = CollectionAccess{&plan->getAst()->query().collections(), slice};

  VPackSlice restrictedTo = slice.get("restrictedTo");

  if (restrictedTo.isString()) {
    _restrictedTo = restrictedTo.copyString();
  }
}

TRI_vocbase_t* CollectionAccessingNode::vocbase() const {
  return collection()->vocbase();
}

/// @brief modify collection after cloning
/// should be used only in smart-graph context!
void CollectionAccessingNode::collection(aql::Collection const* collection) {
  TRI_ASSERT(collection != nullptr);
  _collectionAccess.setCollection(collection);
}

void CollectionAccessingNode::toVelocyPack(arangodb::velocypack::Builder& builder,
                                           unsigned /*flags*/) const {
  builder.add("database", VPackValue(collection()->vocbase()->name()));
  if (!_usedShard.empty()) {
    builder.add("collection", VPackValue(_usedShard));
  } else {
    builder.add("collection", VPackValue(collection()->name()));
  }

  if (prototypeCollection() != nullptr) {
    builder.add("prototype", VPackValue(prototypeCollection()->name()));
  }
  builder.add(StaticStrings::Satellite, VPackValue(collection()->isSatellite()));

  if (ServerState::instance()->isCoordinator()) {
    builder.add(StaticStrings::NumberOfShards, VPackValue(collection()->numberOfShards()));
  }

  if (!_restrictedTo.empty()) {
    builder.add("restrictedTo", VPackValue(_restrictedTo));
  }
#ifdef USE_ENTERPRISE
  builder.add("isSatellite", VPackValue(isUsedAsSatellite()));
  builder.add("isSatelliteOf", isUsedAsSatellite()
                                   ? VPackValue(getRawSatelliteOf().value().id(), VPackValueType::UInt)
                                   : VPackValue(VPackValueType::Null));
#endif
}

void CollectionAccessingNode::toVelocyPackHelperPrimaryIndex(arangodb::velocypack::Builder& builder) const {
  auto col = collection()->getCollection();
  builder.add(VPackValue("indexes"));
  col->getIndexesVPack(builder, [](arangodb::Index const* idx, uint8_t& flags) {
                         if (idx->type() == arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
                           flags = Index::makeFlags(Index::Serialize::Basics);
                           return true;
                         }

                         return false;
                       });
}

bool CollectionAccessingNode::isUsedAsSatellite() const {
  return _collectionAccess.isUsedAsSatellite();
}

void CollectionAccessingNode::useAsSatelliteOf(ExecutionNodeId prototypeAccessId) {
  TRI_ASSERT(collection()->isSatellite());
  _collectionAccess.useAsSatelliteOf(prototypeAccessId);
}

auto CollectionAccessingNode::getSatelliteOf(
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById) const
    -> ExecutionNode* {
  return _collectionAccess.getSatelliteOf(nodesById);
}

auto CollectionAccessingNode::getRawSatelliteOf() const -> std::optional<aql::ExecutionNodeId> {
  return _collectionAccess.getRawSatelliteOf();
}

aql::Collection const* CollectionAccessingNode::collection() const {
  return _collectionAccess.collection();
}

void CollectionAccessingNode::restrictToShard(std::string const& shardId) {
  _restrictedTo = shardId;
}

bool CollectionAccessingNode::isRestricted() const {
  return !_restrictedTo.empty();
}

std::string const& CollectionAccessingNode::restrictedShard() const {
  return _restrictedTo;
}

void CollectionAccessingNode::setPrototype(arangodb::aql::Collection const* prototypeCollection,
                                           arangodb::aql::Variable const* prototypeOutVariable) {
  _collectionAccess.setPrototype(prototypeCollection, prototypeOutVariable);
}

aql::Collection const* CollectionAccessingNode::prototypeCollection() const {
  return _collectionAccess.prototypeCollection();
}

aql::Variable const* CollectionAccessingNode::prototypeOutVariable() const {
  return _collectionAccess.prototypeOutVariable();
}

auto CollectionAccessingNode::collectionAccess() const -> aql::CollectionAccess const& {
  return _collectionAccess;
}
