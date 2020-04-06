////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "CollectionAccess.h"

#include "Aql/Collections.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Basics/debugging.h"

#include <velocypack/Slice.h>

#include <optional>

using namespace arangodb;
using namespace arangodb::aql;

auto CollectionAccess::collection() const noexcept -> aql::Collection const* {
  return _collection;
}

CollectionAccess::CollectionAccess(aql::Collection const* collection)
    : _collection(collection) {}

CollectionAccess::CollectionAccess(aql::Collections const* const collections, velocypack::Slice const slice) {
  if (slice.get("prototype").isString()) {
    _prototypeCollection =
        collections->get(slice.get("prototype").copyString());
  }
  auto colName = slice.get("collection").copyString();
  _collection = collections->get(colName);

  TRI_ASSERT(_collection != nullptr);

  if (_collection == nullptr) {
    std::string msg("collection '");
    msg.append(slice.get("collection").copyString());
    msg.append("' not found");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg);
  }

  if (auto const isSatelliteOfSlice = slice.get("isSatelliteOf"); isSatelliteOfSlice.isInteger()) {
    auto const isSatelliteOfId = isSatelliteOfSlice.getUInt();
    _isSatelliteOf = ExecutionNodeId{isSatelliteOfId};
  } else {
    _isSatelliteOf = std::nullopt;
  }
}

void CollectionAccess::setCollection(aql::Collection const* collection) {
  TRI_ASSERT(collection != nullptr);
  _collection = collection;
}

void CollectionAccess::setPrototype(aql::Collection const* prototypeCollection,
                                    aql::Variable const* prototypeOutVariable) {
  _prototypeCollection = prototypeCollection;
  _prototypeOutVariable = prototypeOutVariable;
}

auto CollectionAccess::prototypeCollection() const noexcept -> aql::Collection const* {
  return _prototypeCollection;
}

auto CollectionAccess::prototypeOutVariable() const noexcept -> aql::Variable const* {
  return _prototypeOutVariable;
}

void CollectionAccess::useAsSatelliteOf(ExecutionNodeId prototypeAccessId) {
  _isSatelliteOf = prototypeAccessId;
}

bool CollectionAccess::isUsedAsSatellite() const noexcept {
  return _isSatelliteOf != std::nullopt;
}

auto CollectionAccess::getSatelliteOf(std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById) const
    -> ExecutionNode* {
  if (_isSatelliteOf.has_value()) {
    auto* parentNode = nodesById.at(_isSatelliteOf.value());
    auto* parentColAccess = ExecutionNode::castTo<CollectionAccessingNode*>(parentNode);
    if (parentColAccess->isUsedAsSatellite()) {
      parentNode = parentColAccess->getSatelliteOf(nodesById);
      // recursive path compression if our prototype has a prototype itself
      // this is why we want _isSatelliteOf to be mutable
      _isSatelliteOf = parentNode->id();
    }
    return parentNode;
  } else {
    return nullptr;
  }
}


auto CollectionAccess::getRawSatelliteOf() const -> std::optional<ExecutionNodeId> {
  return _isSatelliteOf;
}
