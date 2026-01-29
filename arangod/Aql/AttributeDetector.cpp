////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "AttributeDetector.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/CollectOptions.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/IndexCollectNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/MultipleRemoteModificationNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/SingleRemoteOperationNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Projections.h"
#include "Aql/Variable.h"
#include "Graph/BaseOptions.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserOptions.h"

#include <velocypack/Builder.h>

namespace arangodb::aql {

AttributeDetector::AttributeDetector(ExecutionPlan* plan) : _plan(plan) {}

void AttributeDetector::detect() {
  if (_plan == nullptr) {
    return;
  }

  _collectionAccessMap.clear();
  _collectionAccesses.clear();

  ExecutionNode* root = _plan->root();
  if (root != nullptr) {
    root->walk(*this);
  }

  if (_plan->getAst()->functionsMayAccessDocuments()) {
    auto& q = _plan->getAst()->query();

    for (auto const& c : q.collectionNames()) {
      auto& access = _collectionAccessMap[c];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = c;
      }
      access->requiresAllAttributesRead = true;
    }
  }

  for (auto& [collName, access] : _collectionAccessMap) {
    if (access && access->outVariable && access->readAttributes.empty() &&
        !access->requiresAllAttributesRead &&
        !access->requiresAllAttributesWrite) {
      access->requiresAllAttributesRead = true;
    }
  }

  _collectionAccesses.reserve(_collectionAccessMap.size());
  for (auto& [name, access] : _collectionAccessMap) {
    if (access) {
      _collectionAccesses.push_back(std::move(*access));
    }
  }
}

bool AttributeDetector::before(ExecutionNode* node) {
  if (node == nullptr) {
    return false;
  }

  switch (node->getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION: {
      auto* enumNode = ExecutionNode::castTo<EnumerateCollectionNode*>(node);
      std::string collName = enumNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }

      access->outVariable = enumNode->outVariable();

      Projections const& projs = enumNode->projections();
      if (!projs.empty()) {
        for (auto const& proj : projs.projections()) {
          if (!proj.path.empty()) {
            access->readAttributes.insert(proj.path);
          }
        }
      }

      Projections const& filterProjections = enumNode->filterProjections();
      if (!filterProjections.empty()) {
        for (auto const& proj : filterProjections.projections()) {
          if (!proj.path.empty()) {
            access->readAttributes.insert(proj.path);
          }
        }
      }

      if (projs.empty() && enumNode->isVarUsedLater(enumNode->outVariable())) {
        access->requiresAllAttributesRead = true;
      }
      break;
    }

    case ExecutionNode::INDEX: {
      auto* idxNode = ExecutionNode::castTo<IndexNode*>(node);
      std::string collName = idxNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }

      access->outVariable = idxNode->outVariable();

      Projections const& projs = idxNode->projections();
      if (!projs.empty()) {
        for (auto const& proj : projs.projections()) {
          if (!proj.path.empty()) {
            access->readAttributes.insert(proj.path);
          }
        }
      }

      Projections const& filterProjections = idxNode->filterProjections();
      if (!filterProjections.empty()) {
        for (auto const& proj : filterProjections.projections()) {
          if (!proj.path.empty()) {
            access->readAttributes.insert(proj.path);
          }
        }
      }

      if (projs.empty() && idxNode->isVarUsedLater(idxNode->outVariable())) {
        access->requiresAllAttributesRead = true;
      }

      Condition* cond = idxNode->condition();
      if (cond && !cond->isEmpty()) {
        containers::FlatHashSet<aql::AttributeNamePath> attributes;
        if (Ast::getReferencedAttributesRecursive(
                cond->root(), access->outVariable, "", attributes,
                _plan->getAst()->query().resourceMonitor())) {
          for (auto const& attr : attributes) {
            if (!attr.empty()) {
              access->readAttributes.insert(attr);
            }
          }
        } else {
          access->requiresAllAttributesRead = true;
        }
      }
      break;
    }

    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto* viewNode =
          ExecutionNode::castTo<iresearch::IResearchViewNode*>(node);
      for (auto const& coll : viewNode->collections()) {
        auto& access = _collectionAccessMap[coll.first.get().name()];
        if (!access) {
          access = std::make_unique<CollectionAccess>();
          access->collectionName = coll.first.get().getCollection()->name();
        }
        access->requiresAllAttributesRead = true;
      }
      break;
    }

    case ExecutionNode::TRAVERSAL: {
      auto* travNode = ExecutionNode::castTo<TraversalNode*>(node);

      Projections const& edgeProjs = travNode->options()->getEdgeProjections();
      for (auto* edgeColl : travNode->edgeColls()) {
        auto& access = _collectionAccessMap[edgeColl->name()];
        if (!access) {
          access = std::make_unique<CollectionAccess>();
          access->collectionName = edgeColl->name();
        }
        if (!edgeProjs.empty()) {
          // Projections already include _from and _to
          for (auto const& proj : edgeProjs.projections()) {
            if (!proj.path.empty()) {
              access->readAttributes.insert(proj.path);
            }
          }
        } else {
          // No projections: full edge documents are read
          access->requiresAllAttributesRead = true;
        }
      }

      if (travNode->options()->produceVertices()) {
        Projections const& vertexProjs =
            travNode->options()->getVertexProjections();
        if (!vertexProjs.empty()) {
          for (auto* vertexColl : travNode->vertexColls()) {
            auto& access = _collectionAccessMap[vertexColl->name()];
            if (!access) {
              access = std::make_unique<CollectionAccess>();
              access->collectionName = vertexColl->name();
            }
            for (auto const& proj : vertexProjs.projections()) {
              if (!proj.path.empty()) {
                access->readAttributes.insert(proj.path);
              }
            }
          }
        } else {
          for (auto* vertexColl : travNode->vertexColls()) {
            auto& access = _collectionAccessMap[vertexColl->name()];
            if (!access) {
              access = std::make_unique<CollectionAccess>();
              access->collectionName = vertexColl->name();
            }
            access->requiresAllAttributesRead = true;
          }
        }
      }
      break;
    }

    case ExecutionNode::SHORTEST_PATH: {
      auto* pathNode = ExecutionNode::castTo<ShortestPathNode*>(node);

      Projections const& edgeProjs = pathNode->options()->getEdgeProjections();
      for (auto* edgeColl : pathNode->edgeColls()) {
        auto& access = _collectionAccessMap[edgeColl->name()];
        if (!access) {
          access = std::make_unique<CollectionAccess>();
          access->collectionName = edgeColl->name();
        }
        if (!edgeProjs.empty()) {
          // Projections should include _from and _to if they exist
          for (auto const& proj : edgeProjs.projections()) {
            if (!proj.path.empty()) {
              access->readAttributes.insert(proj.path);
            }
          }
        } else {
          // No projections: full edge documents are read
          access->requiresAllAttributesRead = true;
        }
      }

      if (pathNode->options()->produceVertices()) {
        Projections const& vertexProjs =
            pathNode->options()->getVertexProjections();
        if (!vertexProjs.empty()) {
          for (auto* vertexColl : pathNode->vertexColls()) {
            auto& access = _collectionAccessMap[vertexColl->name()];
            if (!access) {
              access = std::make_unique<CollectionAccess>();
              access->collectionName = vertexColl->name();
            }
            for (auto const& proj : vertexProjs.projections()) {
              if (!proj.path.empty()) {
                access->readAttributes.insert(proj.path);
              }
            }
          }
        } else {
          for (auto* vertexColl : pathNode->vertexColls()) {
            auto& access = _collectionAccessMap[vertexColl->name()];
            if (!access) {
              access = std::make_unique<CollectionAccess>();
              access->collectionName = vertexColl->name();
            }
            access->requiresAllAttributesRead = true;
          }
        }
      }
      break;
    }

    case ExecutionNode::ENUMERATE_PATHS: {
      auto* pathNode = ExecutionNode::castTo<EnumeratePathsNode*>(node);

      Projections const& edgeProjs = pathNode->options()->getEdgeProjections();
      for (auto* edgeColl : pathNode->edgeColls()) {
        auto& access = _collectionAccessMap[edgeColl->name()];
        if (!access) {
          access = std::make_unique<CollectionAccess>();
          access->collectionName = edgeColl->name();
        }
        if (!edgeProjs.empty()) {
          // Projections should include _from and _to if they exist
          for (auto const& proj : edgeProjs.projections()) {
            if (!proj.path.empty()) {
              access->readAttributes.insert(proj.path);
            }
          }
        } else {
          // No projections: full edge documents are read
          access->requiresAllAttributesRead = true;
        }
      }

      if (pathNode->options()->produceVertices()) {
        Projections const& vertexProjs =
            pathNode->options()->getVertexProjections();
        if (!vertexProjs.empty()) {
          for (auto* vertexColl : pathNode->vertexColls()) {
            auto& access = _collectionAccessMap[vertexColl->name()];
            if (!access) {
              access = std::make_unique<CollectionAccess>();
              access->collectionName = vertexColl->name();
            }
            for (auto const& proj : vertexProjs.projections()) {
              if (!proj.path.empty()) {
                access->readAttributes.insert(proj.path);
              }
            }
          }
        } else {
          for (auto* vertexColl : pathNode->vertexColls()) {
            auto& access = _collectionAccessMap[vertexColl->name()];
            if (!access) {
              access = std::make_unique<CollectionAccess>();
              access->collectionName = vertexColl->name();
            }
            access->requiresAllAttributesRead = true;
          }
        }
      }
      break;
    }

    case ExecutionNode::INSERT: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      access->requiresAllAttributesRead = true;
      access->requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::UPDATE: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      access->requiresAllAttributesRead = true;
      access->requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::REPLACE: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      auto& monitor = _plan->getAst()->query().resourceMonitor();
      access->readAttributes.insert(AttributeNamePath("_key", monitor));
      access->requiresAllAttributesRead = true;
      access->requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::UPSERT: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      access->requiresAllAttributesRead = true;
      access->requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::REMOVE: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      auto& monitor = _plan->getAst()->query().resourceMonitor();
      access->readAttributes.insert(AttributeNamePath("_key", monitor));
      access->requiresAllAttributesRead = true;
      access->requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::ENUMERATE_NEAR_VECTORS: {
      auto* vectorNode = ExecutionNode::castTo<EnumerateNearVectorNode*>(node);
      std::string collName = vectorNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      access->requiresAllAttributesRead = true;
      break;
    }

    case ExecutionNode::INDEX_COLLECT: {
      auto* collectNode = ExecutionNode::castTo<IndexCollectNode*>(node);
      std::string collName = collectNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      access->requiresAllAttributesRead = true;
      break;
    }

    case ExecutionNode::JOIN: {
      auto* joinNode = ExecutionNode::castTo<JoinNode*>(node);
      for (auto const& idxInfo : joinNode->getIndexInfos()) {
        std::string collName = idxInfo.collection->name();

        auto& access = _collectionAccessMap[collName];
        if (!access) {
          access = std::make_unique<CollectionAccess>();
          access->collectionName = collName;
        }

        access->outVariable = idxInfo.outVariable;

        Projections const& projs = idxInfo.projections;
        if (!projs.empty()) {
          for (auto const& proj : projs.projections()) {
            if (!proj.path.empty()) {
              access->readAttributes.insert(proj.path);
            }
          }
        }

        Projections const& filterProjections = idxInfo.filterProjections;
        if (!filterProjections.empty()) {
          for (auto const& proj : filterProjections.projections()) {
            if (!proj.path.empty()) {
              access->readAttributes.insert(proj.path);
            }
          }
        }

        if (projs.empty() && joinNode->isVarUsedLater(idxInfo.outVariable)) {
          access->requiresAllAttributesRead = true;
        }

        Condition* cond = idxInfo.condition.get();
        if (cond && !cond->isEmpty()) {
          containers::FlatHashSet<aql::AttributeNamePath> attributes;
          if (Ast::getReferencedAttributesRecursive(
                  cond->root(), access->outVariable, "", attributes,
                  _plan->getAst()->query().resourceMonitor())) {
            for (auto const& attr : attributes) {
              if (!attr.empty()) {
                access->readAttributes.insert(attr);
              }
            }
          } else {
            access->requiresAllAttributesRead = true;
          }
        }
      }
      break;
    }

    case ExecutionNode::REMOTE_SINGLE: {
      TRI_IF_FAILURE("AttributeDetector::REMOTE_SINGLE") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      auto* remoteNode =
          ExecutionNode::castTo<SingleRemoteOperationNode*>(node);
      std::string collName = remoteNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }

      ExecutionNode::NodeType mode = remoteNode->mode();
      if (mode == ExecutionNode::INDEX) {
        access->requiresAllAttributesRead = true;
        access->requiresAllAttributesWrite = false;
      } else {
        access->requiresAllAttributesRead = true;
        access->requiresAllAttributesWrite = true;
      }
      break;
    }

    case ExecutionNode::REMOTE_MULTIPLE: {
      TRI_IF_FAILURE("AttributeDetector::REMOTE_MULTIPLE") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      auto* remoteNode =
          ExecutionNode::castTo<MultipleRemoteModificationNode*>(node);
      std::string collName = remoteNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      if (!access) {
        access = std::make_unique<CollectionAccess>();
        access->collectionName = collName;
      }
      access->requiresAllAttributesRead = true;
      access->requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::MATERIALIZE: {
      // Late materialization moves projections from IndexNode to MaterializeNode
      auto* matNode =
          dynamic_cast<materialize::MaterializeRocksDBNode*>(node);
      if (matNode != nullptr) {
        std::string collName = matNode->collection()->name();

        auto& access = _collectionAccessMap[collName];
        if (!access) {
          access = std::make_unique<CollectionAccess>();
          access->collectionName = collName;
        }

        access->outVariable = &matNode->outVariable();

        Projections const& projs = matNode->projections();
        if (!projs.empty()) {
          for (auto const& proj : projs.projections()) {
            if (!proj.path.empty()) {
              access->readAttributes.insert(proj.path);
            }
          }
        }

        if (projs.empty() && matNode->isVarUsedLater(&matNode->outVariable())) {
          access->requiresAllAttributesRead = true;
        }
      }
      break;
    }

    default:
      break;
  }

  return false;
}

void AttributeDetector::after(ExecutionNode*) {}

bool AttributeDetector::enterSubquery(ExecutionNode*, ExecutionNode*) {
  return true;
}

void AttributeDetector::toVelocyPack(velocypack::Builder& builder) const {
  builder.openArray();
  for (auto const& access : _collectionAccesses) {
    velocypack::serialize(builder, access);
  }
  builder.close();
}

}  // namespace arangodb::aql
