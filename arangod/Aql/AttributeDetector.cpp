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
#include "Aql/Collection.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

AttributeDetector::AttributeDetector(Ast* ast) : _ast(ast) {}

void AttributeDetector::detect() {
  TRI_ASSERT(_ast != nullptr);

  AstNode* root = const_cast<AstNode*>(_ast->root());
  if (root != nullptr) {
    root->walk(*this);
  }

  _collectionAccesses.clear();
  _collectionAccesses.reserve(_collectionAccessMap.size());
  for (auto const& [name, access] : _collectionAccessMap) {
    _collectionAccesses.push_back(access);
  }
}

bool AttributeDetector::before(AstNode* node) {
  if (node == nullptr) {
    return false;
  }

  switch (node->type) {
    case NODE_TYPE_FOR: {
      if (node->numMembers() >= 2) {
        AstNode* varNode = node->getMember(0);
        AstNode* exprNode = node->getMember(1);

        if (varNode->type == NODE_TYPE_VARIABLE && exprNode != nullptr) {
          auto var = static_cast<Variable const*>(varNode->getData());

          if (exprNode->type == NODE_TYPE_COLLECTION) {
            std::string collName = exprNode->getString();
            _variableToCollection[var] = collName;

            if (_collectionAccessMap.find(collName) ==
                _collectionAccessMap.end()) {
              CollectionAccess access;
              access.collectionName = collName;
              _collectionAccessMap[collName] = access;
            }
          } else if (exprNode->type == NODE_TYPE_VIEW) {
            processViewAccess(exprNode, var);
          }
        }
      }
      break;
    }

    case NODE_TYPE_TRAVERSAL:
    case NODE_TYPE_SHORTEST_PATH:
    case NODE_TYPE_ENUMERATE_PATHS: {
      processGraphTraversal(node);
      break;
    }

    case NODE_TYPE_ATTRIBUTE_ACCESS: {
      processAttributeAccess(node, _inWriteContext);
      return true;
    }

    case NODE_TYPE_REFERENCE: {
      auto var = static_cast<Variable const*>(node->getData());
      auto it = _variableToCollection.find(var);
      if (it != _variableToCollection.end()) {
        markRequiresAllAttributes(var, _inWriteContext);
      }
      break;
    }

    case NODE_TYPE_INSERT:
    case NODE_TYPE_REMOVE:
    case NODE_TYPE_UPDATE:
    case NODE_TYPE_REPLACE:
    case NODE_TYPE_UPSERT: {
      processModificationNode(node);
      return false;  // Process modification nodes specially
    }

    case NODE_TYPE_FCALL: {
      if (_ast->functionsMayAccessDocuments()) {
        for (auto& pair : _collectionAccessMap) {
          pair.second.requiresAllAttributesRead = true;
        }
      }
      break;
    }

    case NODE_TYPE_EXPANSION: {
      AstNode* iterator = node->getMember(0);
      if (iterator != nullptr) {
        if (iterator->type == NODE_TYPE_ITERATOR) {
          AstNode* varNode = iterator->getMember(0);
          if (varNode != nullptr && varNode->type == NODE_TYPE_VARIABLE) {
            auto var = static_cast<Variable const*>(varNode->getData());
            markRequiresAllAttributes(var, false);
          }
        }
      }
      break;
    }

    case NODE_TYPE_ARRAY:
    case NODE_TYPE_OBJECT: {
      if (!node->isConstant()) {
        for (size_t i = 0; i < node->numMembers(); ++i) {
          AstNode* member = node->getMemberUnchecked(i);
          if (member->type == NODE_TYPE_OBJECT_ELEMENT) {
            AstNode* keyNode = member->getMember(0);
            if (!keyNode->isConstant()) {
              for (auto& pair : _collectionAccessMap) {
                pair.second.requiresAllAttributesRead = true;
              }
            }
          }
        }
      }
      break;
    }

    case NODE_TYPE_INDEXED_ACCESS: {
      AstNode* indexNode = node->getMember(1);
      if (!indexNode->isConstant()) {
        AstNode* objectNode = node->getMember(0);
        if (objectNode->type == NODE_TYPE_REFERENCE) {
          auto var = static_cast<Variable const*>(objectNode->getData());
          markRequiresAllAttributes(var, _inWriteContext);
        }
      } else {
        if (indexNode->isStringValue()) {
          processAttributeAccess(node, _inWriteContext);
        }
      }
      break;
    }

    default:
      break;
  }

  return true;
}

void AttributeDetector::after(AstNode* node) {
  if (node != nullptr) {
    switch (node->type) {
      case NODE_TYPE_INSERT:
      case NODE_TYPE_REMOVE:
      case NODE_TYPE_UPDATE:
      case NODE_TYPE_REPLACE:
      case NODE_TYPE_UPSERT:
        _inWriteContext = false;
        _modificationVariable = nullptr;
        break;
      default:
        break;
    }
  }
}

bool AttributeDetector::enterSubquery(AstNode*, AstNode*) {
  return true;
}

void AttributeDetector::processAttributeAccess(AstNode const* node,
                                               bool isWrite) {
  TRI_ASSERT(node != nullptr);

  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attrAccess;

  if (node->isAttributeAccessForVariable(attrAccess, true)) {
    Variable const* var = attrAccess.first;
    if (var != nullptr && !attrAccess.second.empty()) {
      std::string topLevelAttr = attrAccess.second[0].name;

      if (attrAccess.second.size() > 1) {
        std::string fullPath;
        for (size_t i = 0; i < attrAccess.second.size(); ++i) {
          if (i > 0) {
            fullPath += ".";
          }
          fullPath += attrAccess.second[i].name;
        }
        trackAttributeAccess(var, fullPath, isWrite);
      } else {
        trackAttributeAccess(var, topLevelAttr, isWrite);
      }
    }
  }
}

void AttributeDetector::processModificationNode(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  _inWriteContext = true;

  switch (node->type) {
    case NODE_TYPE_INSERT: {
      if (node->numMembers() >= 2) {
        AstNode* exprNode = node->getMember(0);
        AstNode* collNode = node->getMember(1);

        if (collNode->type == NODE_TYPE_COLLECTION) {
          std::string collName = collNode->getString();

          if (exprNode->type == NODE_TYPE_OBJECT && exprNode->isConstant()) {
            std::vector<std::string> attrs;
            extractAttributesFromObject(exprNode, attrs);

            auto& access = _collectionAccessMap[collName];
            access.collectionName = collName;
            for (auto const& attr : attrs) {
              access.writeAttributes.emplace(attr);
            }
          } else {
            auto& access = _collectionAccessMap[collName];
            access.collectionName = collName;
            access.requiresAllAttributesWrite = true;
          }
        }
      }
      break;
    }

    case NODE_TYPE_UPDATE:
    case NODE_TYPE_REPLACE: {
      if (node->numMembers() >= 3) {
        AstNode* keyExpr = node->getMember(0);
        AstNode* withDoc = node->getMember(1);
        AstNode* collNode = node->getMember(2);

        if (collNode->type == NODE_TYPE_COLLECTION) {
          std::string collName = collNode->getString();
          auto& access = _collectionAccessMap[collName];
          access.collectionName = collName;

          if (keyExpr->type != NODE_TYPE_VALUE &&
              !keyExpr->isConstant()) {
            access.requiresAllAttributesRead = true;
          }

          if (withDoc->type == NODE_TYPE_OBJECT && withDoc->isConstant()) {
            std::vector<std::string> attrs;
            extractAttributesFromObject(withDoc, attrs);
            for (auto const& attr : attrs) {
              access.writeAttributes.emplace(attr);
              access.readAttributes.emplace(attr);
            }
          } else {
            access.requiresAllAttributesWrite = true;
            access.requiresAllAttributesRead = true;
          }
        }
      }
      break;
    }

    case NODE_TYPE_REMOVE: {
      if (node->numMembers() >= 2) {
        AstNode* collNode = node->getMember(1);
        if (collNode->type == NODE_TYPE_COLLECTION) {
          std::string collName = collNode->getString();
          auto& access = _collectionAccessMap[collName];
          access.collectionName = collName;
          access.requiresAllAttributesWrite = true;
          access.requiresAllAttributesRead = true;
        }
      }
      break;
    }

    case NODE_TYPE_UPSERT: {
      if (node->numMembers() >= 4) {
        AstNode* collNode = node->getMember(3);
        if (collNode->type == NODE_TYPE_COLLECTION) {
          std::string collName = collNode->getString();
          auto& access = _collectionAccessMap[collName];
          access.collectionName = collName;
          access.requiresAllAttributesRead = true;
          access.requiresAllAttributesWrite = true;
        }
      }
      break;
    }

    default:
      break;
  }
}

void AttributeDetector::trackAttributeAccess(Variable const* var,
                                              std::string_view attribute,
                                              bool isWrite) {
  auto it = _variableToCollection.find(var);
  if (it != _variableToCollection.end()) {
    std::string const& collName = it->second;
    auto& access = _collectionAccessMap[collName];
    access.collectionName = collName;

    if (isWrite) {
      access.writeAttributes.emplace(attribute);
      // Write implies read
      access.readAttributes.emplace(attribute);
    } else {
      access.readAttributes.emplace(attribute);
    }
  }
}

void AttributeDetector::markRequiresAllAttributes(Variable const* var,
                                                   bool isWrite) {
  auto it = _variableToCollection.find(var);
  if (it != _variableToCollection.end()) {
    std::string const& collName = it->second;
    auto& access = _collectionAccessMap[collName];
    access.collectionName = collName;

    if (isWrite) {
      access.requiresAllAttributesWrite = true;
      access.requiresAllAttributesRead = true;
    } else {
      access.requiresAllAttributesRead = true;
    }
  }
}

void AttributeDetector::extractAttributesFromObject(
    AstNode const* node, std::vector<std::string>& attributes) {
  if (node->type != NODE_TYPE_OBJECT) {
    return;
  }

  for (size_t i = 0; i < node->numMembers(); ++i) {
    AstNode* member = node->getMemberUnchecked(i);
    if (member->type == NODE_TYPE_OBJECT_ELEMENT) {
      AstNode* keyNode = member->getMember(0);
      if (keyNode->isStringValue()) {
        attributes.emplace_back(keyNode->getString());
      }
    }
  }
}

bool AttributeDetector::requiresWildcardAccess() const {
  for (auto const& access : _collectionAccesses) {
    if (access.requiresAllAttributesRead ||
        access.requiresAllAttributesWrite) {
      return true;
    }
  }
  return false;
}

void AttributeDetector::processViewAccess(AstNode const* node,
                                          Variable const* var) {
  if (node->type == NODE_TYPE_VIEW) {
    std::string viewName = node->getString();
    
    LOG_TOPIC("abac2", DEBUG, Logger::QUERIES)
        << "View access detected: " << viewName 
        << " - requires resolution of linked collections";

    _variableToCollection[var] = viewName + "_VIEW_ACCESS";
    
    auto& access = _collectionAccessMap[viewName + "_VIEW_ACCESS"];
    access.collectionName = viewName + " (View)";
    access.requiresAllAttributesRead = true;
  }
}

void AttributeDetector::processGraphTraversal(AstNode const* node) {
  

  LOG_TOPIC("abac3", DEBUG, Logger::QUERIES)
      << "Graph traversal detected - requires analysis of graph collections";

}

void AttributeDetector::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::serialize(builder, _collectionAccesses);
}


