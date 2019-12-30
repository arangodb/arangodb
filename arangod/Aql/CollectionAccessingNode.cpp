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
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
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
    : _collection(collection),
      _restrictedTo(""),
      _prototypeCollection(nullptr),
      _prototypeOutVariable(nullptr),
      _usedShard(""),
      _isSatellite(false) {
  TRI_ASSERT(_collection != nullptr);
  TRI_ASSERT(_usedShard.empty());
}

CollectionAccessingNode::CollectionAccessingNode(ExecutionPlan* plan,
                                                 arangodb::velocypack::Slice slice)
    : _collection(nullptr),
      _restrictedTo(""),
      _prototypeCollection(nullptr),
      _prototypeOutVariable(nullptr),
      _usedShard(""),
      _isSatellite(false) {
  if (slice.get("prototype").isString()) {
    _prototypeCollection =
        plan->getAst()->query()->collections()->get(slice.get("prototype").copyString());
  }
  auto colName = slice.get("collection").copyString();
  auto typeId = basics::VelocyPackHelper::getNumericValue<int>(slice, "typeID", 0);
  if (typeId == ExecutionNode::DISTRIBUTE) {
    // This is a special case, the distribute node can inject a collection
    // that is NOT yet known to the plan
    auto query = plan->getAst()->query();
    query->addCollection(colName, AccessMode::Type::NONE);
  }

  _collection = plan->getAst()->query()->collections()->get(colName);

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

  _isSatellite = basics::VelocyPackHelper::getBooleanValue(slice, "isSatellite", false);
}

TRI_vocbase_t* CollectionAccessingNode::vocbase() const {
  return _collection->vocbase();
}

/// @brief modify collection after cloning
/// should be used only in smart-graph context!
void CollectionAccessingNode::collection(aql::Collection const* collection) {
  TRI_ASSERT(collection != nullptr);
  _collection = collection;
}

void CollectionAccessingNode::toVelocyPack(arangodb::velocypack::Builder& builder,
                                           unsigned /*flags*/) const {
  builder.add("database", VPackValue(_collection->vocbase()->name()));
  if (!_usedShard.empty()) {
    builder.add("collection", VPackValue(_usedShard));
  } else {
    builder.add("collection", VPackValue(_collection->name()));
  }

  if (_prototypeCollection != nullptr) {
    builder.add("prototype", VPackValue(_prototypeCollection->name()));
  }
  builder.add(StaticStrings::Satellite, VPackValue(_collection->isSatellite()));

  if (ServerState::instance()->isCoordinator()) {
    builder.add(StaticStrings::NumberOfShards, VPackValue(_collection->numberOfShards()));
  }

  if (!_restrictedTo.empty()) {
    builder.add("restrictedTo", VPackValue(_restrictedTo));
  }
#ifdef USE_ENTERPRISE
  // why do we have _isSatellite and _collection->isSatellite()?
  builder.add("isSatellite", VPackValue(_isSatellite));
#endif
}

void CollectionAccessingNode::toVelocyPackHelperPrimaryIndex(arangodb::velocypack::Builder& builder) const {
  auto col = _collection->getCollection();
  builder.add(VPackValue("indexes"));
  col->getIndexesVPack(builder, [](arangodb::Index const* idx, uint8_t& flags) {
                         if (idx->type() == arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
                           flags = Index::makeFlags(Index::Serialize::Basics);
                           return true;
                         }

                         return false;
                       });
}

void CollectionAccessingNode::useAsSatellite() {
  TRI_ASSERT(_collection->isSatellite());
#ifdef USE_ENTERPRISE
  _isSatellite = true;
#endif
}

aql::Collection const* CollectionAccessingNode::collection() const {
  return _collection;
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
  _prototypeCollection = prototypeCollection;
  _prototypeOutVariable = prototypeOutVariable;
}

aql::Collection const* CollectionAccessingNode::prototypeCollection() const {
  return _prototypeCollection;
}

aql::Variable const* CollectionAccessingNode::prototypeOutVariable() const {
  return _prototypeOutVariable;
}
