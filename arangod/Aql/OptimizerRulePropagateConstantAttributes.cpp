////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRules.h"
#include "Aql/Expression.h"
#include "Aql/Collection.h"
#include "Aql/Optimizer.h"
#include "VocBase/LogicalCollection.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "OptimizerUtils.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

namespace {
class PropagateConstantAttributesHelper {
 public:
  explicit PropagateConstantAttributesHelper(ExecutionPlan* plan)
      : _plan(plan), _modified(false) {}

  bool modified() const { return _modified; }

  /// @brief inspects a plan and propagates constant values in expressions
  void propagateConstants() {
    LOG_DEVEL << "PROPAGATE CONSTANTS";
    containers::SmallVector<ExecutionNode*, 8> nodes;
    _plan->findNodesOfType(nodes, EN::FILTER, true);

    for (auto* node = _plan->root()->getSingleton(); node != nullptr;
         node = node->getFirstParent()) {
      if (node->getType() != EN::FILTER) {
        continue;
      }

      auto fn = ExecutionNode::castTo<FilterNode const*>(node);
      auto setter = _plan->getVarSetBy(fn->inVariable()->id);
      if (setter != nullptr && setter->getType() == EN::CALCULATION) {
        auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
        auto expression = cn->expression();

        insertConstantAttributes(const_cast<AstNode*>(expression->node()));
        collectConstantAttributes(const_cast<AstNode*>(expression->node()));
      }
    }
  }

 private:
  AstNode const* getConstant(Variable const* variable,
                             std::string const& attribute) const {
    auto it = _constants.find(variable);

    if (it == _constants.end()) {
      return nullptr;
    }

    auto it2 = (*it).second.find(attribute);

    if (it2 == (*it).second.end()) {
      return nullptr;
    }

    return (*it2).second;
  }

  /// @brief inspects an expression (recursively) and notes constant attribute
  /// values so they can be propagated later
  void collectConstantAttributes(AstNode* node) {
    if (node == nullptr) {
      return;
    }

    LOG_DEVEL << node->toString();

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      collectConstantAttributes(lhs);
      collectConstantAttributes(rhs);
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      LOG_DEVEL << "BINARY EQ " << lhs->toString() << " == " << rhs->toString();

      LOG_DEVEL << "LHS = " << lhs->getTypeString();
      LOG_DEVEL << "RHS = " << rhs->getTypeString();

      if (lhs->isConstant() && (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                                rhs->type == NODE_TYPE_REFERENCE)) {
        LOG_DEVEL << "LHS IS CONST";
        inspectConstantAttribute(rhs, lhs);
      } else if (rhs->isConstant() &&
                 (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                  lhs->type == NODE_TYPE_REFERENCE)) {
        LOG_DEVEL << "RHS IS CONST";
        inspectConstantAttribute(lhs, rhs);
      }
    }
  }

  /// @brief traverses an AST part recursively and patches it by inserting
  /// constant values
  void insertConstantAttributes(AstNode* node) {
    if (node == nullptr) {
      return;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      insertConstantAttributes(lhs);
      insertConstantAttributes(rhs);
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (!lhs->isConstant() && (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                                 rhs->type == NODE_TYPE_REFERENCE)) {
        insertConstantAttribute(node, 1);
      }
      if (!rhs->isConstant() && (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                                 lhs->type == NODE_TYPE_REFERENCE)) {
        insertConstantAttribute(node, 0);
      }
    }
  }

  /// @brief extract an attribute and its variable from an attribute access
  /// (e.g. `a.b.c` will return variable `a` and attribute name `b.c.`.
  bool getAttribute(AstNode const* attribute, Variable const*& variable,
                    std::string& name) {
    TRI_ASSERT(attribute != nullptr);
    TRI_ASSERT(attribute->type == NODE_TYPE_REFERENCE ||
               attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS);
    TRI_ASSERT(name.empty());

    while (attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      name =
          basics::StringUtils::concatT(".", attribute->getStringView(), name);
      attribute = attribute->getMember(0);
    }

    if (attribute->type != NODE_TYPE_REFERENCE) {
      return false;
    }

    variable = static_cast<Variable const*>(attribute->getData());
    TRI_ASSERT(variable != nullptr);

    return true;
  }

  /// @brief inspect the constant value assigned to an attribute
  /// the attribute value will be stored so it can be inserted for the attribute
  /// later
  void inspectConstantAttribute(AstNode const* attribute,
                                AstNode const* value) {
    Variable const* variable = nullptr;
    std::string name;
    LOG_DEVEL << "ATTR = " << attribute->toString();
    if (!getAttribute(attribute, variable, name)) {
      return;
    }
    LOG_DEVEL << "VAR = " << variable->name << " NAME = " << name;
    auto it = _constants.find(variable);

    if (it == _constants.end()) {
      _constants.try_emplace(
          variable,
          std::unordered_map<std::string, AstNode const*>{{name, value}});
      return;
    }

    auto it2 = (*it).second.find(name);

    if (it2 == (*it).second.end()) {
      // first value for the attribute
      (*it).second.try_emplace(name, value);
    } else {
      auto previous = (*it2).second;

      if (previous == nullptr) {
        // we have multiple different values for the attribute. better not use
        // this attribute
        return;
      }

      if (!value->computeValue().binaryEquals(previous->computeValue())) {
        // different value found for an already tracked attribute. better not
        // use this attribute
        (*it2).second = nullptr;
      }
    }
  }

  /// @brief patches an AstNode by inserting a constant value into it
  void insertConstantAttribute(AstNode* parentNode, size_t accessIndex) {
    Variable const* variable = nullptr;
    std::string name;

    AstNode* member = parentNode->getMember(accessIndex);

    if (!getAttribute(member, variable, name)) {
      return;
    }
    LOG_DEVEL << "REPLACE " << variable->name << " NAME = " << name;

    auto constantValue = getConstant(variable, name);

    if (constantValue != nullptr) {
      // first check if we would optimize away a join condition that uses a
      // smartJoinAttribute... we must not do that, because that would otherwise
      // disable SmartJoin functionality
      if (arangodb::ServerState::instance()->isCoordinator() &&
          parentNode->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        AstNode const* current =
            parentNode->getMember(accessIndex == 0 ? 1 : 0);
        if (current->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          AstNode const* nameAttribute = current;
          current = current->getMember(0);
          if (current->type == NODE_TYPE_REFERENCE) {
            auto setter = _plan->getVarSetBy(
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
                  return;
                } else {
                  std::vector<std::string> shardKeys =
                      collection->shardKeys(true);
                  if (std::find(shardKeys.begin(), shardKeys.end(),
                                nameAttribute->getString()) !=
                      shardKeys.end()) {
                    // don't remove equality lookups on shard keys, as this may
                    // prevent the restrict-to-single-shard rule from being
                    // applied later!
                    return;
                  }
                }
              }
            }
          }
        }
      }

      LOG_DEVEL << "CHANGE MEMBER";
      parentNode->changeMember(accessIndex,
                               const_cast<AstNode*>(constantValue));
      _modified = true;
    }
  }

  ExecutionPlan* _plan;
  std::unordered_map<Variable const*,
                     std::unordered_map<std::string, AstNode const*>>
      _constants;
  bool _modified;
};
}  // namespace

/// @brief propagate constant attributes in FILTERs
void arangodb::aql::propagateConstantAttributesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  plan->show();
  PropagateConstantAttributesHelper helper(plan.get());
  helper.propagateConstants();

  opt->addPlan(std::move(plan), rule, helper.modified());
}
