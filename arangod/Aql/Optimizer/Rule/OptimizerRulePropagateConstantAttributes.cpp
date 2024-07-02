////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Cluster/ServerState.h"
#include "Containers/ImmutableMap.h"
#include "Logger/LogMacros.h"
#include "VocBase/LogicalCollection.h"

#include <boost/container_hash/hash.hpp>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(false)

namespace {

struct Replacement {
  std::uint64_t limitCount;
  AstNode* ast;
};

struct VarAttributeAccess {
  Variable const* var;
  containers::SmallVector<std::string_view, 8> path;

  friend bool operator==(VarAttributeAccess const&,
                         VarAttributeAccess const&) noexcept = default;

  friend std::size_t hash_value(VarAttributeAccess const& key) {
    std::size_t seed = 0;
    boost::hash_combine(seed, key.var);
    boost::hash_range(seed, key.path.begin(), key.path.end());
    return seed;
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  VarAttributeAccess const& x) {
    os << x.var->name << " [";
    std::copy(x.path.begin(), x.path.end(),
              std::ostream_iterator<std::string_view>(os, ", "));
    return os << "]";
  }
};

using VarReplacementMap =
    containers::ImmutableMap<VarAttributeAccess, Replacement,
                             boost::hash<VarAttributeAccess>>;
using VarReplacementTransientMap =
    containers::ImmutableMapTransient<VarAttributeAccess, Replacement,
                                      boost::hash<VarAttributeAccess>>;

/// @brief extract an attribute and its variable from an attribute access
/// (e.g. `a.b.c` will return variable `a` and attribute name `b.c.`.
bool getAttribute(AstNode const* attribute,
                  VarAttributeAccess& attributeAccess) {
  TRI_ASSERT(attribute != nullptr);
  if (attribute->type == NODE_TYPE_REFERENCE) {
    attributeAccess.var = static_cast<Variable const*>(attribute->getData());
    return true;
  } else if (attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    getAttribute(attribute->getMember(0), attributeAccess);
    attributeAccess.path.emplace_back(attribute->getStringView());
    return true;
  }
  return false;
}

/// @brief inspect the constant value assigned to an attribute
/// the attribute value will be stored so it can be inserted for the attribute
/// later
void inspectConstantAttribute(AstNode const* attribute, AstNode* value,
                              std::size_t currentLimitCount,
                              VarReplacementTransientMap& replacements) {
  VarAttributeAccess access;
  LOG_RULE << "ATTR = " << attribute->toString();
  if (!getAttribute(attribute, access)) {
    return;
  }
  LOG_RULE << "VAR = " << access;
  auto it = replacements.find(access);

  if (it == nullptr) {
    replacements.set(access, Replacement{currentLimitCount, value});
    return;
  }
}

void collectConstantAttributes(AstNode const* node,
                               std::size_t currentLimitCount,
                               VarReplacementTransientMap& replacements) {
  if (node == nullptr) {
    return;
  }

  LOG_RULE << node->toString();

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
    auto lhs = node->getMember(0);
    auto rhs = node->getMember(1);

    collectConstantAttributes(lhs, currentLimitCount, replacements);
    collectConstantAttributes(rhs, currentLimitCount, replacements);
  } else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
    auto lhs = node->getMember(0);
    auto rhs = node->getMember(1);
    LOG_RULE << "BINARY EQ " << lhs->toString() << " == " << rhs->toString();

    LOG_RULE << "LHS = " << lhs->getTypeString();
    LOG_RULE << "RHS = " << rhs->getTypeString();

    if (lhs->isConstant() && (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                              rhs->type == NODE_TYPE_REFERENCE)) {
      LOG_RULE << "LHS IS CONST";
      inspectConstantAttribute(rhs, lhs, currentLimitCount, replacements);
    } else if (rhs->isConstant() && (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                                     lhs->type == NODE_TYPE_REFERENCE)) {
      LOG_RULE << "RHS IS CONST";
      inspectConstantAttribute(lhs, rhs, currentLimitCount, replacements);
    }
  }
}

AstNode* getConstant(VarAttributeAccess const& access,
                     std::size_t currentLimitCount,
                     VarReplacementTransientMap const& replacements) {
  auto it = replacements.find(access);

  if (it == nullptr) {
    return nullptr;
  }

  // Check is the constant was found at this limit count, i.e. check
  // if there is a limit node between this node and witness.
  if (it->limitCount > currentLimitCount) {
    return nullptr;
  }

  return it->ast;
}

/// @brief patches an AstNode by inserting a constant value into it
bool insertConstantAttribute(ExecutionPlan& plan, AstNode* parentNode,
                             size_t accessIndex, std::size_t currentLimitCount,
                             VarReplacementTransientMap const& replacements) {
  VarAttributeAccess access;

  AstNode* member = parentNode->getMember(accessIndex);
  LOG_RULE << "CONSIDER: " << member->toString();
  if (!getAttribute(member, access)) {
    return false;
  }
  LOG_RULE << "REPLACE " << access;

  auto constantValue = getConstant(access, currentLimitCount, replacements);

  if (constantValue != nullptr) {
    // first check if we would optimize away a join condition that uses a
    // smartJoinAttribute... we must not do that, because that would otherwise
    // disable SmartJoin functionality
    if (arangodb::ServerState::instance()->isCoordinator() &&
        parentNode->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      AstNode const* current = parentNode->getMember(accessIndex == 0 ? 1 : 0);
      if (current->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        AstNode const* nameAttribute = current;
        current = current->getMember(0);
        if (current->type == NODE_TYPE_REFERENCE) {
          auto setter = plan.getVarSetBy(
              static_cast<Variable const*>(current->getData())->id);
          if (setter != nullptr &&
              (setter->getType() == EN::ENUMERATE_COLLECTION ||
               setter->getType() == EN::INDEX)) {
            auto collection = utils::getCollection(setter);
            if (collection != nullptr) {
              auto logical = collection->getCollection();
              if (logical->hasSmartJoinAttribute() &&
                  logical->smartJoinAttribute() ==
                      nameAttribute->getStringView()) {
                // don't remove a SmartJoin attribute access!
                return false;
              } else {
                std::vector<std::string> shardKeys =
                    collection->shardKeys(true);
                if (std::find(shardKeys.begin(), shardKeys.end(),
                              nameAttribute->getString()) != shardKeys.end()) {
                  // don't remove equality lookups on shard keys, as this may
                  // prevent the restrict-to-single-shard rule from being
                  // applied later!
                  return false;
                }
              }
            }
          }
        }
      }
    }

    LOG_RULE << "CHANGE MEMBER";
    parentNode->changeMember(accessIndex, constantValue);
    return true;
  }

  return false;
}

/// @brief traverses an AST part recursively and patches it by inserting
/// constant values
bool insertConstantAttributes(ExecutionPlan& plan, AstNode* node,
                              std::size_t currentLimitCount,
                              VarReplacementTransientMap const& replacements) {
  if (node == nullptr) {
    return false;
  }

  bool modified = false;

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
    auto lhs = node->getMember(0);
    auto rhs = node->getMember(1);

    modified |=
        insertConstantAttributes(plan, lhs, currentLimitCount, replacements);
    modified |=
        insertConstantAttributes(plan, rhs, currentLimitCount, replacements);
  } else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
    auto lhs = node->getMember(0);
    auto rhs = node->getMember(1);

    if (!lhs->isConstant() && (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                               rhs->type == NODE_TYPE_REFERENCE)) {
      modified |= insertConstantAttribute(plan, node, 1, currentLimitCount,
                                          replacements);
    }
    if (!rhs->isConstant() && (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                               lhs->type == NODE_TYPE_REFERENCE)) {
      modified |= insertConstantAttribute(plan, node, 0, currentLimitCount,
                                          replacements);
    }
  }

  return modified;
}

bool processQuery(ExecutionPlan& plan, ExecutionNode* root,
                  VarReplacementMap replacements,
                  std::size_t currentLimitCount) {
  auto replacementsMutable = replacements.transient();

  bool modified = false;

  std::size_t localLimitCount = currentLimitCount;
  // collect
  for (auto* node = root->getSingleton(); node != nullptr;
       node = node->getFirstParent()) {
    LOG_RULE << "[" << node->id() << "] " << node->getTypeString();
    if (node->getType() == EN::LIMIT) {
      localLimitCount += 1;
    }
    if (node->getType() != EN::FILTER) {
      continue;
    }

    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    auto setter = plan.getVarSetBy(fn->inVariable()->id);
    if (setter != nullptr && setter->getType() == EN::CALCULATION) {
      auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
      auto expression = cn->expression();

      collectConstantAttributes(expression->node(), localLimitCount,
                                replacementsMutable);
    }
  }

  localLimitCount = currentLimitCount;
  // replace and enter subqueries
  for (auto* node = root->getSingleton(); node != nullptr;
       node = node->getFirstParent()) {
    LOG_RULE << "[" << node->id() << "] " << node->getTypeString();
    if (node->getType() == EN::LIMIT) {
      localLimitCount += 1;
    }
    if (node->getType() == EN::SUBQUERY) {
      auto* subquery = ExecutionNode::castTo<SubqueryNode*>(node);
      LOG_RULE << "ENTER SUBQUERY";
      modified |=
          processQuery(plan, subquery->getSubquery(),
                       replacementsMutable.persistent(), localLimitCount);
      LOG_RULE << "LEAVE SUBQUERY";
    }
    if (node->getType() != EN::FILTER) {
      continue;
    }

    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    auto setter = plan.getVarSetBy(fn->inVariable()->id);
    if (setter != nullptr && setter->getType() == EN::CALCULATION) {
      auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
      auto expression = cn->expression();

      modified |= insertConstantAttributes(
          plan, const_cast<AstNode*>(expression->node()), localLimitCount,
          replacementsMutable);
    }
  }

  return modified;
}

}  // namespace

/// @brief propagate constant attributes in FILTERs
void arangodb::aql::propagateConstantAttributesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = processQuery(*plan, plan->root(), {}, 0);
  opt->addPlan(std::move(plan), rule, modified);
}
