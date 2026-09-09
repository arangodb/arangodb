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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/Optimizer/Utils/FindProjections.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/UpdateReplaceNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/QueryContext.h"

#include "Basics/StaticStrings.h"

namespace arangodb::aql::optimizer {

// find projection attributes for variable v, starting from node n
// down to the root node of the plan/subquery.
// returns true if it is safe to reduce the full document data from
// "v" to only the projections stored in "attributes". returns false
// otherwise. if false is returned, the contents of "attributes" must
// be ignored by the caller.
// note: this function will *not* wipe "attributes" if there is already
// some data in it.
bool findProjections(ExecutionNode* n, Variable const* v,
                     std::string_view expectedAttribute,
                     bool excludeStartNodeFilterCondition,
                     containers::FlatHashSet<AttributeNamePath>& attributes) {
  using EN = ExecutionNode;

  VarSet vars;

  // Returns true if we managed to extract an attribute path on the given
  // variable. in the true case attributes set is modified by the found
  // AttributeNamePath.
  auto tryAndExtractProjectionsFromExpression =
      [&vars, &attributes, expectedAttribute, v](ExecutionNode const* current,
                                                 AstNode const* node) -> bool {
    vars.clear();
    current->getVariablesUsedHere(vars);

    if (vars.find(v) != vars.end() &&
        !Ast::getReferencedAttributesRecursive(
            node, v, /*expectedAttribute*/ expectedAttribute, attributes,
            current->plan()->getAst()->query().resourceMonitor())) {
      // cannot use projections for this variable
      return false;
    }
    return true;
  };

  auto checkExpression = [&tryAndExtractProjectionsFromExpression](
                             ExecutionNode const* current,
                             Expression const* exp) -> bool {
    return (exp == nullptr ||
            tryAndExtractProjectionsFromExpression(current, exp->node()));
  };

  ExecutionNode* current = n;
  while (current != nullptr) {
    bool doRegularCheck = false;

    if (current->getType() == EN::TRAVERSAL) {
      // check prune condition of traversal
      TraversalNode const* traversalNode =
          ExecutionNode::castTo<TraversalNode const*>(current);

      if (traversalNode->usesInVariable() && traversalNode->inVariable() == v) {
        // start vertex of traversal is our input variable.
        // we need at least the _id attribute from the variable.
        AttributeNamePath anp{
            StaticStrings::IdString,
            traversalNode->plan()->getAst()->query().resourceMonitor()};
        attributes.emplace(std::move(anp));
      }

      // prune condition has to be treated in a special way, because the
      // normal getVariablesUsedHere() call for a TraversalNode does not
      // return the vertex out variable or the edge out variable if they
      // are used by the prune condition.
      Expression const* pruneExpression = traversalNode->pruneExpression();
      if (pruneExpression != nullptr) {
        std::vector<Variable const*> pruneVars;
        traversalNode->getPruneVariables(pruneVars);
        if (std::find(pruneVars.begin(), pruneVars.end(), v) !=
                pruneVars.end() &&
            !Ast::getReferencedAttributesRecursive(
                pruneExpression->node(), v, expectedAttribute, attributes,
                traversalNode->plan()->getAst()->query().resourceMonitor())) {
          // cannot use projections for this variable
          return false;
        }
      }

      if (!checkExpression(traversalNode,
                           traversalNode->postFilterExpression())) {
        // cannot use projections for this variable
        return false;
      }
    } else if (current->getType() == EN::REMOVE) {
      RemoveNode const* removeNode =
          ExecutionNode::castTo<RemoveNode const*>(current);
      if (removeNode->inVariable() == v) {
        // FOR doc IN collection REMOVE doc IN ...
        attributes.emplace(AttributeNamePath(
            StaticStrings::KeyString,
            removeNode->plan()->getAst()->query().resourceMonitor()));
      } else {
        doRegularCheck = true;
      }
    } else if (current->getType() == EN::UPDATE ||
               current->getType() == EN::REPLACE) {
      UpdateReplaceNode const* modificationNode =
          ExecutionNode::castTo<UpdateReplaceNode const*>(current);

      if (modificationNode->inKeyVariable() == v &&
          modificationNode->inDocVariable() != v) {
        // FOR doc IN collection UPDATE/REPLACE doc IN ...
        attributes.emplace(AttributeNamePath(
            StaticStrings::KeyString,
            modificationNode->plan()->getAst()->query().resourceMonitor()));
      } else {
        doRegularCheck = true;
      }
    } else if (current->getType() == EN::CALCULATION) {
      CalculationNode const* calculationNode =
          ExecutionNode::castTo<CalculationNode const*>(current);
      if (!checkExpression(calculationNode, calculationNode->expression())) {
        return false;
      }
    } else if (current->getType() == EN::ENUMERATE_IRESEARCH_VIEW) {
      iresearch::IResearchViewNode const* viewNode =
          ExecutionNode::castTo<iresearch::IResearchViewNode const*>(current);
      // filter condition
      if (!tryAndExtractProjectionsFromExpression(
              viewNode, &viewNode->filterCondition())) {
        return false;
      }
      // scorers
      for (auto const& it : viewNode->scorers()) {
        if (!tryAndExtractProjectionsFromExpression(viewNode, it.node)) {
          return false;
        }
      }
    } else if (current->getType() == EN::GATHER) {
      // compare sort attributes of GatherNode
      auto gn = ExecutionNode::castTo<GatherNode const*>(current);
      for (auto const& it : gn->elements()) {
        if (it.var == v) {
          if (it.attributePath.empty()) {
            // sort of GatherNode refers to the entire document, not to an
            // attribute of the document
            return false;
          }
          // insert attribute name into the set of attributes that we need for
          // our projection
          attributes.emplace(it.attributePath,
                             gn->plan()->getAst()->query().resourceMonitor());
        }
      }
    } else if (current->getType() == EN::ENUMERATE_COLLECTION) {
      EnumerateCollectionNode const* en =
          ExecutionNode::castTo<EnumerateCollectionNode const*>(current);

      if ((!excludeStartNodeFilterCondition || current != n) &&
          en->hasFilter()) {
        if (!Ast::getReferencedAttributesRecursive(
                en->filter()->node(), v,
                /*expectedAttribute*/ expectedAttribute, attributes,
                en->plan()->getAst()->query().resourceMonitor())) {
          return false;
        }
      }
    } else if (current->getType() == EN::INDEX) {
      IndexNode const* indexNode =
          ExecutionNode::castTo<IndexNode const*>(current);
      Condition const* condition = indexNode->condition();

      if (condition != nullptr && condition->root() != nullptr &&
          !tryAndExtractProjectionsFromExpression(indexNode,
                                                  condition->root())) {
        return false;
      }

      if ((!excludeStartNodeFilterCondition || current != n) &&
          indexNode->hasFilter()) {
        if (!Ast::getReferencedAttributesRecursive(
                indexNode->filter()->node(), v,
                /*expectedAttribute*/ expectedAttribute, attributes,
                indexNode->plan()->getAst()->query().resourceMonitor())) {
          return false;
        }
      }
    } else if (current->getType() == EN::SUBQUERY) {
      auto sub = ExecutionNode::castTo<SubqueryNode*>(current);
      ExecutionNode* top = sub->getSubquery();
      while (top->hasDependency()) {
        top = top->getFirstDependency();
      }
      if (!findProjections(top, v, expectedAttribute, false, attributes)) {
        return false;
      }
    } else {
      // all other node types mandate a check
      doRegularCheck = true;
    }

    if (doRegularCheck) {
      vars.clear();
      current->getVariablesUsedHere(vars);

      if (vars.contains(v)) {
        // original variable is still used here
        return false;
      }
    }

    current = current->getFirstParent();
  }

  return true;
}

}  // namespace arangodb::aql::optimizer
