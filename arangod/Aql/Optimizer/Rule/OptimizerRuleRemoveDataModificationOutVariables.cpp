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

#include "OptimizerRuleRemoveDataModificationOutVariables.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {
static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    removeDataModificationOutVariablesNodeTypes{
      arangodb::aql::ExecutionNode::REMOVE,
      arangodb::aql::ExecutionNode::INSERT,
      arangodb::aql::ExecutionNode::UPDATE,
      arangodb::aql::ExecutionNode::REPLACE,
      arangodb::aql::ExecutionNode::UPSERT};
} // namespace

struct VariableReplacer final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
public:
  explicit VariableReplacer(
      std::unordered_map<VariableId, Variable const*> const& replacements)
      : replacements(replacements) {}

  bool before(ExecutionNode* en) override final {
    en->replaceVariables(replacements);
    // always continue
    return false;
  }

private:
  std::unordered_map<VariableId, Variable const*> const& replacements;
};

/// @brief remove $OLD and $NEW variables from data-modification statements
/// if not required
void arangodb::aql::removeDataModificationOutVariablesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ::removeDataModificationOutVariablesNodeTypes,
                        true);

  for (auto const& n : nodes) {
    auto node = ExecutionNode::castTo<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    Variable const* old = node->getOutVariableOld();
    if (!n->isVarUsedLater(old)) {
      // "$OLD" is not used later
      node->clearOutVariableOld();
      modified = true;
    } else {
      switch (n->getType()) {
        case EN::UPDATE:
        case EN::REPLACE: {
          Variable const* inVariable =
              ExecutionNode::castTo<UpdateReplaceNode const*>(n)
                  ->inKeyVariable();
          if (inVariable != nullptr) {
            auto setter = plan->getVarSetBy(inVariable->id);
            if (setter != nullptr &&
                (setter->getType() == EN::ENUMERATE_COLLECTION ||
                 setter->getType() == EN::INDEX)) {
              std::unordered_map<VariableId, Variable const*> replacements;
              replacements.try_emplace(old->id, inVariable);
              VariableReplacer finder(replacements);
              plan->root()->walk(finder);
              modified = true;
            }
          }
          break;
        }
        case EN::REMOVE: {
          Variable const* inVariable =
              ExecutionNode::castTo<RemoveNode const*>(n)->inVariable();
          TRI_ASSERT(inVariable != nullptr);
          auto setter = plan->getVarSetBy(inVariable->id);
          if (setter != nullptr &&
              (setter->getType() == EN::ENUMERATE_COLLECTION ||
               setter->getType() == EN::INDEX)) {
            std::unordered_map<VariableId, Variable const*> replacements;
            replacements.try_emplace(old->id, inVariable);
            VariableReplacer finder(replacements);
            plan->root()->walk(finder);
            modified = true;
          }
          break;
        }
        default: {
          // do nothing
        }
      }
    }

    if (!n->isVarUsedLater(node->getOutVariableNew())) {
      // "$NEW" is not used later
      node->clearOutVariableNew();
      modified = true;
    }

    if (!n->hasParent()) {
      node->producesResults(false);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
