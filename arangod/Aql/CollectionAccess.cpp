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
#include "Basics/debugging.h"

#include <velocypack/Slice.h>

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

  if (auto const isSatelliteOfSlice = slice.get("isSatelliteOf"); isSatelliteOfSlice.isString()) {
    auto const isSatelliteOfName = isSatelliteOfSlice.stringView();
    // Note that because this will get newly created, different CollectionAccess
    // instances are no longer related if isSatelliteOf, because each gets its
    // own parent instance.
    // This is fine because changes to it can only happen during planning/optimization.
    // It can never happen after instantiation.
    // If this is needed for some reason in the future, CollectionAccesses would
    // need to get some ID and be deserialized monolithically.
    _isSatelliteOf = std::make_shared<CollectionAccess>(collections->get(isSatelliteOfName));
    TRI_ASSERT(_isSatelliteOf != nullptr);
    if (_isSatelliteOf == nullptr) {
      using namespace std::string_literals;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "Collection not found to be satellite of: "s.append(isSatelliteOfName));
    }
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

void CollectionAccess::useAsSatelliteOf(std::shared_ptr<CollectionAccess const> prototypeAccess) {
  _isSatelliteOf = std::move(prototypeAccess);
}

bool CollectionAccess::isUsedAsSatellite() const noexcept {
  return _isSatelliteOf != nullptr;
}

auto CollectionAccess::getSatelliteOf() const
    -> std::shared_ptr<aql::CollectionAccess const> const& {
  if (_isSatelliteOf->isUsedAsSatellite()) {
    // recursive path compression if our prototype has a prototype itself
    // this is why we want _isSatelliteOf to be mutable
    _isSatelliteOf = _isSatelliteOf->getSatelliteOf();
  }
  return _isSatelliteOf;
}
