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

#include "OptimizerRuleInterchangeAdjacentEnumerations.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
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
    interchangeAdjacentEnumerationsNodeTypes{
      arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
      arangodb::aql::ExecutionNode::ENUMERATE_LIST};
}  // namespace

/// @brief helper to compute lots of permutation tuples
/// a permutation tuple is represented as a single vector together with
/// another vector describing the boundaries of the tuples.
/// Example:
/// data:   0,1,2, 3,4, 5,6
/// starts: 0,     3,   5,      (indices of starts of sections)
/// means a tuple of 3 permutations of 3, 2 and 2 points respectively
/// This function computes the next permutation tuple among the
/// lexicographically sorted list of all such tuples. It returns true
/// if it has successfully computed this and false if the tuple is already
/// the lexicographically largest one. If false is returned, the permutation
/// tuple is back to the beginning.
static bool NextPermutationTuple(std::vector<size_t>& data,
                                 std::vector<size_t>& starts) {
  auto begin = data.begin();  // a random access iterator

  for (size_t i = starts.size(); i-- != 0;) {
    std::vector<size_t>::iterator from = begin + starts[i];
    std::vector<size_t>::iterator to;
    if (i == starts.size() - 1) {
      to = data.end();
    } else {
      to = begin + starts[i + 1];
    }
    if (std::next_permutation(from, to)) {
      return true;
    }
  }

  return false;
}

/// @brief interchange adjacent EnumerateCollectionNodes in all possible ways
void arangodb::aql::interchangeAdjacentEnumerationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;

  // note: we are looking here only for ENUMERATE_COLLECTION
  // and ENUMERATE_LIST node. this optimizer rule runs very
  // early, so nodes of type INDEX or JOIN are not yet present
  // in the plan, at least not the expected ones that come from
  // substituing a full collection scan with an index etc.
  // we may find indexes in the plan when this rule runs, but
  // only some geo/fulltext indexes which are inserted into the
  // plan by an optimizer rule that replaces old AQL functions
  // FULLTEXT/WITHIN with actual FOR loop-index lookups
  plan->findNodesOfType(nodes, ::interchangeAdjacentEnumerationsNodeTypes,
                        true);

  ::arangodb::containers::HashSet<ExecutionNode*> nodesSet;
  for (auto const& n : nodes) {
    TRI_ASSERT(!nodesSet.contains(n));
    nodesSet.emplace(n);
  }

  std::vector<ExecutionNode*> nodesToPermute;
  std::vector<size_t> permTuple;
  std::vector<size_t> starts;
  std::vector<ExecutionNode*> nn;

  containers::FlatHashMap<VariableId, CalculationNode const*> calculations;
  VarSet inputVars;
  VarSet filterVars;

  // We use that the order of the nodes is such that a node B that is among the
  // recursive dependencies of a node A is later in the vector.
  for (auto const& n : nodes) {
    if (!nodesSet.contains(n)) {
      continue;
    }
    nn.clear();
    nn.emplace_back(n);
    nodesSet.erase(n);

    inputVars.clear();

    // Now follow the dependencies as long as we see further such nodes:
    auto nwalker = n;

    while (true) {
      if (!nwalker->hasDependency()) {
        break;
      }

      auto dep = nwalker->getFirstDependency();

      if (dep->getType() != EN::ENUMERATE_COLLECTION &&
          dep->getType() != EN::ENUMERATE_LIST) {
        break;
      }

      if (n->getType() == EN::ENUMERATE_LIST &&
          dep->getType() == EN::ENUMERATE_LIST) {
        break;
      }

      bool foundDependency = false;

      if (dep->getType() == EN::ENUMERATE_LIST &&
          nwalker->getType() == EN::ENUMERATE_COLLECTION) {
        // now checking for the following case:
        //   FOR a IN ... (EnumerateList) (dep)
        //     FOR b IN collection (EnumerateCollection) (nwalker)
        //       LET #1 = b.something == a.whatever
        //       FILTER #1
        // in this case the two FOR loops don't depend on each other,
        // but can be executed as either `a -> b` or `b -> a`.
        // we can simply decide for one order here, so we can save
        // extra permutations
        calculations.clear();

        ExecutionNode* s = nwalker->getFirstParent();
        while (s != nullptr && !foundDependency) {
          if (s->getType() == EN::CALCULATION) {
            auto cn = ExecutionNode::castTo<CalculationNode const*>(s);
            calculations.emplace(cn->outVariable()->id, cn);
          } else if (s->getType() == EN::FILTER) {
            auto fn = ExecutionNode::castTo<FilterNode const*>(s);
            Variable const* inVariable = fn->inVariable();
            if (auto it = calculations.find(inVariable->id);
                it != calculations.end()) {
              filterVars.clear();
              Ast::getReferencedVariables(it->second->expression()->node(),
                                          filterVars);

              for (auto const& outVar : dep->getVariablesSetHere()) {
                if (filterVars.contains(outVar)) {
                  // this means we will not consider this permutation and
                  // save generating an extra plan, thus speeding up the
                  // optimization phase
                  foundDependency = true;
                  break;
                }
              }
            } else {
              // we did not pick up the CalculationNode for the
              // FilterNode we found. this is not necessarily a
              // problem, but we can as well give up now.
              // in the worst case, we create an extra permutation
              // that could have been avoided under some circumstances.
              break;
            }
          } else {
            // found a node type that we don't handle. we currently
            // only support CalculationNodes and FilterNodes
            break;
          }
          s = s->getFirstParent();
        }

        if (foundDependency) {
          break;
        }
      }

      if (!foundDependency) {
        // track variables that we rely on
        nwalker->getVariablesUsedHere(inputVars);

        // check if nodes depend on each other (i.e. node C consumes a variable
        // introduced by node B or A):
        // - FOR a IN A
        // -   FOR b IN a.values
        // -     FOR c IN b.values
        //   or
        // - FOR a IN A
        // -   FOR b IN ...
        // -     FOR c IN a.values
        for (auto const& outVar : dep->getVariablesSetHere()) {
          if (inputVars.contains(outVar)) {
            foundDependency = true;
            break;
          }
        }
      }

      if (foundDependency) {
        break;
      }

      nwalker = dep;
      nn.emplace_back(nwalker);
      nodesSet.erase(nwalker);
    }

    if (nn.size() > 1) {
      // Move it into the permutation tuple:
      starts.emplace_back(permTuple.size());

      for (auto const& nnn : nn) {
        nodesToPermute.emplace_back(nnn);
        permTuple.emplace_back(permTuple.size());
      }
    }
  }

  // Now we have collected all the runs of EnumerateCollectionNodes in the
  // plan, we need to compute all possible permutations of all of them,
  // independently. This is why we need to compute all permutation tuples.

  if (!starts.empty()) {
    NextPermutationTuple(permTuple, starts);  // will never return false

    do {
      // check if we already have enough plans (plus the one plan that we will
      // add at the end of this function)
      if (opt->runOnlyRequiredRules()) {
        // have enough plans. stop permutations
        break;
      }

      // Clone the plan:
      std::unique_ptr<ExecutionPlan> newPlan(plan->clone());

      // Find the nodes in the new plan corresponding to the ones in the
      // old plan that we want to permute:
      std::vector<ExecutionNode*> newNodes;
      newNodes.reserve(nodesToPermute.size());
      for (size_t j = 0; j < nodesToPermute.size(); j++) {
        newNodes.emplace_back(newPlan->getNodeById(nodesToPermute[j]->id()));
      }

      // Now get going with the permutations:
      for (size_t i = 0; i < starts.size(); i++) {
        size_t lowBound = starts[i];
        size_t highBound =
            (i < starts.size() - 1) ? starts[i + 1] : permTuple.size();
        // We need to remove the nodes
        // newNodes[lowBound..highBound-1] in newPlan and replace
        // them by the same ones in a different order, given by
        // permTuple[lowBound..highBound-1].
        auto parent = newNodes[lowBound]->getFirstParent();

        TRI_ASSERT(parent != nullptr);

        // Unlink all those nodes:
        for (size_t j = lowBound; j < highBound; j++) {
          newPlan->unlinkNode(newNodes[j]);
        }

        // And insert them in the new order:
        for (size_t j = highBound; j-- != lowBound;) {
          newPlan->insertDependency(parent, newNodes[permTuple[j]]);
        }
      }

      // OK, the new plan is ready, let's report it:
      opt->addPlan(std::move(newPlan), rule, true);
    } while (NextPermutationTuple(permTuple, starts));
  }

  opt->addPlan(std::move(plan), rule, false);
}
