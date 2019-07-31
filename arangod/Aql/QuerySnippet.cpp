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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "QuerySnippet.h"

#include "Aql/CollectionAccessingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IResearchViewNode.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

void QuerySnippet::addNode(ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  _nodes.push_back(node);

  switch (node->getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX:
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPSERT: {
      // We do not actually need to know the details here.
      // We just wanna know the shards!
      auto collectionAccessingNode =
          ExecutionNode::castTo<CollectionAccessingNode*>(node);
      TRI_ASSERT(collectionAccessingNode != nullptr);
      auto col = collectionAccessingNode->collection();
      auto shards = col->shardIds();
      // Satellites can only be used on ReadNodes
      bool isSatellite = col->isSatellite() &&
                         (node->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
                          ExecutionNode::INDEX);
      if (collectionAccessingNode->isRestricted()) {
        std::string const& onlyShard = collectionAccessingNode->restrictedShard();
        bool found =
            std::find(shards->begin(), shards->end(), onlyShard) != shards->end();
        if (!found) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_CLUSTER_SHARD_GONE,
              "Collection could not be restricted to the given shard: " + onlyShard +
                  " it is not part of collection " + col->name());
        }
        auto restrictedShards = std::make_shared<std::vector<ShardID>>();
        restrictedShards->emplace_back(onlyShard);
        _expansions.emplace_back(node, false, restrictedShards, isSatellite);
      } else {
        // Satellite can only have a single shard, and we have a modification node here.
        TRI_ASSERT(!isSatellite || shards->size() == 1);
        if (shards->size() > 1) {
          _needToInjectGather = true;
        }
        _expansions.emplace_back(node, shards->size() > 1, shards, isSatellite);
      }
      break;
    }
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode*>(node);

      // evaluate node volatility before the distribution
      // can't do it on DB servers since only parts of the plan will be sent
      viewNode->volatility(true);
      auto collections = viewNode->collections();
      auto shardList = std::make_shared<std::vector<ShardID>>();
      for (aql::Collection const& c : collections) {
        auto shards = c.shardIds();
        shardList->insert(shardList->end(), shards->begin(), shards->end());
      }
      _expansions.emplace_back(node, false, shardList, false);
      break;
    }
    default:
      // do nothing
      break;
  }
}

void QuerySnippet::serializeIntoBuilder(ServerID const& server,
                                        std::unordered_map<ShardID, ServerID> const& shardMapping,
                                        VPackBuilder& infoBuilder) const {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(!_expansions.empty());
  auto it = _expansions.find(server);
  if (it == _expansions.end()) {
    // We do not have a snippet for this server sorry.
    return;
  }
  // TODO toVPack all nodes for this specific server
  // We clone every Node* and maintain a list of ReportingGroups for profiler
}
