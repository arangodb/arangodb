////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/CollectionAccess.h"
#include "Aql/ExecutionNodeId.h"
#include "Basics/debugging.h"

#include <optional>
#include <string>
#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace aql {
struct Collection;
class ExecutionPlan;
struct Variable;

class CollectionAccessingNode {
 public:
  explicit CollectionAccessingNode(aql::Collection const* collection);
  CollectionAccessingNode(ExecutionPlan* plan, arangodb::velocypack::Slice slice);
  virtual ~CollectionAccessingNode() = default;

 public:
  void toVelocyPack(arangodb::velocypack::Builder& builder, unsigned flags) const;

  /// @brief dumps the primary index
  void toVelocyPackHelperPrimaryIndex(arangodb::velocypack::Builder& builder) const;

  /// @brief return the database
  TRI_vocbase_t* vocbase() const;

  /// @brief return the collection
  aql::Collection const* collection() const;

  /// @brief modify collection after cloning
  /// should be used only in smart-graph context!
  void collection(aql::Collection const* collection);

  void setUsedShard(std::string const& shardName) {
    // We can only use the shard we are restricted to
    TRI_ASSERT(shardName.empty() || _restrictedTo.empty() || _restrictedTo == shardName);
    _usedShard = shardName;
  }

  /**
   * @brief Restrict this Node to a single Shard (cluster only)
   *
   * @param shardId The shard restricted to
   */
  void restrictToShard(std::string const& shardId);

  /**
   * @brief Check if this Node is restricted to a single Shard (cluster only)
   *
   * @return True if we are restricted, false otherwise
   */
  bool isRestricted() const;

  /**
   * @brief Get the Restricted shard for this Node
   *
   * @return The Shard this node is restricted to
   */
  std::string const& restrictedShard() const;

  /// @brief set the prototype collection when using distributeShardsLike
  void setPrototype(arangodb::aql::Collection const* prototypeCollection,
                    arangodb::aql::Variable const* prototypeOutVariable);

  aql::Collection const* prototypeCollection() const;
  aql::Variable const* prototypeOutVariable() const;

  bool isUsedAsSatellite() const;

  /// @brief Use this collection access as a satellite of the prototype.
  /// This will work transitively, even if the prototypeAccess is only
  /// subsequently marked as a satellite of another access. However, after
  /// se- and deserialization, this won't work anymore.
  void useAsSatelliteOf(ExecutionNodeId prototypeAccessId);

  void cloneInto(CollectionAccessingNode& c) const {
    c._collectionAccess = _collectionAccess;
    c._restrictedTo = _restrictedTo;
    c._usedShard = _usedShard;
  }

  /// @brief Get the CollectionAccess of which *this* collection access is a
  /// satellite of, if any.
  /// This will make a recursive lookup, so if A isSatelliteOf B, and B isSatelliteOf C,
  /// A.getSatelliteOf() will return C.
  auto getSatelliteOf(std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById) const
      -> ExecutionNode*;

  /// @brief Get local value of getSatelliteOf, without resolving it recursively.
  auto getRawSatelliteOf() const -> std::optional<aql::ExecutionNodeId>;

  auto collectionAccess() const -> aql::CollectionAccess const&;

 protected:
  aql::CollectionAccess _collectionAccess;

  /// @brief A shard this node is restricted to, may be empty
  std::string _restrictedTo;

  std::string _usedShard;
};

}  // namespace aql
}  // namespace arangodb

#endif
