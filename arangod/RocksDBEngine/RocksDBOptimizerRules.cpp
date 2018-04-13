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

class AttributeAccessReplacer final : public WalkerWorker<ExecutionNode> {
 public:
  AttributeAccessReplacer(Variable const* variable, std::vector<std::string> const& attribute)
      : _variable(variable), _attribute(attribute) {
    TRI_ASSERT(_variable != nullptr);
    TRI_ASSERT(!_attribute.empty());
  }

  bool before(ExecutionNode* en) override final {
    if (en->getType() == EN::CALCULATION) {
      auto node = static_cast<CalculationNode*>(en);
      node->expression()->replaceAttributeAccess(_variable, _attribute);
    } else if (en->getType() == EN::GATHER) {
      auto node = static_cast<GatherNode*>(en);
      // intentional copy
      auto sortVars = node->elements();
      for (auto& it : sortVars) {
        if (it.var == _variable && it.attributePath == _attribute) {
          it.attributePath.clear();
        }
      }
      node->elements(sortVars);
    }

    // always continue
    return false;
  }

 private:
  Variable const* _variable;
  std::vector<std::string> _attribute;
};

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
        // TODO
        stop = true;
        /*
        auto gn = static_cast<GatherNode*>(current);
        auto const& sortVars = gn->elements();
        for (auto& it : sortVars) {
          auto path = it.attributePath;
          std::reverse(path.begin(), path.end());
          if (it.var == v && path != attributeNames) {
            stop = true;
            break;
          }
        }
        */
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

    if (optimize && !stop && !attributes.empty() && attributes.size() <= 5) {
      std::vector<std::string> r;
      for (auto& it : attributes) {
        r.emplace_back(std::move(it));
      }
      e->projections(std::move(r));
      
      modified = true;
    }

/*
    if (optimize && !stop) {
      TRI_ASSERT(replaceVar != nullptr);

      AttributeAccessReplacer finder(v, attributeNames);
      plan->root()->walk(&finder);
      
      std::reverse(attributeNames.begin(), attributeNames.end());
      e->projections(std::move(attributeNames));

      modified = true;
    }
    */
  }
    
  opt->addPlan(std::move(plan), rule, modified);
}
