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

#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionPlan.h"
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

bool AttributeDetector::before(ExecutionNode* node) {
  if (node == nullptr) {
    return false;
  }

  auto nodeType = node->getType();

  switch (nodeType) {
    case ExecutionNode::ENUMERATE_COLLECTION: {
      auto* enumNode = ExecutionNode::castTo<EnumerateCollectionNode*>(node);
      std::string collName = enumNode->collection()->name();
      
      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;
      
      if (auto const& projections = enumNode->projections(); !projections.empty()) {
        for (auto const& proj : projections.projections()) {
          auto const& path = proj.path.get();
          if (!path.empty()) {
            access.readAttributes.insert(path[0]);
          }
        }
      } else {
        access.requiresAllAttributesRead = true;
      }
      break;
    }

    case ExecutionNode::INDEX: {
      auto* idxNode = ExecutionNode::castTo<IndexNode*>(node);
      std::string collName = idxNode->collection()->name();
      
      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;
      
      if (auto const& projections = idxNode->projections(); !projections.empty()) {
        for (auto const& proj : projections.projections()) {
          auto const& path = proj.path.get();
          if (!path.empty()) {
            access.readAttributes.insert(path[0]);
          }
        }
      } else {
        access.requiresAllAttributesRead = true;
      }
      break;
    }

    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::UPSERT: {
      auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
      std::string collName = modNode->collection()->name();
      
      auto& access = _collectionAccessMap[collName];
      access.collectionName = collName;
      access.requiresAllAttributesWrite = true;
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
