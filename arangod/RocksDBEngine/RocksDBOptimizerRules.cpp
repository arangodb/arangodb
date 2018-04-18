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
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/SortNode.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

void RocksDBOptimizerRules::registerResources() {
  OptimizerRulesFeature::registerRule("reduce-extraction-to-projection", reduceExtractionToProjectionRule, 
               OptimizerRule::reduceExtractionToProjectionRule_pass10, false, true);
}

// simplify an EnumerationCollectionNode that fetches an entire document to a projection of this document
void RocksDBOptimizerRules::reduceExtractionToProjectionRule(Optimizer* opt, 
                                                             std::unique_ptr<ExecutionPlan> plan, 
                                                             OptimizerRule const* rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  
  std::vector<ExecutionNode::NodeType> const types = {ExecutionNode::ENUMERATE_COLLECTION, ExecutionNode::INDEX}; 
  plan->findNodesOfType(nodes, types, true);

  bool modified = false;
  std::unordered_set<Variable const*> vars;
  std::unordered_set<std::string> attributes;

  for (auto const& n : nodes) {
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
      if (current->getType() == EN::CALCULATION) {
        Expression* exp = static_cast<CalculationNode*>(current)->expression();

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
        auto gn = static_cast<GatherNode*>(current);
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
        vars.clear();
        current->getVariablesUsedHere(vars);

        if (vars.find(v) != vars.end()) {
          // original variable is still used here
          stop = true;
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
        static_cast<IndexNode*>(n)->initIndexCoversProjections();
      }
      
      modified = true;
    }
  }
    
  opt->addPlan(std::move(plan), rule, modified);
}
