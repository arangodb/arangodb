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

#ifndef ARANGOD_AQL_COLLECTIONACCESS_H
#define ARANGOD_AQL_COLLECTIONACCESS_H

#include <memory>
#include <optional>
#include <unordered_map>

#include "Aql/ExecutionNodeId.h"

namespace arangodb::velocypack {
class Slice;
}

namespace arangodb::aql {
class Collections;
class ExecutionNode;
struct Collection;
struct Variable;

/**
 * @brief Holds information how an aql::Collection is accessed. This is on the
 * Node level during query planning, so can be different for different accesses
 * of the same collection. It's not used to differentiate shards.
 */
class CollectionAccess {
 public:
  CollectionAccess() = default;
  explicit CollectionAccess(aql::Collection const*);
  CollectionAccess(aql::Collections const*, velocypack::Slice);

  auto collection() const noexcept -> aql::Collection const*;

  /// @brief modify collection after cloning
  /// should be used only in smart-graph context!
  void setCollection(aql::Collection const* collection);

  /// @brief set the prototype collection when using distributeShardsLike
  void setPrototype(aql::Collection const* prototypeCollection,
                    aql::Variable const* prototypeOutVariable);

  auto prototypeCollection() const noexcept -> aql::Collection const*;
  auto prototypeOutVariable() const noexcept -> aql::Variable const*;

  auto isUsedAsSatellite() const noexcept -> bool;

  /// @brief Use this collection access as a satellite of the prototype.
  /// This will work transitively, even if the prototypeAccess is only
  /// subsequently marked as a satellite of another access. However, after
  /// se- and deserialization, this won't work anymore.
  void useAsSatelliteOf(ExecutionNodeId prototypeAccessId);

  /// @brief Get the CollectionAccess of which *this* collection access is a
  /// satellite of, if any.
  /// This will make a recursive lookup, so if A isSatelliteOf B, and B isSatelliteOf C,
  /// A.getSatelliteOf() will return C.
  auto getSatelliteOf(std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById) const
      -> ExecutionNode*;

  auto getRawSatelliteOf() const -> std::optional<ExecutionNodeId>;

 private:
  aql::Collection const* _collection = nullptr;

  /// @brief prototype collection when using distributeShardsLike
  aql::Collection const* _prototypeCollection = nullptr;
  aql::Variable const* _prototypeOutVariable = nullptr;

  // is a non-nullptr iff used as a SatelliteCollection.
  // mutable to allow path compression of chains of `isSatelliteOf`.
  mutable std::optional<ExecutionNodeId> _isSatelliteOf{std::nullopt};
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_COLLECTIONACCESS_H
