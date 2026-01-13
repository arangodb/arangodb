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
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "AttributeDetector.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Projections.h"
#include "Aql/Variable.h"
#include "Inspection/VPack.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::aql;

AttributeDetector::AttributeDetector(ExecutionPlan* plan) : _plan(plan) {}

void AttributeDetector::detect() {
  TRI_ASSERT(_plan != nullptr);

  if (_plan->root() != nullptr) {
    _plan->root()->walk(*this);
  }

  _collectionAccesses.clear();
  _collectionAccesses.reserve(_collectionAccessMap.size());
  for (auto const& [name, access] : _collectionAccessMap) {
    _collectionAccesses.push_back(access);
  }
}

void AttributeDetector::extractAttributesFromAstNode(AstNode const* node,
                                                     Variable const* var,
                                                     CollectionAccess& access) {
  if (node == nullptr) {
    return;
  }

  switch (node->type) {
    case NODE_TYPE_ATTRIBUTE_ACCESS: {
      AstNode const* accessed = node->getMember(0);
      if (accessed->type == NODE_TYPE_REFERENCE) {
        auto* refVar = static_cast<Variable const*>(accessed->getData());
        if (refVar == var) {
          access.readAttributes.insert(std::string(node->getStringView()));
        }
      } else {
        // Nested attribute access
        extractAttributesFromAstNode(accessed, var, access);
      }
      break;
    }

    case NODE_TYPE_REFERENCE: {
      auto* refVar = static_cast<Variable const*>(node->getData());
      if (refVar == var) {
        access.requiresAllAttributesRead = true;
      }
      break;
    }

    case NODE_TYPE_VALUE:
      break;

    default: {
      size_t const n = node->numMembers();
      for (size_t i = 0; i < n; ++i) {
        extractAttributesFromAstNode(node->getMember(i), var, access);
      }
      break;
    }
  }
}

bool AttributeDetector::before(ExecutionNode* node) {
  if (node == nullptr) {
    return false;
  }

  auto nodeType = node->getType();

  switch (nodeType) {
    case ExecutionNode::ENUMERATE_COLLECTION: {
      auto* enumNode = ExecutionNode::castTo<EnumerateCollectionNode*>(node);
      std::string collName = enumNode->collection()->name();
      Variable const* outVar = enumNode->outVariable();

      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;

      // Check projections first
      if (auto const& projections = enumNode->projections();
          !projections.empty()) {
        for (auto const& proj : projections.projections()) {
          auto const& path = proj.path.get();
          if (!path.empty()) {
            access.readAttributes.insert(path[0]);
          }
        }
      } else {
        access.requiresAllAttributesRead = true;
      }

      // Store variable mapping for later calculation node analysis
      _variableToCollection[outVar] = collName;
      break;
    }

    case ExecutionNode::INDEX: {
      auto* idxNode = ExecutionNode::castTo<IndexNode*>(node);
      std::string collName = idxNode->collection()->name();
      Variable const* outVar = idxNode->outVariable();

      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;

      if (auto const& projections = idxNode->projections();
          !projections.empty()) {
        for (auto const& proj : projections.projections()) {
          auto const& path = proj.path.get();
          if (!path.empty()) {
            access.readAttributes.insert(path[0]);
          }
        }
      } else {
        access.requiresAllAttributesRead = true;
      }

      _variableToCollection[outVar] = collName;
      break;
    }

    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      // TODO: Implement IResearch view tracking
      break;
    }

    case ExecutionNode::TRAVERSAL: {
      auto* travNode = ExecutionNode::castTo<TraversalNode*>(node);

      for (auto* edgeColl : travNode->edgeColls()) {
        auto& access = _collectionAccessMap[edgeColl->name()];
        access.collectionName = edgeColl->name();
        access.requiresAllAttributesRead = true;
      }

      for (auto* vertexColl : travNode->vertexColls()) {
        auto& access = _collectionAccessMap[vertexColl->name()];
        access.collectionName = vertexColl->name();
        access.requiresAllAttributesRead = true;
      }
      break;
    }

    case ExecutionNode::SHORTEST_PATH: {
      auto* pathNode = ExecutionNode::castTo<ShortestPathNode*>(node);

      for (auto* edgeColl : pathNode->edgeColls()) {
        auto& access = _collectionAccessMap[edgeColl->name()];
        access.collectionName = edgeColl->name();
        access.requiresAllAttributesRead = true;
      }

      for (auto* vertexColl : pathNode->vertexColls()) {
        auto& access = _collectionAccessMap[vertexColl->name()];
        access.collectionName = vertexColl->name();
        access.requiresAllAttributesRead = true;
      }
      break;
    }

    case ExecutionNode::ENUMERATE_PATHS: {
      auto* pathNode = ExecutionNode::castTo<EnumeratePathsNode*>(node);

      for (auto* edgeColl : pathNode->edgeColls()) {
        auto& access = _collectionAccessMap[edgeColl->name()];
        access.collectionName = edgeColl->name();
        access.requiresAllAttributesRead = true;
      }

      for (auto* vertexColl : pathNode->vertexColls()) {
        auto& access = _collectionAccessMap[vertexColl->name()];
        access.collectionName = vertexColl->name();
        access.requiresAllAttributesRead = true;
      }
      break;
    }

    case ExecutionNode::CALCULATION: {
      auto* calcNode = ExecutionNode::castTo<CalculationNode*>(node);
      Expression* expr = calcNode->expression();

      if (expr != nullptr && expr->node() != nullptr) {
        AstNode const* astNode = expr->node();

        VarSet usedVars;
        calcNode->getVariablesUsedHere(usedVars);

        for (auto* var : usedVars) {
          auto it = _variableToCollection.find(var);
          if (it != _variableToCollection.end()) {
            auto& access = _collectionAccessMap[it->second];
            extractAttributesFromAstNode(astNode, var, access);
          }
        }
      }
      break;
    }

    case ExecutionNode::INSERT: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;
      access.requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPSERT: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;
      access.requiresAllAttributesRead = true;
      access.requiresAllAttributesWrite = true;
      break;
    }

    case ExecutionNode::REMOVE: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();

      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;
      access.requiresAllAttributesRead = true;
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
  velocypack::serialize(builder, _collectionAccesses);
}
