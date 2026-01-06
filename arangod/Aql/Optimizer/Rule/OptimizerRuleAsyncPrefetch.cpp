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

#include "OptimizerRuleAsyncPrefetch.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;

void arangodb::aql::asyncPrefetchRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  struct AsyncPrefetchChecker : WalkerWorkerBase<ExecutionNode> {
    bool before(ExecutionNode* n) override {
      auto eligibility = n->canUseAsyncPrefetching();
      if (eligibility == AsyncPrefetchEligibility::kDisableGlobally) {
        // found a node that we can't support -> abort
        eligible = false;
        return true;
      }
      return false;
    }

    bool eligible{true};
  };

  struct AsyncPrefetchEnabler : WalkerWorkerBase<ExecutionNode> {
    AsyncPrefetchEnabler() { stack.emplace_back(0); }

    bool before(ExecutionNode* n) override {
      auto eligibility = n->canUseAsyncPrefetching();
      if (eligibility == AsyncPrefetchEligibility::kDisableGlobally) {
        // found a node that we can't support -> abort
        TRI_ASSERT(!modified);
        return true;
      }
      if (eligibility ==
          AsyncPrefetchEligibility::kDisableForNodeAndDependencies) {
        TRI_ASSERT(!stack.empty());
        ++stack.back();
      }
      return false;
    }

    void after(ExecutionNode* n) override {
      TRI_ASSERT(!stack.empty());
      auto eligibility = n->canUseAsyncPrefetching();
      if (stack.back() == 0 &&
          eligibility == AsyncPrefetchEligibility::kEnableForNode) {
        // we are currently excluding any node inside a subquery.
        // TODO: lift this restriction.
        n->setIsAsyncPrefetchEnabled(true);
        modified = true;
      }
      if (eligibility ==
          AsyncPrefetchEligibility::kDisableForNodeAndDependencies) {
        TRI_ASSERT(stack.back() > 0);
        --stack.back();
      }
    }

    bool enterSubquery(ExecutionNode*, ExecutionNode*) override {
      // this will disable the optimization for subqueries right now
      stack.push_back(1);
      return true;
    }

    void leaveSubquery(ExecutionNode*, ExecutionNode*) override {
      TRI_ASSERT(!stack.empty());
      stack.pop_back();
    }

    // per query-level (main query, subqueries) stack of eligibilities
    containers::SmallVector<uint32_t, 4> stack;
    bool modified{false};
  };

  bool modified = false;
  // first check if the query satisfies all constraints we have for
  // async prefetching
  AsyncPrefetchChecker checker;
  plan->root()->walk(checker);

  if (checker.eligible) {
    // only if it does, start modifying nodes in the query
    AsyncPrefetchEnabler enabler;
    plan->root()->walk(enabler);
    modified = enabler.modified;
    if (modified) {
      plan->getAst()->setContainsAsyncPrefetch();
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}