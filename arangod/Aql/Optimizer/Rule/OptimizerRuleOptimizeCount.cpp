////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "OptimizerRuleOptimizeCount.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

/// @brief turn LENGTH(FOR doc IN ...) subqueries into an optimized count
/// operation
void arangodb::aql::optimizeCountRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  bool modified = false;

  if (plan->getAst()->query().queryOptions().fullCount) {
    // fullCount is unsupported yet
    opt->addPlan(std::move(plan), rule, modified);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  VarSet vars;
  std::unordered_map<ExecutionNode*,
                     std::pair<bool, std::unordered_set<AstNode const*>>>
      candidates;

  // find all calculation nodes in the plan
  for (auto const& n : nodes) {
    auto cn = ExecutionNode::castTo<CalculationNode*>(n);
    AstNode const* root = cn->expression()->node();
    if (root == nullptr) {
      continue;
    }

    std::unordered_map<ExecutionNode*,
                       std::pair<bool, std::unordered_set<AstNode const*>>>
        localCandidates;

    // look for all expressions that contain COUNT(subquery) or LENGTH(subquery)
    auto visitor = [&localCandidates, &plan](AstNode const* node) -> bool {
      if (node->type == NODE_TYPE_FCALL && node->numMembers() > 0) {
        ast::FunctionCallNode fcall(node);
        auto func = fcall.getFunction();
        auto args = fcall.getArguments();
        if (func->name == "LENGTH" || func->name == "COUNT") {
          if (args->numMembers() > 0 &&
              args->getMemberUnchecked(0)->type == NODE_TYPE_REFERENCE) {
            ast::ReferenceNode ref(args->getMemberUnchecked(0));
            Variable const* v = ref.getVariable();
            auto setter = plan->getVarSetBy(v->id);
            if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
              // COUNT(subquery) / LENGTH(subquery)
              auto sn = ExecutionNode::castTo<SubqueryNode*>(setter);
              if (sn->isModificationNode()) {
                // subquery modifies data
                // cannot apply optimization for data-modification queries
                return true;
              }
              if (!sn->isDeterministic()) {
                // subquery is non-deterministic. cannot apply the optimization
                return true;
              }

              auto current = sn->getSubquery();
              if (current == nullptr || current->getType() != EN::RETURN) {
                // subquery does not end with a RETURN instruction - we cannot
                // handle this
                return true;
              }

              auto it = localCandidates.find(setter);
              if (it == localCandidates.end()) {
                localCandidates.emplace(
                    setter,
                    std::make_pair(true,
                                   std::unordered_set<AstNode const*>({node})));
              } else {
                (*it).second.second.emplace(node);
              }
              return false;
            }
          }
        }
      } else if (node->type == NODE_TYPE_REFERENCE) {
        Variable const* v = static_cast<Variable const*>(node->getData());
        auto setter = plan->getVarSetBy(v->id);
        if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
          // subquery used for something else inside the calculation,
          // e.g. FIRST(subquery).
          // we cannot continue with the optimization for this subquery, but for
          // others
          localCandidates[setter].first = false;
          return false;
        }
      }
      return true;
    };

    Ast::traverseReadOnly(root, visitor, [](AstNode const*) {});

    for (auto const& it : localCandidates) {
      // check if subquery result is used for something else than LENGTH/COUNT
      // in *this* calculation
      if (!it.second.first) {
        // subquery result is used for other calculations than COUNT(subquery)
        continue;
      }

      SubqueryNode const* sn =
          ExecutionNode::castTo<SubqueryNode const*>(it.first);
      if (n->isVarUsedLater(sn->outVariable())) {
        // subquery result is used elsewhere later - we cannot optimize
        continue;
      }

      bool valid = true;
      // check if subquery result is used somewhere else before the current
      // calculation we are looking at
      auto current = sn->getFirstParent();
      while (current != nullptr && current != n) {
        vars.clear();
        current->getVariablesUsedHere(vars);
        if (vars.find(sn->outVariable()) != vars.end()) {
          valid = false;
          break;
        }
        current = current->getFirstParent();
      }

      if (valid) {
        // subquery result is not used elsewhere - we can continue optimizing
        // transfer the candidate into the global result
        candidates.emplace(it.first, it.second);
      }
    }
  }

  for (auto const& it : candidates) {
    TRI_ASSERT(it.second.first);
    auto sn = ExecutionNode::castTo<SubqueryNode const*>(it.first);

    // scan from the subquery node to the bottom of the ExecutionPlan to check
    // if any of the following nodes also use the subquery result
    auto current = sn->getSubquery();
    TRI_ASSERT(current->getType() == EN::RETURN);
    auto returnNode = ExecutionNode::castTo<ReturnNode*>(current);
    auto returnSetter = plan->getVarSetBy(returnNode->inVariable()->id);
    if (returnSetter == nullptr) {
      continue;
    }
    if (returnSetter->getType() == EN::CALCULATION) {
      // check if we can understand this type of calculation
      auto cn = ExecutionNode::castTo<CalculationNode*>(returnSetter);
      auto expr = cn->expression();
      if (!expr->isConstant() && !expr->isAttributeAccess()) {
        continue;
      }
    }

    // find the head of the plan/subquery
    while (current->hasDependency()) {
      current = current->getFirstDependency();
    }

    TRI_ASSERT(current != nullptr);

    if (current->getType() != EN::SINGLETON) {
      continue;
    }

    // from here we need to find the first FOR loop.
    // if it is a full collection scan or an index scan, we note its out
    // variable. if we find a nested loop, we abort searching
    bool valid = true;
    ExecutionNode* found = nullptr;
    Variable const* outVariable = nullptr;
    current = current->getFirstParent();

    while (current != nullptr) {
      auto type = current->getType();
      switch (type) {
        case EN::ENUMERATE_COLLECTION:
        case EN::INDEX: {
          if (found != nullptr) {
            // found a nested collection/index scan
            found = nullptr;
            valid = false;
          } else {
            TRI_ASSERT(valid);
            if (dynamic_cast<DocumentProducingNode*>(current)->hasFilter()) {
              // node uses early pruning. this is not supported
              valid = false;
            } else {
              outVariable =
                  dynamic_cast<DocumentProducingNode*>(current)->outVariable();

              if (type == EN::INDEX &&
                  ExecutionNode::castTo<IndexNode const*>(current)
                          ->getIndexes()
                          .size() != 1) {
                // more than one index, so we would need to run uniqueness
                // checks on the results. this is currently unsupported, so
                // don't apply the optimization
                valid = false;
              } else {
                // a FOR loop without an early pruning filter. this is what we
                // are looking for!
                found = current;
              }
            }
          }
          break;
        }

        case EN::DISTRIBUTE:

        case EN::INSERT:
        case EN::UPDATE:
        case EN::REPLACE:
        case EN::REMOVE:
        case EN::UPSERT:
          // we don't handle data-modification queries

        case EN::LIMIT:
          // limit is not yet supported

        case EN::ENUMERATE_LIST:
        case EN::TRAVERSAL:
        case EN::SHORTEST_PATH:
        case EN::ENUMERATE_PATHS:
        case EN::ENUMERATE_IRESEARCH_VIEW: {
          // we don't handle nested FOR loops
          found = nullptr;
          valid = false;
          break;
        }

        case EN::RETURN: {
          // we reached the end
          break;
        }

        default: {
          if (outVariable != nullptr) {
            vars.clear();
            current->getVariablesUsedHere(vars);
            if (vars.find(outVariable) != vars.end()) {
              // result variable of FOR loop is used somewhere where we
              // can't handle it - don't apply the optimization
              found = nullptr;
              valid = false;
            }
          }
          break;
        }
      }

      if (!valid) {
        break;
      }

      current = current->getFirstParent();
    }

    if (valid && found != nullptr) {
      dynamic_cast<DocumentProducingNode*>(found)->setCountFlag();
      returnNode->inVariable(outVariable);

      // replace COUNT/LENGTH with SUM, as we are getting an array from the
      // subquery
      auto& server = plan->getAst()->query().vocbase().server();
      auto func = server.getFeature<AqlFunctionFeature>().byName("SUM");
      for (AstNode const* funcNode : it.second.second) {
        const_cast<AstNode*>(funcNode)->setData(static_cast<void const*>(func));
      }

      if (returnSetter->getType() == EN::CALCULATION) {
        plan->clearVarUsageComputed();
        plan->findVarUsage();

        auto cn = ExecutionNode::castTo<CalculationNode*>(returnSetter);
        if (cn->expression()->isConstant() &&
            !cn->isVarUsedLater(cn->outVariable())) {
          plan->unlinkNode(cn);
        }
      }
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
