////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/CollectOptions.h"
#include "Aql/ClusterNodes.h"
#include "Aql/CollectNode.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Function.h"
#include "Aql/Index.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"
#include "Aql/TraversalConditionFinder.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/json-utilities.h"

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using EN = arangodb::aql::ExecutionNode;

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a SORT operation for IN right-hand side operands
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::sortInValuesRule(Optimizer* opt, ExecutionPlan* plan,
                                     Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));

  for (auto const& n : nodes) {
    // filter nodes always have one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      // filter variable was not introduced by a calculation.
      continue;
    }

    // filter variable was introduced a CalculationNode. now check the
    // expression
    auto s = static_cast<CalculationNode*>(setter);
    auto filterExpression = s->expression();
    auto const* inNode = filterExpression->node();

    TRI_ASSERT(inNode != nullptr);

    // check the filter condition
    if ((inNode->type != NODE_TYPE_OPERATOR_BINARY_IN &&
         inNode->type != NODE_TYPE_OPERATOR_BINARY_NIN) ||
        inNode->canThrow() || !inNode->isDeterministic()) {
      // we better not tamper with this filter
      continue;
    }

    auto rhs = inNode->getMember(1);

    if (rhs->type != NODE_TYPE_REFERENCE) {
      continue;
    }

    auto loop = n->getLoop();

    if (loop == nullptr) {
      // FILTER is not used inside a loop. so it will be used at most once
      // not need to sort the IN values then
      continue;
    }

    variable = static_cast<Variable const*>(rhs->getData());
    setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || (setter->getType() != EN::CALCULATION &&
                              setter->getType() != EN::SUBQUERY)) {
      // variable itself was not introduced by a calculation.
      continue;
    }

    if (loop == setter->getLoop()) {
      // the FILTER and its value calculation are contained in the same loop
      // this means the FILTER will be executed as many times as its value
      // calculation. sorting the IN values will not provide a benefit here
      continue;
    }

    static size_t const Threshold = 8;
    auto ast = plan->getAst();
    AstNode const* originalArg = nullptr;

    if (setter->getType() == EN::CALCULATION) {
      AstNode const* originalNode =
          static_cast<CalculationNode*>(setter)->expression()->node();
      TRI_ASSERT(originalNode != nullptr);

      AstNode const* testNode = originalNode;

      if (originalNode->type == NODE_TYPE_FCALL &&
          static_cast<Function const*>(originalNode->getData())->externalName ==
              "NOOPT") {
        // bypass NOOPT(...)
        TRI_ASSERT(originalNode->numMembers() == 1);
        auto args = originalNode->getMember(0);

        if (args->numMembers() > 0) {
          testNode = args->getMember(0);
        }
      }

      if (testNode->type == NODE_TYPE_VALUE ||
          testNode->type == NODE_TYPE_OBJECT) {
        // not really usable...
        continue;
      }

      if (testNode->type == NODE_TYPE_ARRAY &&
          testNode->numMembers() < Threshold) {
        // number of values is below threshold
        continue;
      }

      if (testNode->isSorted()) {
        // already sorted
        continue;
      }

      originalArg = originalNode;
    } else {
      TRI_ASSERT(setter->getType() == EN::SUBQUERY);
      auto sub = static_cast<SubqueryNode*>(setter);

      // estimate items in subquery
      size_t nrItems = 0;
      sub->getSubquery()->getCost(nrItems);

      if (nrItems < Threshold) {
        continue;
      }

      originalArg = ast->createNodeReference(sub->outVariable());
    }

    TRI_ASSERT(originalArg != nullptr);

    auto args = ast->createNodeArray();
    args->addMember(originalArg);
    auto sorted = ast->createNodeFunctionCall("SORTED_UNIQUE", args);

    auto outVar = ast->variables()->createTemporaryVariable();
    ExecutionNode* calculationNode = nullptr;
    auto expression = new Expression(ast, sorted);
    try {
      calculationNode =
          new CalculationNode(plan, plan->nextId(), expression, outVar);
    } catch (...) {
      delete expression;
      throw;
    }
    plan->registerNode(calculationNode);

    // make the new node a parent of the original calculation node
    calculationNode->addDependency(setter);
    auto const& oldParents = setter->getParents();
    TRI_ASSERT(!oldParents.empty());
    calculationNode->addParent(oldParents[0]);

    oldParents[0]->removeDependencies();
    oldParents[0]->addDependency(calculationNode);
    setter->setParent(calculationNode);

    if (setter->getType() == EN::CALCULATION) {
      // mark the original node as being removable, even if it can throw
      // this is special as the optimizer will normally not remove any nodes
      // if they throw - even when fully unused otherwise
      static_cast<CalculationNode*>(setter)->canRemoveIfThrows(true);
    }

    AstNode* clone = ast->clone(inNode);
    // set sortedness bit for the IN operator
    clone->setBoolValue(true);
    // finally adjust the variable inside the IN calculation
    clone->changeMember(1, ast->createNodeReference(outVar));
    filterExpression->replaceNode(clone);

    modified = true;
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeRedundantSortsRule(Optimizer* opt,
                                             ExecutionPlan* plan,
                                             Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::SORT, true));

  if (nodes.empty()) {
    // quick exit
    opt->addPlan(plan, rule, false);
    return;
  }

  std::unordered_set<ExecutionNode*> toUnlink;

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);

  for (auto const& n : nodes) {
    if (toUnlink.find(n) != toUnlink.end()) {
      // encountered a sort node that we already deleted
      continue;
    }

    auto const sortNode = static_cast<SortNode*>(n);

    auto sortInfo = sortNode->getSortInformation(plan, &buffer);

    if (sortInfo.isValid && !sortInfo.criteria.empty()) {
      // we found a sort that we can understand
      std::vector<ExecutionNode*> stack;

      sortNode->addDependencies(stack);

      int nodesRelyingOnSort = 0;

      while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->getType() == EN::SORT) {
          // we found another sort. now check if they are compatible!

          auto other = static_cast<SortNode*>(current)
                           ->getSortInformation(plan, &buffer);

          switch (sortInfo.isCoveredBy(other)) {
            case SortInformation::unequal: {
              // different sort criteria
              if (nodesRelyingOnSort == 0) {
                // a sort directly followed by another sort: now remove one of
                // them

                if (other.canThrow || !other.isDeterministic) {
                  // if the sort can throw or is non-deterministic, we must not
                  // remove it
                  break;
                }

                if (sortNode->isStable()) {
                  // we should not optimize predecessors of a stable sort (used
                  // in a COLLECT node)
                  // the stable sort is for a reason, and removing any
                  // predecessors sorts might
                  // change the result
                  break;
                }

                // remove sort that is a direct predecessor of a sort
                toUnlink.emplace(current);
              }
              break;
            }

            case SortInformation::otherLessAccurate: {
              toUnlink.emplace(current);
              break;
            }

            case SortInformation::ourselvesLessAccurate: {
              // the sort at the start of the pipeline makes the sort at the end
              // superfluous, so we'll remove it
              toUnlink.emplace(n);
              break;
            }

            case SortInformation::allEqual: {
              // the sort at the end of the pipeline makes the sort at the start
              // superfluous, so we'll remove it
              toUnlink.emplace(current);
              break;
            }
          }
        } else if (current->getType() == EN::FILTER) {
          // ok: a filter does not depend on sort order
        } else if (current->getType() == EN::CALCULATION) {
          // ok: a filter does not depend on sort order only if it does not
          // throw
          if (current->canThrow()) {
            ++nodesRelyingOnSort;
          }
        } else if (current->getType() == EN::ENUMERATE_LIST ||
                   current->getType() == EN::ENUMERATE_COLLECTION ||
                   current->getType() == EN::TRAVERSAL) {
          // ok, but we cannot remove two different sorts if one of these node
          // types is between them
          // example: in the following query, the one sort will be optimized
          // away:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC SORT i.a
          //   DESC RETURN i
          // but in the following query, the sorts will stay:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC LET a =
          //   i.a SORT i.a DESC RETURN i
          ++nodesRelyingOnSort;
        } else {
          // abort at all other type of nodes. we cannot remove a sort beyond
          // them
          // this includes COLLECT and LIMIT
          break;
        }

        if (!current->hasDependency()) {
          // node either has no or more than one dependency. we don't know what
          // to do and must abort
          // note: this will also handle Singleton nodes
          break;
        }

        current->addDependencies(stack);
      }

      if (toUnlink.find(n) == toUnlink.end() && sortNode->simplify(plan)) {
        // sort node had only constant expressions. it will make no difference
        // if we execute it or not
        // so we can remove it
        toUnlink.emplace(n);
      }
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(plan, rule, !toUnlink.empty());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all unnecessary filters
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeUnnecessaryFiltersRule(Optimizer* opt,
                                                 ExecutionPlan* plan,
                                                 Optimizer::Rule const* rule) {
  bool modified = false;
  std::unordered_set<ExecutionNode*> toUnlink;
  // should we enter subqueries??
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));

  for (auto const& n : nodes) {
    // filter nodes always have one input variable
    auto varsUsedHere = n->getVariablesUsedHere();
    TRI_ASSERT(varsUsedHere.size() == 1);

    // now check who introduced our variable
    auto variable = varsUsedHere[0];
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      // filter variable was not introduced by a calculation.
      continue;
    }

    // filter variable was introduced a CalculationNode. now check the
    // expression
    auto s = static_cast<CalculationNode*>(setter);
    auto root = s->expression()->node();

    TRI_ASSERT(root != nullptr);

    if (root->canThrow() || !root->isDeterministic()) {
      // we better not tamper with this filter
      continue;
    }

    // filter expression is constant and thus cannot throw
    // we can now evaluate it safely
    TRI_ASSERT(!s->expression()->canThrow());

    if (root->isTrue()) {
      // filter is always true
      // remove filter node and merge with following node
      toUnlink.emplace(n);
      modified = true;
    } else if (root->isFalse()) {
      // filter is always false
      // now insert a NoResults node below it
      auto noResults = new NoResultsNode(plan, plan->nextId());
      plan->registerNode(noResults);
      plan->replaceNode(n, noResults);
      modified = true;
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove INTO of a COLLECT if not used
/// additionally remove all unused aggregate calculations from a COLLECT
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeCollectVariablesRule(Optimizer* opt,
                                               ExecutionPlan* plan,
                                               Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::COLLECT, true));

  for (auto const& n : nodes) {
    auto collectNode = static_cast<CollectNode*>(n);
    TRI_ASSERT(collectNode != nullptr);

    auto varsUsedLater = n->getVarsUsedLater();
    auto outVariable = collectNode->outVariable();

    if (outVariable != nullptr &&
        varsUsedLater.find(outVariable) == varsUsedLater.end()) {
      // outVariable not used later
      collectNode->clearOutVariable();
      modified = true;
    }

    collectNode->clearAggregates(
        [&varsUsedLater, &modified](
            std::pair<Variable const*,
                      std::pair<Variable const*, std::string>> const& aggregate)
            -> bool {
              if (varsUsedLater.find(aggregate.first) == varsUsedLater.end()) {
                // result of aggregate function not used later
                modified = true;
                return true;
              }
              return false;
            });
  }

  opt->addPlan(plan, rule, modified);
}

class PropagateConstantAttributesHelper {
 public:
  PropagateConstantAttributesHelper() : _constants(), _modified(false) {}

  bool modified() const { return _modified; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief inspects a plan and propages constant values in expressions
  //////////////////////////////////////////////////////////////////////////////

  void propagateConstants(ExecutionPlan* plan) {
    std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));

    for (auto const& node : nodes) {
      auto fn = static_cast<FilterNode*>(node);

      auto inVar = fn->getVariablesUsedHere();
      TRI_ASSERT(inVar.size() == 1);

      auto setter = plan->getVarSetBy(inVar[0]->id);
      if (setter != nullptr && setter->getType() == EN::CALCULATION) {
        auto cn = static_cast<CalculationNode*>(setter);
        auto expression = cn->expression();

        if (expression != nullptr) {
          collectConstantAttributes(const_cast<AstNode*>(expression->node()));
        }
      }
    }

    if (!_constants.empty()) {
      for (auto const& node : nodes) {
        auto fn = static_cast<FilterNode*>(node);

        auto inVar = fn->getVariablesUsedHere();
        TRI_ASSERT(inVar.size() == 1);

        auto setter = plan->getVarSetBy(inVar[0]->id);
        if (setter != nullptr && setter->getType() == EN::CALCULATION) {
          auto cn = static_cast<CalculationNode*>(setter);
          auto expression = cn->expression();

          if (expression != nullptr) {
            insertConstantAttributes(const_cast<AstNode*>(expression->node()));
          }
        }
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief inspects an expression (recursively) and notes constant attribute
  /// values so they can be propagated later
  //////////////////////////////////////////////////////////////////////////////

  void collectConstantAttributes(AstNode* node) {
    if (node == nullptr) {
      return;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      collectConstantAttributes(lhs);
      collectConstantAttributes(rhs);
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        inspectConstantAttribute(rhs, lhs);
      } else if (rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        inspectConstantAttribute(lhs, rhs);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief traverses an AST part recursively and patches it by inserting
  /// constant values
  //////////////////////////////////////////////////////////////////////////////

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

      if (!lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        insertConstantAttribute(node, 1);
      }
      if (!rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        insertConstantAttribute(node, 0);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract an attribute and its variable from an attribute access
  /// (e.g. `a.b.c` will return variable `a` and attribute name `b.c.`.
  //////////////////////////////////////////////////////////////////////////////

  bool getAttribute(AstNode const* attribute, Variable const*& variable,
                    std::string& name) {
    TRI_ASSERT(attribute != nullptr &&
               attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS);
    TRI_ASSERT(name.empty());

    while (attribute->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      name = std::string(".") + std::string(attribute->getStringValue(),
                                            attribute->getStringLength()) +
             name;
      attribute = attribute->getMember(0);
    }

    if (attribute->type != NODE_TYPE_REFERENCE) {
      return false;
    }

    variable = static_cast<Variable const*>(attribute->getData());
    TRI_ASSERT(variable != nullptr);

    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief inspect the constant value assigned to an attribute
  /// the attribute value will be stored so it can be inserted for the attribute
  /// later
  //////////////////////////////////////////////////////////////////////////////

  void inspectConstantAttribute(AstNode const* attribute,
                                AstNode const* value) {
    Variable const* variable = nullptr;
    std::string name;

    if (!getAttribute(attribute, variable, name)) {
      return;
    }

    auto it = _constants.find(variable);

    if (it == _constants.end()) {
      _constants.emplace(
          variable,
          std::unordered_map<std::string, AstNode const*>{{name, value}});
      return;
    }

    auto it2 = (*it).second.find(name);

    if (it2 == (*it).second.end()) {
      // first value for the attribute
      (*it).second.emplace(name, value);
    } else {
      auto previous = (*it2).second;

      if (previous == nullptr) {
        // we have multiple different values for the attribute. better not use
        // this attribute
        return;
      }

      if (TRI_CompareValuesJson(value->computeJson(), previous->computeJson(),
                                true) != 0) {
        // different value found for an already tracked attribute. better not
        // use this attribute
        (*it2).second = nullptr;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief patches an AstNode by inserting a constant value into it
  //////////////////////////////////////////////////////////////////////////////

  void insertConstantAttribute(AstNode* parentNode, size_t accessIndex) {
    Variable const* variable = nullptr;
    std::string name;

    if (!getAttribute(parentNode->getMember(accessIndex), variable, name)) {
      return;
    }

    auto constantValue = getConstant(variable, name);

    if (constantValue != nullptr) {
      parentNode->changeMember(accessIndex,
                               const_cast<AstNode*>(constantValue));
      _modified = true;
    }
  }

  std::unordered_map<Variable const*,
                     std::unordered_map<std::string, AstNode const*>>
      _constants;

  bool _modified;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief propagate constant attributes in FILTERs
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::propagateConstantAttributesRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  PropagateConstantAttributesHelper helper;
  helper.propagateConstants(plan);

  opt->addPlan(plan, rule, helper.modified());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove SORT RAND() if appropriate
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeSortRandRule(Optimizer* opt, ExecutionPlan* plan,
                                       Optimizer::Rule const* rule) {
  bool modified = false;
  // should we enter subqueries??
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::SORT, true));

  for (auto const& n : nodes) {
    auto node = static_cast<SortNode*>(n);
    auto const& elements = node->getElements();
    if (elements.size() != 1) {
      // we're looking for "SORT RAND()", which has just one sort criterion
      continue;
    }

    auto const variable = elements[0].first;
    TRI_ASSERT(variable != nullptr);

    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto cn = static_cast<CalculationNode*>(setter);
    auto const expression = cn->expression();

    if (expression == nullptr || expression->node() == nullptr ||
        expression->node()->type != NODE_TYPE_FCALL) {
      // not the right type of node
      continue;
    }

    auto funcNode = expression->node();
    auto func = static_cast<Function const*>(funcNode->getData());

    // we're looking for "RAND()", which is a function call
    // with an empty parameters array
    if (func->externalName != "RAND" || funcNode->numMembers() != 1 ||
        funcNode->getMember(0)->numMembers() != 0) {
      continue;
    }

    // now we're sure we got SORT RAND() !

    // we found what we were looking for!
    // now check if the dependencies qualify
    if (!n->hasDependency()) {
      break;
    }

    auto current = n->getFirstDependency();
    ExecutionNode* collectionNode = nullptr;

    while (current != nullptr) {
      if (current->canThrow()) {
        // we shouldn't bypass a node that can throw
        collectionNode = nullptr;
        break;
      }

      switch (current->getType()) {
        case EN::SORT:
        case EN::COLLECT:
        case EN::FILTER:
        case EN::SUBQUERY:
        case EN::ENUMERATE_LIST:
        case EN::TRAVERSAL:
        case EN::INDEX: {
          // if we found another SortNode, an CollectNode, FilterNode, a
          // SubqueryNode,
          // an EnumerateListNode, a TraversalNode or an IndexNode
          // this means we cannot apply our optimization
          collectionNode = nullptr;
          current = nullptr;
          continue;  // this will exit the while loop
        }

        case EN::ENUMERATE_COLLECTION: {
          if (collectionNode == nullptr) {
            // note this node
            collectionNode = current;
            break;
          } else {
            // we already found another collection node before. this means we
            // should not apply our optimization
            collectionNode = nullptr;
            current = nullptr;
            continue;  // this will exit the while loop
          }
          // cannot get here
          TRI_ASSERT(false);
        }

        default: {
          // ignore all other nodes
        }
      }

      if (!current->hasDependency()) {
        break;
      }

      current = current->getFirstDependency();
    }

    if (collectionNode != nullptr) {
      // we found a node to modify!
      TRI_ASSERT(collectionNode->getType() == EN::ENUMERATE_COLLECTION);
      // set the random iteration flag for the EnumerateCollectionNode
      static_cast<EnumerateCollectionNode*>(collectionNode)->setRandom();

      // remove the SortNode
      // note: the CalculationNode will be removed by
      // "remove-unnecessary-calculations"
      // rule if not used

      plan->unlinkNode(n);
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move calculations up in the plan
/// this rule modifies the plan in place
/// it aims to move up calculations as far up in the plan as possible, to
/// avoid redundant calculations in inner loops
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::moveCalculationsUpRule(Optimizer* opt, ExecutionPlan* plan,
                                           Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(
      plan->findNodesOfType(EN::CALCULATION, true));
  bool modified = false;

  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);

    if (nn->expression()->canThrow() || !nn->expression()->isDeterministic()) {
      // we will only move expressions up that cannot throw and that are
      // deterministic
      continue;
    }

    std::unordered_set<Variable const*> neededVars;
    n->getVariablesUsedHere(neededVars);

    std::vector<ExecutionNode*> stack;

    n->addDependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      bool found = false;

      for (auto const& v : current->getVariablesSetHere()) {
        if (neededVars.find(v) != neededVars.end()) {
          // shared variable, cannot move up any more
          found = true;
          break;
        }
      }

      if (found) {
        // done with optimizing this calculation node
        break;
      }

      if (!current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      current->addDependencies(stack);

      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into before the current node
      plan->insertDependency(current, n);
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move calculations down in the plan
/// this rule modifies the plan in place
/// it aims to move calculations as far down in the plan as possible, beyond
/// FILTER and LIMIT operations
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::moveCalculationsDownRule(Optimizer* opt,
                                             ExecutionPlan* plan,
                                             Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(
      plan->findNodesOfType(EN::CALCULATION, true));
  bool modified = false;

  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow() || !nn->expression()->isDeterministic()) {
      // we will only move expressions down that cannot throw and that are
      // deterministic
      continue;
    }

    // this is the variable that the calculation will set
    auto variable = nn->outVariable();

    std::vector<ExecutionNode*> stack;
    n->addParents(stack);

    bool shouldMove = false;
    ExecutionNode* lastNode = nullptr;

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      lastNode = current;
      bool done = false;

      for (auto const& v : current->getVariablesUsedHere()) {
        if (v == variable) {
          // the node we're looking at needs the variable we're setting.
          // can't push further!
          done = true;
          break;
        }
      }

      if (done) {
        // done with optimizing this calculation node
        break;
      }

      auto const currentType = current->getType();

      if (currentType == EN::FILTER || currentType == EN::SORT ||
          currentType == EN::LIMIT || currentType == EN::SUBQUERY) {
        // we found something interesting that justifies moving our node down
        shouldMove = true;
      } else if (currentType == EN::INDEX ||
                 currentType == EN::ENUMERATE_COLLECTION ||
                 currentType == EN::ENUMERATE_LIST ||
                 currentType == EN::TRAVERSAL || currentType == EN::COLLECT ||
                 currentType == EN::NORESULTS) {
        // we will not push further down than such nodes
        shouldMove = false;
        break;
      }

      if (!current->hasParent()) {
        break;
      }

      current->addParents(stack);
    }

    if (shouldMove && lastNode != nullptr) {
      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into before the current node
      plan->insertDependency(lastNode, n);
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fuse calculations in the plan
/// this rule modifies the plan in place
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::fuseCalculationsRule(Optimizer* opt, ExecutionPlan* plan,
                                         Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(
      plan->findNodesOfType(EN::CALCULATION, true));

  if (nodes.size() < 2) {
    opt->addPlan(plan, rule, false);
    return;
  }

  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);
    if (nn->expression()->canThrow() || !nn->expression()->isDeterministic()) {
      // we will only fuse calculations of expressions that cannot throw and
      // that are deterministic
      continue;
    }

    if (toUnlink.find(n) != toUnlink.end()) {
      // do not process the same node twice
      continue;
    }

    std::unordered_map<Variable const*, ExecutionNode*> toInsert;
    for (auto& it : nn->getVariablesUsedHere()) {
      if (!n->isVarUsedLater(it)) {
        toInsert.emplace(it, n);
      }
    }

    TRI_ASSERT(n->hasDependency());
    std::vector<ExecutionNode*> stack{n->getFirstDependency()};

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      bool handled = false;

      if (current->getType() == EN::CALCULATION) {
        auto otherExpression =
            static_cast<CalculationNode const*>(current)->expression();

        if (otherExpression->isDeterministic() &&
            !otherExpression->canThrow() &&
            otherExpression->canRunOnDBServer() ==
                nn->expression()->canRunOnDBServer()) {
          // found another calculation node
          auto varsSet(current->getVariablesSetHere());
          if (varsSet.size() == 1) {
            // check if it is a calculation for a variable that we are looking
            // for
            auto it = toInsert.find(varsSet[0]);

            if (it != toInsert.end()) {
              // remove the variable from the list of search variables
              toInsert.erase(it);

              // replace the variable reference in the original expression with
              // the expression for that variable
              auto expression = nn->expression();
              TRI_ASSERT(expression != nullptr);
              expression->replaceVariableReference((*it).first,
                                                   otherExpression->node());

              toUnlink.emplace(current);

              // insert the calculations' own referenced variables into the list
              // of search variables
              for (auto& it2 : current->getVariablesUsedHere()) {
                if (!n->isVarUsedLater(it2)) {
                  toInsert.emplace(it2, n);
                }
              }

              handled = true;
            }
          }
        }
      }

      if (!handled) {
        // remove all variables from our list that might be used elsewhere
        for (auto& it : current->getVariablesUsedHere()) {
          toInsert.erase(it);
        }
      }

      if (toInsert.empty()) {
        // done
        break;
      }

      if (!current->hasDependency()) {
        break;
      }

      stack.emplace_back(current->getFirstDependency());
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(plan, rule, !toUnlink.empty());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the "right" type of CollectNode and
/// add a sort node for each COLLECT (note: the sort may be removed later)
/// this rule cannot be turned off (otherwise, the query result might be wrong!)
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::specializeCollectRule(Optimizer* opt, ExecutionPlan* plan,
                                          Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::COLLECT, true));
  bool modified = false;

  for (auto const& n : nodes) {
    auto collectNode = static_cast<CollectNode*>(n);

    if (collectNode->isSpecialized()) {
      // already specialized this node
      continue;
    }

    auto const& groupVariables = collectNode->groupVariables();

    // test if we can use an alternative version of COLLECT with a hash table
    bool const canUseHashAggregation =
        (!groupVariables.empty() &&
         (!collectNode->hasOutVariable() || collectNode->count()) &&
         collectNode->getOptions().canUseHashMethod());

    if (canUseHashAggregation) {
      // create a new plan with the adjusted COLLECT node
      std::unique_ptr<ExecutionPlan> newPlan(plan->clone());

      // use the cloned COLLECT node
      auto newCollectNode =
          static_cast<CollectNode*>(newPlan->getNodeById(collectNode->id()));
      TRI_ASSERT(newCollectNode != nullptr);

      // specialize the CollectNode so it will become a HashedCollectBlock
      // later
      // additionally, add a SortNode BEHIND the CollectNode (to sort the
      // final result)
      newCollectNode->aggregationMethod(
          CollectOptions::CollectMethod::COLLECT_METHOD_HASH);
      newCollectNode->specialized();

      if (!collectNode->isDistinctCommand()) {
        // add the post-SORT
        std::vector<std::pair<Variable const*, bool>> sortElements;
        for (auto const& v : newCollectNode->groupVariables()) {
          sortElements.emplace_back(std::make_pair(v.first, true));
        }

        auto sortNode =
            new SortNode(newPlan.get(), newPlan->nextId(), sortElements, false);
        newPlan->registerNode(sortNode);

        TRI_ASSERT(newCollectNode->hasParent());
        auto const& parents = newCollectNode->getParents();
        auto parent = parents[0];

        sortNode->addDependency(newCollectNode);
        parent->replaceDependency(newCollectNode, sortNode);
      }
      newPlan->findVarUsage();

      if (nodes.size() > 1) {
        // this will tell the optimizer to optimize the cloned plan with this
        // specific rule again
        opt->addPlan(newPlan.release(), rule, true,
                     static_cast<int>(rule->level - 1));
      } else {
        // no need to run this specific rule again on the cloned plan
        opt->addPlan(newPlan.release(), rule, true);
      }
    }

    // mark node as specialized, so we do not process it again
    collectNode->specialized();

    // finally, adjust the original plan and create a sorted version of COLLECT

    // specialize the CollectNode so it will become a SortedCollectBlock
    // later
    collectNode->aggregationMethod(
        CollectOptions::CollectMethod::COLLECT_METHOD_SORTED);

    // insert a SortNode IN FRONT OF the CollectNode
    if (!groupVariables.empty()) {
      std::vector<std::pair<Variable const*, bool>> sortElements;
      for (auto const& v : groupVariables) {
        sortElements.emplace_back(std::make_pair(v.second, true));
      }

      auto sortNode = new SortNode(plan, plan->nextId(), sortElements, true);
      plan->registerNode(sortNode);

      TRI_ASSERT(collectNode->hasDependency());
      auto dep = collectNode->getFirstDependency();
      sortNode->addDependency(dep);
      collectNode->replaceDependency(dep, sortNode);

      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief split and-combined filters and break them into smaller parts
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::splitFiltersRule(Optimizer* opt, ExecutionPlan* plan,
                                     Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));
  bool modified = false;

  for (auto const& n : nodes) {
    auto inVars(n->getVariablesUsedHere());
    TRI_ASSERT(inVars.size() == 1);
    auto setter = plan->getVarSetBy(inVars[0]->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto cn = static_cast<CalculationNode*>(setter);
    auto const expression = cn->expression();

    if (expression->canThrow() || !expression->isDeterministic() ||
        expression->node()->type != NODE_TYPE_OPERATOR_BINARY_AND) {
      continue;
    }

    std::vector<AstNode*> stack{expression->nodeForModification()};

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        stack.emplace_back(current->getMember(0));
        stack.emplace_back(current->getMember(1));
      } else {
        modified = true;

        ExecutionNode* calculationNode = nullptr;
        auto outVar = plan->getAst()->variables()->createTemporaryVariable();
        auto expression = new Expression(plan->getAst(), current);
        try {
          calculationNode =
              new CalculationNode(plan, plan->nextId(), expression, outVar);
        } catch (...) {
          delete expression;
          throw;
        }
        plan->registerNode(calculationNode);

        plan->insertDependency(n, calculationNode);

        auto filterNode = new FilterNode(plan, plan->nextId(), outVar);
        plan->registerNode(filterNode);

        plan->insertDependency(n, filterNode);
      }
    }

    if (modified) {
      plan->unlinkNode(n, false);
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move filters up in the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// filters are not pushed beyond limits
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::moveFiltersUpRule(Optimizer* opt, ExecutionPlan* plan,
                                      Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));
  bool modified = false;

  for (auto const& n : nodes) {
    auto neededVars = n->getVariablesUsedHere();
    TRI_ASSERT(neededVars.size() == 1);

    std::vector<ExecutionNode*> stack;
    n->addDependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::LIMIT) {
        // cannot push a filter beyond a LIMIT node
        break;
      }

      if (current->canThrow()) {
        // must not move a filter beyond a node that can throw
        break;
      }

      if (current->getType() == EN::CALCULATION) {
        // must not move a filter beyond a node with a non-deterministic result
        auto calculation = static_cast<CalculationNode const*>(current);
        if (!calculation->expression()->isDeterministic()) {
          break;
        }
      }

      bool found = false;

      for (auto const& v : current->getVariablesSetHere()) {
        for (auto it = neededVars.begin(); it != neededVars.end(); ++it) {
          if ((*it)->id == v->id) {
            // shared variable, cannot move up any more
            found = true;
            break;
          }
        }
      }

      if (found) {
        // done with optimizing this calculation node
        break;
      }

      if (!current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      current->addDependencies(stack);

      // first, unlink the filter from the plan
      plan->unlinkNode(n);
      // and re-insert into plan in front of the current node
      plan->insertDependency(current, n);
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

class arangodb::aql::RedundantCalculationsReplacer final
    : public WalkerWorker<ExecutionNode> {
 public:
  explicit RedundantCalculationsReplacer(
      std::unordered_map<VariableId, Variable const*> const& replacements)
      : _replacements(replacements) {}

  template <typename T>
  void replaceInVariable(ExecutionNode* en) {
    auto node = static_cast<T*>(en);

    node->_inVariable = Variable::replace(node->_inVariable, _replacements);
  }

  void replaceInCalculation(ExecutionNode* en) {
    auto node = static_cast<CalculationNode*>(en);
    std::unordered_set<Variable const*> variables;
    node->expression()->variables(variables);

    // check if the calculation uses any of the variables that we want to
    // replace
    for (auto const& it : variables) {
      if (_replacements.find(it->id) != _replacements.end()) {
        // calculation uses a to-be-replaced variable
        node->expression()->replaceVariables(_replacements);
        return;
      }
    }
  }

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::ENUMERATE_LIST: {
        replaceInVariable<EnumerateListNode>(en);
        break;
      }

      case EN::RETURN: {
        replaceInVariable<ReturnNode>(en);
        break;
      }

      case EN::CALCULATION: {
        replaceInCalculation(en);
        break;
      }

      case EN::FILTER: {
        replaceInVariable<FilterNode>(en);
        break;
      }

      case EN::COLLECT: {
        auto node = static_cast<CollectNode*>(en);
        for (auto& variable : node->_groupVariables) {
          variable.second = Variable::replace(variable.second, _replacements);
        }
        for (auto& variable : node->_aggregateVariables) {
          variable.second.first = Variable::replace(variable.second.first, _replacements);
        }
        break;
      }

      case EN::SORT: {
        auto node = static_cast<SortNode*>(en);
        for (auto& variable : node->_elements) {
          variable.first = Variable::replace(variable.first, _replacements);
        }
        break;
      }

      default: {
        // ignore all other types of nodes
      }
    }

    // always continue
    return false;
  }

 private:
  std::unordered_map<VariableId, Variable const*> const& _replacements;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNode(s) that are repeatedly used in a query
/// (i.e. common expressions)
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeRedundantCalculationsRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(
      plan->findNodesOfType(EN::CALCULATION, true));

  if (nodes.size() < 2) {
    // quick exit
    opt->addPlan(plan, rule, false);
    return;
  }

  arangodb::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  std::unordered_map<VariableId, Variable const*> replacements;

  for (auto const& n : nodes) {
    auto nn = static_cast<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic()) {
      // If this node is non-deterministic, we must not touch it!
      continue;
    }

    auto outvar = n->getVariablesSetHere();
    TRI_ASSERT(outvar.size() == 1);

    try {
      nn->expression()->stringifyIfNotTooLong(&buffer);
    } catch (...) {
      // expression could not be stringified (maybe because not all node types
      // are supported). this is not an error, we just skip the optimization
      buffer.reset();
      continue;
    }

    std::string const referenceExpression(buffer.c_str(), buffer.length());
    buffer.reset();

    std::vector<ExecutionNode*> stack;
    n->addDependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::CALCULATION) {
        try {
          static_cast<CalculationNode*>(current)
              ->expression()
              ->stringifyIfNotTooLong(&buffer);
        } catch (...) {
          // expression could not be stringified (maybe because not all node
          // types
          // are supported). this is not an error, we just skip the optimization
          buffer.reset();
          continue;
        }

        std::string const compareExpression(buffer.c_str(), buffer.length());
        buffer.reset();

        if (compareExpression == referenceExpression) {
          // expressions are identical
          auto outvars = current->getVariablesSetHere();
          TRI_ASSERT(outvars.size() == 1);

          // check if target variable is already registered as a replacement
          // this covers the following case:
          // - replacements is set to B => C
          // - we're now inserting a replacement A => B
          // the goal now is to enter a replacement A => C instead of A => B
          auto target = outvars[0];
          while (target != nullptr) {
            auto it = replacements.find(target->id);

            if (it != replacements.end()) {
              target = (*it).second;
            } else {
              break;
            }
          }
          replacements.emplace(outvar[0]->id, target);

          // also check if the insertion enables further shortcuts
          // this covers the following case:
          // - replacements is set to A => B
          // - we have just inserted a replacement B => C
          // the goal now is to change the replacement A => B to A => C
          for (auto it = replacements.begin(); it != replacements.end(); ++it) {
            if ((*it).second == outvar[0]) {
              (*it).second = target;
            }
          }
        }
      }

      if (current->getType() == EN::COLLECT) {
        if (static_cast<CollectNode*>(current)->hasOutVariable()) {
          // COLLECT ... INTO is evil (tm): it needs to keep all already defined
          // variables
          // we need to abort optimization here
          break;
        }
      }

      if (!current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      current->addDependencies(stack);
    }
  }

  if (!replacements.empty()) {
    // finally replace the variables
    RedundantCalculationsReplacer finder(replacements);
    plan->root()->walk(&finder);

    opt->addPlan(plan, rule, true);
  } else {
    // no changes
    opt->addPlan(plan, rule, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNodes and SubqueryNodes that are never needed
/// this modifies an existing plan in place
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeUnnecessaryCalculationsRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  std::vector<ExecutionNode::NodeType> const types = {EN::CALCULATION,
                                                      EN::SUBQUERY};

  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(types, true));
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
    if (n->getType() == EN::CALCULATION) {
      auto nn = static_cast<CalculationNode*>(n);

      if (nn->canThrow() && !nn->canRemoveIfThrows()) {
        // If this node can throw, we must not optimize it away!
        continue;
      }
    } else {
      auto nn = static_cast<SubqueryNode*>(n);

      if (nn->canThrow()) {
        // subqueries that can throw must not be optimized away
        continue;
      }

      if (nn->isModificationQuery()) {
        // subqueries that modify data must not be optimized away
        continue;
      }
    }

    auto outvar = n->getVariablesSetHere();
    TRI_ASSERT(outvar.size() == 1);
    auto varsUsedLater = n->getVarsUsedLater();

    if (varsUsedLater.find(outvar[0]) == varsUsedLater.end()) {
      // The variable whose value is calculated here is not used at
      // all further down the pipeline! We remove the whole
      // calculation node,
      toUnlink.emplace(n);
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(plan, rule, !toUnlink.empty());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief useIndex, try to use an index for filtering
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::useIndexesRule(Optimizer* opt, ExecutionPlan* plan,
                                   Optimizer::Rule const* rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  std::vector<ExecutionNode*> nodes(plan->findEndNodes(true));

  std::unordered_map<size_t, ExecutionNode*> changes;

  auto cleanupChanges = [&changes]() -> void {
    for (auto& v : changes) {
      delete v.second;
    }
    changes.clear();
  };

  TRI_DEFER(cleanupChanges());
  bool hasEmptyResult = false;
  for (auto const& n : nodes) {
    ConditionFinder finder(plan, &changes, &hasEmptyResult);
    n->walk(&finder);
  }

  if (!changes.empty()) {
    for (auto& it : changes) {
      plan->registerNode(it.second);
      plan->replaceNode(plan->getNodeById(it.first), it.second);

      // prevent double deletion by cleanupChanges()
      it.second = nullptr;
    }
    opt->addPlan(plan, rule, true);
  } else {
    opt->addPlan(plan, rule, hasEmptyResult);
  }
}

struct SortToIndexNode final : public WalkerWorker<ExecutionNode> {
  ExecutionPlan* _plan;
  SortNode* _sortNode;
  std::vector<std::pair<VariableId, bool>> _sorts;
  std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
  bool _modified;

 public:
  explicit SortToIndexNode(ExecutionPlan* plan)
      : _plan(plan),
        _sortNode(nullptr),
        _sorts(),
        _variableDefinitions(),
        _modified(false) {}

  bool handleEnumerateCollectionNode(
      EnumerateCollectionNode* enumerateCollectionNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (enumerateCollectionNode->isInInnerLoop()) {
      // index node contained in an outer loop. must not optimize away the sort!
      return true;
    }

    SortCondition sortCondition(_sorts, std::vector<std::vector<arangodb::basics::AttributeName>>(), _variableDefinitions);

    if (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess() &&
        sortCondition.isUnidirectional()) {
      // we have found a sort condition, which is unidirectionl
      // now check if any of the collection's indexes covers it

      Variable const* outVariable = enumerateCollectionNode->outVariable();
      auto const& indexes = enumerateCollectionNode->collection()->getIndexes();
      arangodb::aql::Index const* bestIndex = nullptr;
      double bestCost = 0.0;
      size_t bestNumCovered = 0;

      for (auto& index : indexes) {
        if (!index->isSorted() || index->sparse) {
          // can only use a sorted index
          // cannot use a sparse index for sorting
          continue;
        }

        size_t const numCovered =
            sortCondition.coveredAttributes(outVariable, index->fields); 

        if (numCovered == 0) {
          continue;
        }

        double estimatedCost = 0.0;
        if (!index->supportsSortCondition(
                &sortCondition, 
                outVariable,
                enumerateCollectionNode->collection()->count(),
                estimatedCost)) {
          // should never happen
          TRI_ASSERT(false);
          continue;
        }
 
        if (bestIndex == nullptr || estimatedCost < bestCost) {
          bestIndex = index;
          bestCost = estimatedCost;
          bestNumCovered = numCovered;
        }
      }

      if (bestIndex != nullptr) {
        auto condition = std::make_unique<Condition>(_plan->getAst());
        condition->normalize(_plan);

        std::unique_ptr<ExecutionNode> newNode(new IndexNode(
            _plan, _plan->nextId(), enumerateCollectionNode->vocbase(),
            enumerateCollectionNode->collection(), outVariable,
            std::vector<Index const*>({bestIndex}), condition.get(),
            sortCondition.isDescending()));

        condition.release();

        auto n = newNode.release();

        _plan->registerNode(n);
        _plan->replaceNode(enumerateCollectionNode, n);
        _modified = true;

        if (bestNumCovered == sortCondition.numAttributes()) {
          // if the index covers the complete sort condition, we can also remove
          // the sort node
          _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
        }
      }
    }

    return true;  // always abort further searching here
  }

  bool handleIndexNode(IndexNode* indexNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (indexNode->isInInnerLoop()) {
      // index node contained in an outer loop. must not optimize away the sort!
      return true;
    }

    auto const& indexes = indexNode->getIndexes();
    auto cond = indexNode->condition();
    TRI_ASSERT(cond != nullptr);
          
    Variable const* outVariable = indexNode->outVariable();
    TRI_ASSERT(outVariable != nullptr);

    if (indexes.size() != 1) {
      // can only use this index node if it uses exactly one index or multiple
      // indexes on exactly the same attributes

      if (!cond->isSorted()) {
        // index conditions do not guarantee sortedness
        return true;
      }

      std::vector<std::vector<arangodb::basics::AttributeName>> seen;

      for (auto& index : indexes) {
        if (index->sparse) {
          // cannot use a sparse index for sorting
          return true;
        }

        if (!seen.empty() && arangodb::basics::AttributeName::isIdentical(
                                 index->fields, seen, true)) {
          // different attributes
          return true;
        }
      }

      // all indexes use the same attributes and index conditions guarantee
      // sorted output
    }

    // if we get here, we either have one index or multiple indexes on the same
    // attributes
    auto index = indexes[0];
    bool handled = false;

    SortCondition sortCondition(_sorts, cond->getConstAttributes(outVariable, !index->sparse), _variableDefinitions);

    bool const isOnlyAttributeAccess =
        (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess());

    if (isOnlyAttributeAccess && index->isSorted() && !index->sparse &&
        sortCondition.isUnidirectional() &&
        sortCondition.isDescending() == indexNode->reverse()) {
      // we have found a sort condition, which is unidirectional and in the same
      // order as the IndexNode...
      // now check if the sort attributes match the ones of the index
      size_t const numCovered =
          sortCondition.coveredAttributes(outVariable, index->fields); 

      if (numCovered >= sortCondition.numAttributes()) {
        // sort condition is fully covered by index... now we can remove the
        // sort node from the plan
        _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
        _modified = true;
        handled = true;
      }
    }

    if (!handled && isOnlyAttributeAccess && indexes.size() == 1) {
      // special case... the index cannot be used for sorting, but we only
      // compare with equality
      // lookups. now check if the equality lookup attributes are the same as
      // the index attributes
      auto root = cond->root();

      if (root != nullptr) {
        auto condNode = root->getMember(0);

        if (condNode->isOnlyEqualityMatch()) {
          // now check if the index fields are the same as the sort condition
          // fields
          // e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1, c.value2
          size_t const numCovered = 
              sortCondition.coveredAttributes(outVariable, index->fields); 

          if (numCovered == sortCondition.numAttributes() &&
              sortCondition.isUnidirectional() &&
              (index->isSorted() ||
               index->fields.size() == sortCondition.numAttributes())) {
            // no need to sort
            _plan->unlinkNode(_plan->getNodeById(_sortNode->id()));
            _modified = true;
          }
        }
      }
    }

    return true;  // always abort after we found an IndexNode
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::ENUMERATE_LIST:
      case EN::SUBQUERY:
      case EN::FILTER:
        return false;  // skip. we don't care.

      case EN::CALCULATION: {
        auto outvars = en->getVariablesSetHere();
        TRI_ASSERT(outvars.size() == 1);

        _variableDefinitions.emplace(
            outvars[0]->id,
            static_cast<CalculationNode const*>(en)->expression()->node());
        return false;
      }

      case EN::SINGLETON:
      case EN::COLLECT:
      case EN::INSERT:
      case EN::REMOVE:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::SCATTER:
      case EN::DISTRIBUTE:
      case EN::GATHER:
      case EN::REMOTE:
      case EN::ILLEGAL:
      case EN::LIMIT:  // LIMIT is criterion to stop
        return true;   // abort.

      case EN::SORT:  // pulling two sorts together is done elsewhere.
        if (!_sorts.empty() || _sortNode != nullptr) {
          return true;  // a different SORT node. abort
        }
        _sortNode = static_cast<SortNode*>(en);
        for (auto& it : _sortNode->getElements()) {
          _sorts.emplace_back((it.first)->id, it.second);
        }
        return false;

      case EN::INDEX:
        return handleIndexNode(static_cast<IndexNode*>(en));

      case EN::ENUMERATE_COLLECTION:
        return handleEnumerateCollectionNode(
            static_cast<EnumerateCollectionNode*>(en));
    }
    return true;
  }
};

void arangodb::aql::useIndexForSortRule(Optimizer* opt, ExecutionPlan* plan,
                                        Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::SORT, true));

  for (auto const& n : nodes) {
    auto sortNode = static_cast<SortNode*>(n);

    SortToIndexNode finder(plan);
    sortNode->walk(&finder);

    if (finder._modified) {
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to remove filters which are covered by indexes
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeFiltersCoveredByIndexRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  std::unordered_set<ExecutionNode*> toUnlink;
  bool modified = false;
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));

  for (auto const& node : nodes) {
    auto fn = static_cast<FilterNode const*>(node);
    // find the node with the filter expression
    auto inVar = fn->getVariablesUsedHere();
    TRI_ASSERT(inVar.size() == 1);

    auto setter = plan->getVarSetBy(inVar[0]->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = static_cast<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    auto condition = std::make_unique<Condition>(plan->getAst());
    condition->andCombine(conditionNode);
    condition->normalize(plan);

    if (condition->root() == nullptr) {
      continue;
    }

    size_t const n = condition->root()->numMembers();

    if (n != 1) {
      // either no condition or multiple ORed conditions...
      continue;
    }

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::INDEX) {
        auto indexNode = static_cast<IndexNode const*>(current);

        // found an index node, now check if the expression is covered by the
        // index
        auto indexCondition = indexNode->condition();

        if (indexCondition != nullptr && !indexCondition->isEmpty()) {
          auto const& indexesUsed = indexNode->getIndexes();

          if (indexesUsed.size() == 1) {
            // single index. this is something that we can handle

            auto newNode = condition->removeIndexCondition(
                indexNode->outVariable(), indexCondition->root());

            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(setter);
              toUnlink.emplace(node);
              modified = true;
              handled = true;
            } else if (newNode != condition->root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan->getAst(), newNode);
              CalculationNode* cn =
                  new CalculationNode(plan, plan->nextId(), expr.get(),
                                      calculationNode->outVariable());
              expr.release();
              plan->registerNode(cn);
              plan->replaceNode(setter, cn);
              modified = true;
              handled = true;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT ||
          !current->hasDependency()) {
        break;
      }

      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief interchange adjacent EnumerateCollectionNodes in all possible ways
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::interchangeAdjacentEnumerationsRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(
      plan->findNodesOfType(EN::ENUMERATE_COLLECTION, true));

  std::unordered_set<ExecutionNode*> nodesSet;
  for (auto const& n : nodes) {
    TRI_ASSERT(nodesSet.find(n) == nodesSet.end());
    nodesSet.emplace(n);
  }

  std::vector<ExecutionNode*> nodesToPermute;
  std::vector<size_t> permTuple;
  std::vector<size_t> starts;

  // We use that the order of the nodes is such that a node B that is among the
  // recursive dependencies of a node A is later in the vector.
  for (auto const& n : nodes) {
    if (nodesSet.find(n) != nodesSet.end()) {
      std::vector<ExecutionNode*> nn{n};
      nodesSet.erase(n);

      // Now follow the dependencies as long as we see further such nodes:
      auto nwalker = n;

      while (true) {
        if (!nwalker->hasDependency()) {
          break;
        }

        auto dep = nwalker->getFirstDependency();

        if (dep->getType() != EN::ENUMERATE_COLLECTION) {
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
  }

  // Now we have collected all the runs of EnumerateCollectionNodes in the
  // plan, we need to compute all possible permutations of all of them,
  // independently. This is why we need to compute all permutation tuples.

  opt->addPlan(plan, rule, false);

  if (!starts.empty()) {
    NextPermutationTuple(permTuple, starts);  // will never return false

    do {
      // Clone the plan:
      auto newPlan = plan->clone();

      try {  // get rid of plan if any of this fails
        // Find the nodes in the new plan corresponding to the ones in the
        // old plan that we want to permute:
        std::vector<ExecutionNode*> newNodes;
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
          auto const& parents = newNodes[lowBound]->getParents();

          TRI_ASSERT(parents.size() == 1);
          auto parent = parents[0];  // needed for insertion later

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
        if (!opt->addPlan(newPlan, rule, true)) {
          // have enough plans. stop permutations
          break;
        }
      } catch (...) {
        delete newPlan;
        throw;
      }

    } while (NextPermutationTuple(permTuple, starts));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scatter operations in cluster
/// this rule inserts scatter, gather and remote nodes so operations on sharded
/// collections actually work
/// it will change plans in place
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::scatterInClusterRule(Optimizer* opt, ExecutionPlan* plan,
                                         Optimizer::Rule const* rule) {
  bool wasModified = false;

  if (arangodb::ServerState::instance()->isCoordinator()) {
    // find subqueries
    std::unordered_map<ExecutionNode*, ExecutionNode*> subqueries;

    for (auto& it : plan->findNodesOfType(ExecutionNode::SUBQUERY, true)) {
      subqueries.emplace(static_cast<SubqueryNode const*>(it)->getSubquery(),
                         it);
    }

    // we are a coordinator. now look in the plan for nodes of type
    // EnumerateCollectionNode, IndexNode and modification nodes
    std::vector<ExecutionNode::NodeType> const types = {
        ExecutionNode::ENUMERATE_COLLECTION, ExecutionNode::INDEX,
        ExecutionNode::INSERT, ExecutionNode::UPDATE, ExecutionNode::REPLACE,
        ExecutionNode::REMOVE, ExecutionNode::UPSERT};

    std::vector<ExecutionNode*> nodes(plan->findNodesOfType(types, true));

    for (auto& node : nodes) {
      // found a node we need to replace in the plan

      auto const& parents = node->getParents();
      auto const& deps = node->getDependencies();
      TRI_ASSERT(deps.size() == 1);

      // don't do this if we are already distributing!
      if (deps[0]->getType() == ExecutionNode::REMOTE &&
          deps[0]->getFirstDependency()->getType() ==
              ExecutionNode::DISTRIBUTE) {
        continue;
      }

      bool const isRootNode = plan->isRoot(node);
      plan->unlinkNode(node, true);

      auto const nodeType = node->getType();

      // extract database and collection from plan node
      TRI_vocbase_t* vocbase = nullptr;
      Collection const* collection = nullptr;

      if (nodeType == ExecutionNode::ENUMERATE_COLLECTION) {
        vocbase = static_cast<EnumerateCollectionNode*>(node)->vocbase();
        collection = static_cast<EnumerateCollectionNode*>(node)->collection();
      } else if (nodeType == ExecutionNode::INDEX) {
        vocbase = static_cast<IndexNode*>(node)->vocbase();
        collection = static_cast<IndexNode*>(node)->collection();
      } else if (nodeType == ExecutionNode::INSERT ||
                 nodeType == ExecutionNode::UPDATE ||
                 nodeType == ExecutionNode::REPLACE ||
                 nodeType == ExecutionNode::REMOVE ||
                 nodeType == ExecutionNode::UPSERT) {
        vocbase = static_cast<ModificationNode*>(node)->vocbase();
        collection = static_cast<ModificationNode*>(node)->collection();
        if (nodeType == ExecutionNode::REMOVE ||
            nodeType == ExecutionNode::UPDATE) {
          // Note that in the REPLACE or UPSERT case we are not getting here,
          // since
          // the distributeInClusterRule fires and a DistributionNode is
          // used.
          auto* modNode = static_cast<ModificationNode*>(node);
          modNode->getOptions().ignoreDocumentNotFound = true;
        }
      } else {
        TRI_ASSERT(false);
      }

      // insert a scatter node
      ExecutionNode* scatterNode =
          new ScatterNode(plan, plan->nextId(), vocbase, collection);
      plan->registerNode(scatterNode);
      scatterNode->addDependency(deps[0]);

      // insert a remote node
      ExecutionNode* remoteNode =
          new RemoteNode(plan, plan->nextId(), vocbase, collection, "", "", "");
      plan->registerNode(remoteNode);
      remoteNode->addDependency(scatterNode);

      // re-link with the remote node
      node->addDependency(remoteNode);

      // insert another remote node
      remoteNode =
          new RemoteNode(plan, plan->nextId(), vocbase, collection, "", "", "");
      plan->registerNode(remoteNode);
      remoteNode->addDependency(node);

      // insert a gather node
      ExecutionNode* gatherNode =
          new GatherNode(plan, plan->nextId(), vocbase, collection);
      plan->registerNode(gatherNode);
      gatherNode->addDependency(remoteNode);

      // and now link the gather node with the rest of the plan
      if (parents.size() == 1) {
        parents[0]->replaceDependency(deps[0], gatherNode);
      }

      // check if the node that we modified was at the end of a subquery
      auto it = subqueries.find(node);

      if (it != subqueries.end()) {
        static_cast<SubqueryNode*>((*it).second)->setSubquery(gatherNode, true);
      }

      if (isRootNode) {
        // if we replaced the root node, set a new root node
        plan->root(gatherNode);
      }
      wasModified = true;
    }
  }

  opt->addPlan(plan, rule, wasModified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief distribute operations in cluster
///
/// this rule inserts distribute, remote nodes so operations on sharded
/// collections actually work, this differs from scatterInCluster in that every
/// incoming row is only sent to one shard and not all as in scatterInCluster
///
/// it will change plans in place
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::distributeInClusterRule(Optimizer* opt, ExecutionPlan* plan,
                                            Optimizer::Rule const* rule) {
  bool wasModified = false;

  if (arangodb::ServerState::instance()->isCoordinator()) {
    // we are a coordinator, we replace the root if it is a modification node

    // only replace if it is the last node in the plan
    auto node = plan->root();
    TRI_ASSERT(node != nullptr);

    while (node != nullptr) {
      // loop until we find a modification node or the end of the plan
      auto nodeType = node->getType();

      if (nodeType == ExecutionNode::INSERT ||
          nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE ||
          nodeType == ExecutionNode::REPLACE ||
          nodeType == ExecutionNode::UPSERT) {
        // found a node!
        break;
      }

      if (!node->hasDependency()) {
        // reached the end
        opt->addPlan(plan, rule, wasModified);
        return;
      }

      node = node->getFirstDependency();
    }

    TRI_ASSERT(node != nullptr);

    if (node == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
    }

    ExecutionNode* originalParent = nullptr;
    {
      if (node->hasParent()) {
        auto const& parents = node->getParents();
        originalParent = parents[0];
        TRI_ASSERT(originalParent != nullptr);
        TRI_ASSERT(node != plan->root());
      } else {
        TRI_ASSERT(node == plan->root());
      }
    }

    // when we get here, we have found a matching data-modification node!
    auto const nodeType = node->getType();

    TRI_ASSERT(nodeType == ExecutionNode::INSERT ||
               nodeType == ExecutionNode::REMOVE ||
               nodeType == ExecutionNode::UPDATE ||
               nodeType == ExecutionNode::REPLACE ||
               nodeType == ExecutionNode::UPSERT);

    Collection const* collection =
        static_cast<ModificationNode*>(node)->collection();

    bool const defaultSharding = collection->usesDefaultSharding();

    if (nodeType == ExecutionNode::REMOVE ||
        nodeType == ExecutionNode::UPDATE) {
      if (!defaultSharding) {
        // We have to use a ScatterNode.
        opt->addPlan(plan, rule, wasModified);
        return;
      }
    }

    // In the INSERT and REPLACE cases we use a DistributeNode...

    TRI_ASSERT(node->hasDependency());
    auto const& deps = node->getDependencies();

    if (originalParent != nullptr) {
      originalParent->removeDependency(node);
      // unlink the node
      auto root = plan->root();
      plan->unlinkNode(node, true);
      plan->root(root, true);  // fix root node
    } else {
      // unlink the node
      plan->unlinkNode(node, true);
      plan->root(deps[0], true);  // fix root node
    }

    // extract database from plan node
    TRI_vocbase_t* vocbase = static_cast<ModificationNode*>(node)->vocbase();

    // insert a distribute node
    ExecutionNode* distNode = nullptr;
    Variable const* inputVariable;
    if (nodeType == ExecutionNode::INSERT ||
        nodeType == ExecutionNode::REMOVE) {
      TRI_ASSERT(node->getVariablesUsedHere().size() == 1);

      // in case of an INSERT, the DistributeNode is responsible for generating
      // keys
      // if none present
      bool const createKeys = (nodeType == ExecutionNode::INSERT);
      inputVariable = node->getVariablesUsedHere()[0];
      distNode = new DistributeNode(plan, plan->nextId(), vocbase, collection,
                                    inputVariable->id, createKeys, true);
    } else if (nodeType == ExecutionNode::REPLACE) {
      std::vector<Variable const*> v = node->getVariablesUsedHere();
      if (defaultSharding && v.size() > 1) {
        // We only look into _inKeyVariable
        inputVariable = v[1];
      } else {
        // We only look into _inDocVariable
        inputVariable = v[0];
      }
      distNode = new DistributeNode(plan, plan->nextId(), vocbase, collection,
                                    inputVariable->id, false, v.size() > 1);
    } else if (nodeType == ExecutionNode::UPDATE) {
      std::vector<Variable const*> v = node->getVariablesUsedHere();
      if (v.size() > 1) {
        // If there is a key variable:
        inputVariable = v[1];
        // This is the _inKeyVariable! This works, since we use a ScatterNode
        // for non-default-sharding attributes.
      } else {
        // was only UPDATE <doc> IN <collection>
        inputVariable = v[0];
      }
      distNode = new DistributeNode(plan, plan->nextId(), vocbase, collection,
                                    inputVariable->id, false, v.size() > 1);
    } else if (nodeType == ExecutionNode::UPSERT) {
      // an UPSERT nodes has two input variables!
      std::vector<Variable const*> v(node->getVariablesUsedHere());
      TRI_ASSERT(v.size() >= 2);

      distNode = new DistributeNode(plan, plan->nextId(), vocbase, collection,
                                    v[0]->id, v[2]->id, false, true);
    } else {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error");
    }

    TRI_ASSERT(distNode != nullptr);

    plan->registerNode(distNode);
    distNode->addDependency(deps[0]);

    // insert a remote node
    ExecutionNode* remoteNode =
        new RemoteNode(plan, plan->nextId(), vocbase, collection, "", "", "");
    plan->registerNode(remoteNode);
    remoteNode->addDependency(distNode);

    // re-link with the remote node
    node->addDependency(remoteNode);

    // insert another remote node
    remoteNode =
        new RemoteNode(plan, plan->nextId(), vocbase, collection, "", "", "");
    plan->registerNode(remoteNode);
    remoteNode->addDependency(node);

    // insert a gather node
    ExecutionNode* gatherNode =
        new GatherNode(plan, plan->nextId(), vocbase, collection);
    plan->registerNode(gatherNode);
    gatherNode->addDependency(remoteNode);

    if (originalParent != nullptr) {
      // we did not replace the root node
      originalParent->addDependency(gatherNode);
    } else {
      // we replaced the root node, set a new root node
      plan->root(gatherNode, true);
    }
    wasModified = true;
  }

  opt->addPlan(plan, rule, wasModified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move filters up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::distributeFilternCalcToClusterRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  bool modified = false;

  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::GATHER, true));

  for (auto& n : nodes) {
    auto const& remoteNodeList = n->getDependencies();
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (!n->hasParent()) {
      continue;
    }

    std::unordered_set<Variable const*> varsSetHere;
    auto parents = n->getParents();

    while (true) {
      bool stopSearching = false;
      auto inspectNode = parents[0];

      switch (inspectNode->getType()) {
        case EN::ENUMERATE_LIST:
        case EN::SINGLETON:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT: {
          for (auto& v : inspectNode->getVariablesSetHere()) {
            varsSetHere.emplace(v);
          }
          parents = inspectNode->getParents();
          continue;
        }

        case EN::COLLECT:
        case EN::SUBQUERY:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::ILLEGAL:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::SORT:
        case EN::INDEX:
        case EN::ENUMERATE_COLLECTION:
        case EN::TRAVERSAL:
          // do break
          stopSearching = true;
          break;

        case EN::CALCULATION: {
          auto calc = static_cast<CalculationNode const*>(inspectNode);
          // check if the expression can be executed on a DB server safely
          if (!calc->expression()->canRunOnDBServer()) {
            stopSearching = true;
            break;
          }
          // intentionally fall through here
        }
        case EN::FILTER:
          for (auto& v : inspectNode->getVariablesUsedHere()) {
            if (varsSetHere.find(v) != varsSetHere.end()) {
              // do not move over the definition of variables that we need
              stopSearching = true;
              break;
            }
          }

          if (!stopSearching) {
            // remember our cursor...
            parents = inspectNode->getParents();
            // then unlink the filter/calculator from the plan
            plan->unlinkNode(inspectNode);
            // and re-insert into plan in front of the remoteNode
            plan->insertDependency(rn, inspectNode);

            modified = true;
            // ready to rumble!
          }
          break;
      }

      if (stopSearching) {
        break;
      }
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move sorts up into the cluster distribution part of the plan
/// this rule modifies the plan in place
/// sorts are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
///
/// filters are not pushed beyond limits
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::distributeSortToClusterRule(Optimizer* opt,
                                                ExecutionPlan* plan,
                                                Optimizer::Rule const* rule) {
  bool modified = false;

  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::GATHER, true));

  for (auto& n : nodes) {
    auto const& remoteNodeList = n->getDependencies();
    auto gatherNode = static_cast<GatherNode*>(n);
    TRI_ASSERT(remoteNodeList.size() > 0);
    auto rn = remoteNodeList[0];

    if (!n->hasParent()) {
      continue;
    }

    auto parents = n->getParents();

    while (1) {
      bool stopSearching = false;

      auto inspectNode = parents[0];

      switch (inspectNode->getType()) {
        case EN::ENUMERATE_LIST:
        case EN::SINGLETON:
        case EN::COLLECT:
        case EN::INSERT:
        case EN::REMOVE:
        case EN::REPLACE:
        case EN::UPDATE:
        case EN::UPSERT:
        case EN::CALCULATION:
        case EN::FILTER:
        case EN::SUBQUERY:
        case EN::RETURN:
        case EN::NORESULTS:
        case EN::SCATTER:
        case EN::DISTRIBUTE:
        case EN::GATHER:
        case EN::ILLEGAL:
        case EN::REMOTE:
        case EN::LIMIT:
        case EN::INDEX:
        case EN::TRAVERSAL:
        case EN::ENUMERATE_COLLECTION:
          // For all these, we do not want to pull a SortNode further down
          // out to the DBservers, note that potential FilterNodes and
          // CalculationNodes that can be moved to the DBservers have
          // already been moved over by the distribute-filtercalc-to-cluster
          // rule which is done first.
          stopSearching = true;
          break;
        case EN::SORT:
          auto thisSortNode = static_cast<SortNode*>(inspectNode);

          // remember our cursor...
          parents = inspectNode->getParents();
          // then unlink the filter/calculator from the plan
          plan->unlinkNode(inspectNode);
          // and re-insert into plan in front of the remoteNode
          plan->insertDependency(rn, inspectNode);
          gatherNode->setElements(thisSortNode->getElements());
          modified = true;
          // ready to rumble!
      }

      if (stopSearching) {
        break;
      }
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeUnnecessaryRemoteScatterRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::REMOTE, true));
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    // check if the remote node is preceeded by a scatter node and any number of
    // calculation and singleton nodes. if yes, remove remote and scatter
    if (!n->hasDependency()) {
      continue;
    }

    auto const dep = n->getFirstDependency();
    if (dep->getType() != EN::SCATTER) {
      continue;
    }

    bool canOptimize = true;
    auto node = dep;
    while (node != nullptr) {
      auto const& d = node->getDependencies();

      if (d.size() != 1) {
        break;
      }

      node = d[0];
      if (node->getType() != EN::SINGLETON &&
          node->getType() != EN::CALCULATION) {
        // found some other node type...
        // this disqualifies the optimization
        canOptimize = false;
        break;
      }

      if (node->getType() == EN::CALCULATION) {
        auto calc = static_cast<CalculationNode const*>(node);
        // check if the expression can be executed on a DB server safely
        if (!calc->expression()->canRunOnDBServer()) {
          canOptimize = false;
          break;
        }
      }
    }

    if (canOptimize) {
      toUnlink.emplace(n);
      toUnlink.emplace(dep);
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(plan, rule, !toUnlink.empty());
}

////////////////////////////////////////////////////////////////////////////////
/// WalkerWorker for undistributeRemoveAfterEnumColl
////////////////////////////////////////////////////////////////////////////////

class RemoveToEnumCollFinder final : public WalkerWorker<ExecutionNode> {
  ExecutionPlan* _plan;
  std::unordered_set<ExecutionNode*>& _toUnlink;
  bool _remove;
  bool _scatter;
  bool _gather;
  EnumerateCollectionNode* _enumColl;
  ExecutionNode* _setter;
  const Variable* _variable;
  ExecutionNode* _lastNode;

 public:
  RemoveToEnumCollFinder(ExecutionPlan* plan,
                         std::unordered_set<ExecutionNode*>& toUnlink)
      : _plan(plan),
        _toUnlink(toUnlink),
        _remove(false),
        _scatter(false),
        _gather(false),
        _enumColl(nullptr),
        _setter(nullptr),
        _variable(nullptr),
        _lastNode(nullptr){};

  ~RemoveToEnumCollFinder() {}

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::REMOVE: {
        if (_remove) {
          break;
        }

        // find the variable we are removing . . .
        auto rn = static_cast<RemoveNode*>(en);
        auto varsToRemove = rn->getVariablesUsedHere();

        // remove nodes always have one input variable
        TRI_ASSERT(varsToRemove.size() == 1);

        _setter = _plan->getVarSetBy(varsToRemove[0]->id);
        TRI_ASSERT(_setter != nullptr);
        auto enumColl = _setter;

        if (_setter->getType() == EN::CALCULATION) {
          // this should be an attribute access for _key
          auto cn = static_cast<CalculationNode*>(_setter);
          if (!cn->expression()->isAttributeAccess()) {
            break;  // abort . . .
          }
          // check the variable is the same as the remove variable
          auto vars = cn->getVariablesSetHere();
          if (vars.size() != 1 || vars[0]->id != varsToRemove[0]->id) {
            break;  // abort . . .
          }
          // check the remove node's collection is sharded over _key
          std::vector<std::string> shardKeys = rn->collection()->shardKeys();
          if (shardKeys.size() != 1 || shardKeys[0] != TRI_VOC_ATTRIBUTE_KEY) {
            break;  // abort . . .
          }

          // set the varsToRemove to the variable in the expression of this
          // node and also define enumColl
          varsToRemove = cn->getVariablesUsedHere();
          TRI_ASSERT(varsToRemove.size() == 1);
          enumColl = _plan->getVarSetBy(varsToRemove[0]->id);
          TRI_ASSERT(_setter != nullptr);
        }

        if (enumColl->getType() != EN::ENUMERATE_COLLECTION) {
          break;  // abort . . .
        }

        _enumColl = static_cast<EnumerateCollectionNode*>(enumColl);

        if (_enumColl->collection() != rn->collection()) {
          break;  // abort . . .
        }

        _variable = varsToRemove[0];  // the variable we'll remove
        _remove = true;
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::REMOTE: {
        _toUnlink.emplace(en);
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::DISTRIBUTE:
      case EN::SCATTER: {
        if (_scatter) {  // met more than one scatter node
          break;         // abort . . .
        }
        _scatter = true;
        _toUnlink.emplace(en);
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::GATHER: {
        if (_gather) {  // met more than one gather node
          break;        // abort . . .
        }
        _gather = true;
        _toUnlink.emplace(en);
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::FILTER: {
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::CALCULATION: {
        TRI_ASSERT(_setter != nullptr);
        if (_setter->getType() == EN::CALCULATION &&
            _setter->id() == en->id()) {
          _lastNode = en;
          return false;  // continue . . .
        }
        if (_lastNode == nullptr || _lastNode->getType() != EN::FILTER) {
          // doesn't match the last filter node
          break;  // abort . . .
        }
        auto cn = static_cast<CalculationNode*>(en);
        auto fn = static_cast<FilterNode*>(_lastNode);

        // check these are a Calc-Filter pair
        if (cn->getVariablesSetHere()[0]->id !=
            fn->getVariablesUsedHere()[0]->id) {
          break;  // abort . . .
        }

        // check that we are filtering/calculating something with the variable
        // we are to remove
        auto varsUsedHere = cn->getVariablesUsedHere();

        if (varsUsedHere.size() != 1) {
          break;  // abort . . .
        }
        if (varsUsedHere[0]->id != _variable->id) {
          break;
        }
        _lastNode = en;
        return false;  // continue . . .
      }
      case EN::ENUMERATE_COLLECTION: {
        // check that we are enumerating the variable we are to remove
        // and that we have already seen a remove node
        TRI_ASSERT(_enumColl != nullptr);
        if (en->id() != _enumColl->id()) {
          break;
        }
        return true;  // reached the end!
      }
      case EN::SINGLETON:
      case EN::ENUMERATE_LIST:
      case EN::SUBQUERY:
      case EN::COLLECT:
      case EN::INSERT:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::ILLEGAL:
      case EN::LIMIT:
      case EN::SORT:
      case EN::TRAVERSAL:
      case EN::INDEX: {
        // if we meet any of the above, then we abort . . .
      }
    }
    _toUnlink.clear();
    return true;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief recognizes that a RemoveNode can be moved to the shards.
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::undistributeRemoveAfterEnumCollRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::REMOVE, true));
  std::unordered_set<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    RemoveToEnumCollFinder finder(plan, toUnlink);
    n->walk(&finder);
  }

  bool modified = false;
  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    modified = true;
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief auxilliary struct for finding common nodes in OR conditions
////////////////////////////////////////////////////////////////////////////////

struct CommonNodeFinder {
  std::vector<AstNode const*> possibleNodes;

  bool find(AstNode const* node, AstNodeType condition,
            AstNode const*& commonNode, std::string& commonName) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return (find(node->getMember(0), condition, commonNode, commonName) &&
              find(node->getMember(1), condition, commonNode, commonName));
    }

    if (node->type == NODE_TYPE_VALUE) {
      possibleNodes.clear();
      return true;
    }

    if (node->type == condition ||
        (condition != NODE_TYPE_OPERATOR_BINARY_EQ &&
         (node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_IN))) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      bool const isIn =
          (node->type == NODE_TYPE_OPERATOR_BINARY_IN && rhs->isArray());

      if (node->type == NODE_TYPE_OPERATOR_BINARY_IN &&
          rhs->type == NODE_TYPE_EXPANSION) {
        // ooh, cannot optimize this (yet)
        possibleNodes.clear();
        return false;
      }

      if (!isIn && lhs->isConstant()) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->isConstant()) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->type == NODE_TYPE_FCALL || rhs->type == NODE_TYPE_FCALL_USER ||
          rhs->type == NODE_TYPE_REFERENCE) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn &&
          (lhs->type == NODE_TYPE_FCALL || lhs->type == NODE_TYPE_FCALL_USER ||
           lhs->type == NODE_TYPE_REFERENCE)) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn && (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                    lhs->type == NODE_TYPE_INDEXED_ACCESS)) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (lhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          // don't return, must consider the other side of the condition
        } else {
          possibleNodes.emplace_back(lhs);
        }
      }
      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type == NODE_TYPE_INDEXED_ACCESS) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (rhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          return false;
        } else {
          possibleNodes.emplace_back(rhs);
          return true;
        }
      }
    }
    possibleNodes.clear();
    return (!commonName.empty());
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief auxilliary struct for the OR-to-IN conversion
////////////////////////////////////////////////////////////////////////////////

struct OrSimplifier {
  Ast* ast;

  explicit OrSimplifier(Ast* ast) : ast(ast) {}
  
  std::string stringifyNode(AstNode const* node) const {
    try {
      return node->toString();
    }
    catch (...) {
    }
    return "";
  }

  bool qualifies(AstNode const* node, std::string& attributeName) const {
    if (node->isConstant()) {
      return false;
    }

    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
        node->type == NODE_TYPE_INDEXED_ACCESS ||
        node->type == NODE_TYPE_REFERENCE) {
      attributeName = stringifyNode(node);
      return true;
    }

    return false;
  }

  bool detect(AstNode const* node, bool preferRight, std::string& attributeName, AstNode const*& attr, AstNode const*& value) const {
    attributeName.clear();

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      if (!preferRight && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic() && !rhs->canThrow()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }

      if (qualifies(rhs, attributeName)) {
        if (lhs->isDeterministic() && !lhs->canThrow()) {
          attr = rhs;
          value = lhs;
          return true;
        }
      }
      // fallthrough intentional
    }
    else if (node->type == NODE_TYPE_OPERATOR_BINARY_IN) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      if (rhs->isArray() && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic() && !rhs->canThrow()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }
      // fallthrough intentional
    }

    return false;
  }

  AstNode* buildValues(AstNode const* attr, AstNode const* lhs, bool leftIsArray, AstNode const* rhs, bool rightIsArray) const {
    auto values = ast->createNodeArray();
    if (leftIsArray) {
      size_t const n = lhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(lhs->getMemberUnchecked(i));
      }
    }
    else {
      values->addMember(lhs);
    }
    
    if (rightIsArray) {
      size_t const n = rhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(rhs->getMemberUnchecked(i));
      }
    }
    else {
      values->addMember(rhs);
    }

    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, attr, values);
  }

  AstNode* simplify(AstNode const* node) const {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      
      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);
      
      if (lhs != lhsNew || rhs != rhsNew) {
        // create a modified node
        node = ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      if ((lhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ || lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN) && 
          (rhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ || rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN)) {
        std::string leftName;
        std::string rightName;
        AstNode const* leftAttr = nullptr;
        AstNode const* rightAttr = nullptr;
        AstNode const* leftValue = nullptr;
        AstNode const* rightValue = nullptr;

        for (size_t i = 0; i < 4; ++i) {
          if (detect(lhsNew, i >= 2, leftName, leftAttr, leftValue) && 
              detect(rhsNew, i % 2 == 0, rightName, rightAttr, rightValue) && 
              leftName == rightName) {
            return buildValues(leftAttr, leftValue, lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN, rightValue, rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN);
          }
        }
      }

      // return node as is
      return const_cast<AstNode*>(node);
    }
    
    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);
      
      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);

      if (lhs != lhsNew || rhs != rhsNew) {
        // return a modified node
        return ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      // fallthrough intentional
    }
      
    return const_cast<AstNode*>(node);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief this rule replaces expressions of the type:
///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to the
//  same (single) attribute.
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::replaceOrWithInRule(Optimizer* opt, ExecutionPlan* plan,
                                        Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = static_cast<FilterNode*>(n);
    auto inVar = fn->getVariablesUsedHere();

    auto cn = static_cast<CalculationNode*>(dep);
    auto outVar = cn->getVariablesSetHere();

    if (outVar.size() != 1 || outVar[0]->id != inVar[0]->id) {
      continue;
    }
    
    auto root = cn->expression()->node();

    OrSimplifier simplifier(plan->getAst());
    auto newRoot = simplifier.simplify(root);

    if (newRoot != root) {
      ExecutionNode* newNode = nullptr;
      Expression* expr = new Expression(plan->getAst(), newRoot);
      
      try {
        TRI_IF_FAILURE("OptimizerRules::replaceOrWithInRuleOom") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        newNode = new CalculationNode(plan, plan->nextId(), expr, outVar[0]);
      } catch (...) {
        delete expr;
        throw;
      }

      plan->registerNode(newNode);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

struct RemoveRedundantOr {
  AstNode const* bestValue = nullptr;
  AstNodeType comparison;
  bool inclusive;
  bool isComparisonSet = false;
  CommonNodeFinder finder;
  AstNode const* commonNode = nullptr;
  std::string commonName;

  AstNode* createReplacementNode(Ast* ast) {
    TRI_ASSERT(commonNode != nullptr);
    TRI_ASSERT(bestValue != nullptr);
    TRI_ASSERT(isComparisonSet == true);
    return ast->createNodeBinaryOperator(comparison, commonNode->clone(ast),
                                         bestValue);
  }

  bool isInclusiveBound(AstNodeType type) {
    return (type == NODE_TYPE_OPERATOR_BINARY_GE ||
            type == NODE_TYPE_OPERATOR_BINARY_LE);
  }

  int isCompatibleBound(AstNodeType type, AstNode const* value) {
    if ((comparison == NODE_TYPE_OPERATOR_BINARY_LE ||
         comparison == NODE_TYPE_OPERATOR_BINARY_LT) &&
        (type == NODE_TYPE_OPERATOR_BINARY_LE ||
         type == NODE_TYPE_OPERATOR_BINARY_LT)) {
      return -1;  // high bound
    } else if ((comparison == NODE_TYPE_OPERATOR_BINARY_GE ||
                comparison == NODE_TYPE_OPERATOR_BINARY_GT) &&
               (type == NODE_TYPE_OPERATOR_BINARY_GE ||
                type == NODE_TYPE_OPERATOR_BINARY_GT)) {
      return 1;  // low bound
    }
    return 0;  // incompatible bounds
  }

  // returns false if the existing value is better and true if the input value
  // is
  // better
  bool compareBounds(AstNodeType type, AstNode const* value, int lowhigh) {
    int cmp = CompareAstNodes(bestValue, value, true);

    if (cmp == 0 && (isInclusiveBound(comparison) != isInclusiveBound(type))) {
      return (isInclusiveBound(type) ? true : false);
    }
    return (cmp * lowhigh == 1);
  }

  bool hasRedundantCondition(AstNode const* node) {
    if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_LT, commonNode,
                    commonName)) {
      return hasRedundantConditionWalker(node);
    }
    return false;
  }

  bool hasRedundantConditionWalker(AstNode const* node) {
    AstNodeType type = node->type;

    if (type == NODE_TYPE_OPERATOR_BINARY_OR) {
      return (hasRedundantConditionWalker(node->getMember(0)) &&
              hasRedundantConditionWalker(node->getMember(1)));
    }

    if (type == NODE_TYPE_OPERATOR_BINARY_LE ||
        type == NODE_TYPE_OPERATOR_BINARY_LT ||
        type == NODE_TYPE_OPERATOR_BINARY_GE ||
        type == NODE_TYPE_OPERATOR_BINARY_GT) {
      auto lhs = node->getMember(0);
      auto rhs = node->getMember(1);

      if (hasRedundantConditionWalker(rhs) &&
          !hasRedundantConditionWalker(lhs) && lhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = Ast::ReverseOperator(type);
          bestValue = lhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(Ast::ReverseOperator(type), lhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, lhs, lowhigh)) {
          comparison = Ast::ReverseOperator(type);
          bestValue = lhs;
        }
        return true;
      }
      if (hasRedundantConditionWalker(lhs) &&
          !hasRedundantConditionWalker(rhs) && rhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = type;
          bestValue = rhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(type, rhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, rhs, lowhigh)) {
          comparison = type;
          bestValue = rhs;
        }
        return true;
      }
      // if hasRedundantConditionWalker(lhs) and
      // hasRedundantConditionWalker(rhs), then one of the conditions in the OR
      // statement is of the form x == x fall-through intentional
    } else if (type == NODE_TYPE_REFERENCE ||
               type == NODE_TYPE_ATTRIBUTE_ACCESS ||
               type == NODE_TYPE_INDEXED_ACCESS) {
      // get a string representation of the node for comparisons
      return (node->toString() == commonName);
    }

    return false;
  }
};

void arangodb::aql::removeRedundantOrRule(Optimizer* opt, ExecutionPlan* plan,
                                          Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::FILTER, true));

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = static_cast<FilterNode*>(n);
    auto inVar = fn->getVariablesUsedHere();

    auto cn = static_cast<CalculationNode*>(dep);
    auto outVar = cn->getVariablesSetHere();

    if (outVar.size() != 1 || outVar[0]->id != inVar[0]->id) {
      continue;
    }
    if (cn->expression()->node()->type != NODE_TYPE_OPERATOR_BINARY_OR) {
      continue;
    }

    RemoveRedundantOr remover;
    if (remover.hasRedundantCondition(cn->expression()->node())) {
      Expression* expr = nullptr;
      ExecutionNode* newNode = nullptr;
      auto astNode = remover.createReplacementNode(plan->getAst());

      expr = new Expression(plan->getAst(), astNode);

      try {
        newNode = new CalculationNode(plan, plan->nextId(), expr, outVar[0]);
      } catch (...) {
        delete expr;
        throw;
      }

      plan->registerNode(newNode);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove $OLD and $NEW variables from data-modification statements
/// if not required
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::removeDataModificationOutVariablesRule(
    Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule) {
  bool modified = false;
  std::vector<ExecutionNode::NodeType> const types = {
      EN::REMOVE, EN::INSERT, EN::UPDATE, EN::REPLACE, EN::UPSERT};

  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(types, true));

  for (auto const& n : nodes) {
    auto node = static_cast<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    auto varsUsedLater = n->getVarsUsedLater();
    if (varsUsedLater.find(node->getOutVariableOld()) == varsUsedLater.end()) {
      // "$OLD" is not used later
      node->clearOutVariableOld();
      modified = true;
    }

    if (varsUsedLater.find(node->getOutVariableNew()) == varsUsedLater.end()) {
      // "$NEW" is not used later
      node->clearOutVariableNew();
      modified = true;
    }
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief patch UPDATE statement on single collection that iterates over the
/// entire collection to operate in batches
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::patchUpdateStatementsRule(Optimizer* opt,
                                              ExecutionPlan* plan,
                                              Optimizer::Rule const* rule) {
  bool modified = false;

  // not need to dive into subqueries here, as UPDATE needs to be on the top
  // level
  std::vector<ExecutionNode*> nodes(plan->findNodesOfType(EN::UPDATE, false));

  for (auto const& n : nodes) {
    // we should only get through here a single time
    auto node = static_cast<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    auto& options = node->getOptions();
    if (!options.readCompleteInput) {
      // already ok
      continue;
    }

    auto const collection = node->collection();

    auto dep = n->getFirstDependency();

    while (dep != nullptr) {
      auto const type = dep->getType();

      if (type == EN::ENUMERATE_LIST || type == EN::INDEX ||
          type == EN::SUBQUERY) {
        // not suitable
        modified = false;
        break;
      }

      if (type == EN::ENUMERATE_COLLECTION) {
        auto collectionNode = static_cast<EnumerateCollectionNode const*>(dep);

        if (collectionNode->collection() != collection) {
          // different collection, not suitable
          modified = false;
          break;
        } else {
          modified = true;
        }
      }

      if (type == EN::TRAVERSAL) {
        // unclear what will be read by the traversal
        modified = false;
        break;
      }

      dep = dep->getFirstDependency();
    }

    if (modified) {
      options.readCompleteInput = false;
    }
  }

  // always re-add the original plan, be it modified or not
  // only a flag in the plan will be modified
  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merges filter nodes into graph traversal nodes
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::mergeFilterIntoTraversalRule(Optimizer* opt,
                                                 ExecutionPlan* plan,
                                                 Optimizer::Rule const* rule) {
  std::vector<ExecutionNode*> tNodes(
      plan->findNodesOfType(EN::TRAVERSAL, true));

  if (tNodes.empty()) {
    opt->addPlan(plan, rule, false);
    return;
  }

  // These are all the end nodes where we start
  std::vector<ExecutionNode*> nodes(plan->findEndNodes(true));

  bool modified = false;
  for (auto const& n : nodes) {
    TraversalConditionFinder finder(plan, &modified);
    n->walk(&finder);
  }

  opt->addPlan(plan, rule, modified);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pulls out simple subqueries and merges them with the level above
///
/// For example, if we have the input query
///
/// FOR x IN (
///     FOR y IN collection FILTER y.value >= 5 RETURN y.test
///   ) 
///   RETURN x.a
///
/// then this rule will transform it into:
/// 
/// FOR tmp IN collection
///   FILTER tmp.value >= 5 
///   LET x = tmp.test
///   RETURN x.a
////////////////////////////////////////////////////////////////////////////////

void arangodb::aql::inlineSubqueriesRule(Optimizer* opt, 
                                         ExecutionPlan* plan, 
                                         Optimizer::Rule const* rule) {

  std::vector<ExecutionNode*> nodes(
      plan->findNodesOfType(EN::SUBQUERY, true));

  if (nodes.empty()) {
    opt->addPlan(plan, rule, false);
    return;
  }

  bool modified = false;

  for (auto const& n : nodes) {
    auto subqueryNode = static_cast<SubqueryNode const*>(n);
  
    if (subqueryNode->isModificationQuery()) {
      // can't modify modifying subqueries
      continue;
    }

    Variable const* out = subqueryNode->outVariable();
    TRI_ASSERT(out != nullptr);

    auto current = n;
    // now check where the subquery is used
    while (current->hasParent()) {
      if (current->getType() == EN::ENUMERATE_LIST) {
        // we're only interested in FOR loops...
        auto listNode = static_cast<EnumerateListNode*>(current);

        // ...that use our subquery as its input
        if (listNode->inVariable() == out) {
          // bingo!
          auto queryVariables = plan->getAst()->variables();
          std::vector<ExecutionNode*> subNodes(subqueryNode->getSubquery()->getDependencyChain(true));

          TRI_ASSERT(! subNodes.empty());
          auto returnNode = static_cast<ReturnNode*>(subNodes[0]);
          TRI_ASSERT(returnNode->getType() == EN::RETURN);

          modified = true;
          auto previous = n->getFirstDependency();
          auto insert = n->getFirstParent();
          TRI_ASSERT(insert != nullptr);
  
          // unlink the original SubqueryNode
          plan->unlinkNode(n, false);
           
          for (auto& it : subNodes) {
            // first unlink them all
            plan->unlinkNode(it, true);

            if (it->getType() == EN::SINGLETON) {
              // reached the singleton node already. that means we can stop
              break;
            }

            // and now insert them one level up
            if (it != returnNode) {
              // we skip over the subquery's return node. we don't need it anymore
              insert->removeDependencies();
              insert->addDependency(it);
              insert = it;

              // additionally rename the variables from the subquery so they cannot
              // conflict with the ones from the top query
              for (auto const& variable : it->getVariablesSetHere()) {
                queryVariables->renameVariable(variable->id);
              }
            }
          } 

          // link the top node in the subquery with the original plan
          if (previous != nullptr) {
            insert->addDependency(previous);
          }

          // remove the list node from the plan
          plan->unlinkNode(listNode, false);

          queryVariables->renameVariable(returnNode->inVariable()->id, listNode->outVariable()->name);

          // finally replace the variables
          std::unordered_map<VariableId, Variable const*> replacements;
          replacements.emplace(listNode->outVariable()->id, returnNode->inVariable());
          RedundantCalculationsReplacer finder(replacements);
          plan->root()->walk(&finder);
    
          current = nullptr;
        }
      }

      if (current == nullptr) {
        break;
      }

      auto const& parents = current->getParents();
      current = parents[0];
    }
  }

  opt->addPlan(plan, rule, modified);
}

