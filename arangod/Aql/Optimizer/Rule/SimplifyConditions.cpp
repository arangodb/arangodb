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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "SimplifyConditions.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Basics/NumberUtils.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {
using EN = ExecutionNode;

void simplifyConditionsRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                            OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  auto p = plan.get();
  bool changed = false;

  auto visitor = [&changed, p](AstNode* node) {
    again:
      if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        auto const* accessed = node->getMemberUnchecked(0);

        if (accessed->type == NODE_TYPE_REFERENCE) {
          ast::ReferenceNode ref(accessed);
          Variable const* v = ref.getVariable();
          TRI_ASSERT(v != nullptr);

          auto setter = p->getVarSetBy(v->id);

          if (setter == nullptr || setter->getType() != EN::CALCULATION) {
            return node;
          }

          accessed = ExecutionNode::castTo<CalculationNode*>(setter)
                         ->expression()
                         ->node();
          if (accessed == nullptr) {
            return node;
          }
        }

        TRI_ASSERT(accessed != nullptr);

        if (accessed->type == NODE_TYPE_OBJECT) {
          std::string_view attributeName(node->getStringView());
          bool isDynamic = false;
          size_t const n = accessed->numMembers();
          for (size_t i = 0; i < n; ++i) {
            auto member = accessed->getMemberUnchecked(i);

            if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
                member->getStringView() == attributeName) {
              ast::ObjectElementNode objElem(member);
              AstNode* next = objElem.getValue();
              if (!next->isDeterministic()) {
                return node;
              }
              node = next;
              goto again;
            } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
              isDynamic = true;
            }
          }

          if (!isDynamic) {
            changed = true;
            return p->getAst()->createNodeValueNull();
          }
        }
      } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
        ast::IndexedAccessNode indexAccess(node);
        auto const* accessed = indexAccess.getObject();

        if (accessed->type == NODE_TYPE_REFERENCE) {
          ast::ReferenceNode ref(accessed);
          Variable const* v = ref.getVariable();
          TRI_ASSERT(v != nullptr);

          auto setter = p->getVarSetBy(v->id);

          if (setter == nullptr || setter->getType() != EN::CALCULATION) {
            return node;
          }

          accessed = ExecutionNode::castTo<CalculationNode*>(setter)
                         ->expression()
                         ->node();
          if (accessed == nullptr) {
            return node;
          }
        }

        auto indexValue = indexAccess.getIndex();

        if (!indexValue->isConstant() ||
            !(indexValue->isStringValue() || indexValue->isNumericValue())) {
          return node;
        }

        if (accessed->type == NODE_TYPE_OBJECT) {
          std::string_view attributeName;
          std::string indexString;

          if (indexValue->isStringValue()) {
            attributeName = indexValue->getStringView();
          } else {
            TRI_ASSERT(indexValue->isNumericValue());
            indexString = std::to_string(indexValue->getIntValue());
            attributeName = std::string_view(indexString);
          }

          bool isDynamic = false;
          size_t const n = accessed->numMembers();
          for (size_t i = 0; i < n; ++i) {
            auto member = accessed->getMemberUnchecked(i);

            if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
                member->getStringView() == attributeName) {
              ast::ObjectElementNode objElem2(member);
              AstNode* next = objElem2.getValue();
              if (!next->isDeterministic()) {
                return node;
              }
              node = next;
              goto again;
            } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
              isDynamic = true;
            }
          }

          if (!isDynamic) {
            changed = true;
            return p->getAst()->createNodeValueNull();
          }
        } else if (accessed->type == NODE_TYPE_ARRAY) {
          int64_t position;
          if (indexValue->isStringValue()) {
            bool valid;
            position = NumberUtils::atoi<int64_t>(
                indexValue->getStringValue(),
                indexValue->getStringValue() + indexValue->getStringLength(),
                valid);
            if (!valid) {
              changed = true;
              return p->getAst()->createNodeValueNull();
            }
          } else {
            TRI_ASSERT(indexValue->isNumericValue());
            position = indexValue->getIntValue();
          }
          int64_t const n = accessed->numMembers();
          if (position < 0) {
            position = n + position;
          }
          if (position >= 0 && position < n) {
            ast::ArrayNode arr(accessed);
            AstNode* next = arr.getElements()[static_cast<size_t>(position)];
            if (!next->isDeterministic()) {
              return node;
            }
            node = next;
            goto again;
          }

          changed = true;
          return p->getAst()->createNodeValueNull();
        }
      }

      return node;
  };

  bool modified = false;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic() ||
        nn->outVariable()->type() == Variable::Type::Const) {
      continue;
    }

    AstNode* root = nn->expression()->nodeForModification();

    if (root != nullptr) {
      changed = false;
      AstNode* simplified = plan->getAst()->traverseAndModify(root, visitor);
      if (simplified != root || changed) {
        nn->expression()->replaceNode(simplified);
        nn->expression()->invalidateAfterReplacements();
        modified = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
}  // namespace arangodb::aql
