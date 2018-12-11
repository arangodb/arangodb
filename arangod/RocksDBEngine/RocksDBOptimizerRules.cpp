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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBOptimizerRules.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/SortNode.h"
#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;
  
namespace {

std::vector<ExecutionNode::NodeType> const reduceExtractionToProjectionTypes = {ExecutionNode::ENUMERATE_COLLECTION, ExecutionNode::INDEX}; 

} // namespace

void RocksDBOptimizerRules::registerResources() {
  // simplify an EnumerationCollectionNode that fetches an entire document to a projection of this document
  OptimizerRulesFeature::registerRule("reduce-extraction-to-projection", reduceExtractionToProjectionRule, 
               OptimizerRule::reduceExtractionToProjectionRule, false, true);
  // remove SORT RAND() LIMIT 1 if appropriate
  OptimizerRulesFeature::registerRule("remove-sort-rand-limit-1", removeSortRandRule,
                                      OptimizerRule::removeSortRandRule, false, true);
}

// simplify an EnumerationCollectionNode that fetches an entire document to a projection of this document
void RocksDBOptimizerRules::reduceExtractionToProjectionRule(Optimizer* opt, 
                                                             std::unique_ptr<ExecutionPlan> plan, 
                                                             OptimizerRule const* rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  
  plan->findNodesOfType(nodes, ::reduceExtractionToProjectionTypes, true);

  bool modified = false;
  std::unordered_set<Variable const*> vars;
  std::unordered_set<std::string> attributes;

  for (auto& n : nodes) {
    bool stop = false;
    bool optimize = false;
    attributes.clear();
    DocumentProducingNode* e = dynamic_cast<DocumentProducingNode*>(n);
    if (e == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot convert node to DocumentProducingNode");
    }
    Variable const* v = e->outVariable();

    ExecutionNode* current = n->getFirstParent();
    while (current != nullptr) {
      bool doRegularCheck = false;

      if (current->getType() == EN::REMOVE) {
        RemoveNode const* removeNode = ExecutionNode::castTo<RemoveNode const*>(current);
        if (removeNode->inVariable() == v) { 
          // FOR doc IN collection REMOVE doc IN ...
          attributes.emplace(StaticStrings::KeyString);
          optimize = true;
        } else {
          doRegularCheck = true;
        }
      } else if (current->getType() == EN::UPDATE || current->getType() == EN::REPLACE) {
        UpdateReplaceNode const* modificationNode = ExecutionNode::castTo<UpdateReplaceNode const*>(current);

        if (modificationNode->inKeyVariable() == v && 
            modificationNode->inDocVariable() != v) {
          // FOR doc IN collection UPDATE/REPLACE doc IN ...
          attributes.emplace(StaticStrings::KeyString);
          optimize = true;
        } else {
          doRegularCheck = true;
        }
      } else if (current->getType() == EN::CALCULATION) {
        Expression* exp = ExecutionNode::castTo<CalculationNode*>(current)->expression();

        if (exp != nullptr && exp->node() != nullptr) {
          AstNode const* node = exp->node();
          vars.clear();
          current->getVariablesUsedHere(vars);
            
          if (vars.find(v) != vars.end()) {
            if (!Ast::getReferencedAttributes(node, v, attributes)) {
              stop = true;
              break;
            }
            optimize = true;
          }
        }
      } else if (current->getType() == EN::GATHER) {
        // compare sort attributes of GatherNode
        auto gn = ExecutionNode::castTo<GatherNode*>(current);
        for (auto const& it : gn->elements()) {
          if (it.var == v) {
            if (it.attributePath.empty()) {
              // sort of GatherNode refers to the entire document, not to an
              // attribute of the document
              stop = true;
              break;
            }
            // insert 0th level of attribute name into the set of attributes
            // that we need for our projection
            attributes.emplace(it.attributePath[0]); 
          }
        }
      } else {
        // all other node types mandate a check
        doRegularCheck = true;
      }

      if (doRegularCheck) {
        vars.clear();
        current->getVariablesUsedHere(vars);

        if (vars.find(v) != vars.end()) {
          // original variable is still used here
          stop = true;
          break;
        }
      }

      if (stop) {
        break;
      }

      current = current->getFirstParent();
    }

    // projections are currently limited (arbitrarily to 5 attributes)
    if (optimize && !stop && !attributes.empty() && attributes.size() <= 5) {
      std::vector<std::string> r;
      for (auto& it : attributes) {
        r.emplace_back(std::move(it));
      }
      // store projections in DocumentProducingNode
      e->projections(std::move(r));

      if (n->getType() == ExecutionNode::INDEX) {
        // need to update _indexCoversProjections value in an IndexNode
        ExecutionNode::castTo<IndexNode*>(n)->initIndexCoversProjections();
      }
      
      modified = true;
    }
  }
    
  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove SORT RAND() if appropriate
void RocksDBOptimizerRules::removeSortRandRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                                               OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::SORT, true);
  
  bool modified = false;
  
  for (auto const& n : nodes) {
    auto node = ExecutionNode::castTo<SortNode*>(n);
    auto const& elements = node->elements();
    if (elements.size() != 1) {
      // we're looking for "SORT RAND()", which has just one sort criterion
      continue;
    }
    
    auto const variable = elements[0].var;
    TRI_ASSERT(variable != nullptr);
    
    auto setter = plan->getVarSetBy(variable->id);
    
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }
    
    auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
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
    if (func->name != "RAND" || funcNode->numMembers() != 1 ||
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
      switch (current->getType()) {
        case EN::SORT:
        case EN::COLLECT:
        case EN::FILTER:
        case EN::SUBQUERY:
        case EN::ENUMERATE_LIST:
        case EN::TRAVERSAL:
        case EN::SHORTEST_PATH:
        case EN::INDEX:
#ifdef USE_IRESEARCH
        case EN::ENUMERATE_IRESEARCH_VIEW:
#endif
        {
          // if we found another SortNode, a CollectNode, FilterNode, a
          // SubqueryNode, an EnumerateListNode, a TraversalNode or an IndexNode
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
    
    if (collectionNode == nullptr || !node->hasParent()) {
      continue; // skip
    }
    
    current = node->getFirstParent();
    bool valid = false;
    if (current->getType() == EN::LIMIT) {
      LimitNode* ln = ExecutionNode::castTo<LimitNode*>(current);
      if (ln->limit() == 1 && ln->offset() == 0) {
        valid = true;
      }
    }
    
    if (valid) {
      // we found a node to modify!
      TRI_ASSERT(collectionNode->getType() == EN::ENUMERATE_COLLECTION);
      // set the random iteration flag for the EnumerateCollectionNode
      ExecutionNode::castTo<EnumerateCollectionNode*>(collectionNode)->setRandom();
      
      // remove the SortNode and the CalculationNode
      plan->unlinkNode(n);
      plan->unlinkNode(setter);
      modified = true;
    }
  }
  
  opt->addPlan(std::move(plan), rule, modified);
}
