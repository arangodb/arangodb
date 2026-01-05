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

#include "OptimizerRuleOptimizeProjections.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

class AttributeAccessReplacer final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
public:
  AttributeAccessReplacer(ExecutionNode const* self,
                          Variable const* searchVariable,
                          std::span<std::string_view> attribute,
                          Variable const* replaceVariable, size_t index)
      : _self(self),
        _searchVariable(searchVariable),
        _attribute(attribute),
        _replaceVariable(replaceVariable),
        _index(index) {
    TRI_ASSERT(_searchVariable != nullptr);
    TRI_ASSERT(!_attribute.empty());
    TRI_ASSERT(_replaceVariable != nullptr);
  }

  bool before(ExecutionNode* en) override final {
    en->replaceAttributeAccess(_self, _searchVariable, _attribute,
                               _replaceVariable, _index);

    // always continue
    return false;
  }

private:
  ExecutionNode const* _self;
  Variable const* _searchVariable;
  std::span<std::string_view> _attribute;
  Variable const* _replaceVariable;
  size_t _index;
};

void arangodb::aql::optimizeProjections(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(
      nodes, {EN::INDEX, EN::ENUMERATE_COLLECTION, EN::JOIN, EN::MATERIALIZE},
      true);

  auto replace = [&plan](ExecutionNode* self, Projections& p,
                         Variable const* searchVariable, size_t index) {
    bool modified = false;
    std::vector<std::string_view> path;
    for (size_t i = 0; i < p.size(); ++i) {
      TRI_ASSERT(p[i].variable == nullptr);
      p[i].variable = plan->getAst()->variables()->createTemporaryVariable();
      path.clear();
      for (auto const& it : p[i].path.get()) {
        path.emplace_back(it);
      }

      AttributeAccessReplacer replacer(self, searchVariable, std::span(path),
                                       p[i].variable, index);
      plan->root()->walk(replacer);
      modified = true;
    }
    return modified;
  };

  bool modified = false;
  for (auto* n : nodes) {
    if (n->getType() == EN::JOIN) {
      // JoinNode. optimize projections in all parts
      auto* joinNode = ExecutionNode::castTo<JoinNode*>(n);
      size_t index = 0;
      for (auto& it : joinNode->getIndexInfos()) {
        modified |= replace(n, it.projections, it.outVariable, index++);
      }
    } else if (n->getType() == EN::MATERIALIZE) {
      auto* matNode = dynamic_cast<materialize::MaterializeRocksDBNode*>(n);
      if (matNode == nullptr) {
        continue;
      }

      containers::FlatHashSet<AttributeNamePath> attributes;
      if (utils::findProjections(matNode, &matNode->outVariable(),
                                 /*expectedAttribute*/ "",
                                 /*excludeStartNodeFilterCondition*/ true,
                                 attributes)) {
        if (attributes.size() <= matNode->maxProjections()) {
          matNode->projections() = Projections(std::move(attributes));
        }
      }

      modified |= replace(n, matNode->projections(), &matNode->outVariable(),
                          /*index*/ 0);
    } else {
      // IndexNode or EnumerateCollectionNode.
      TRI_ASSERT(n->getType() == EN::ENUMERATE_COLLECTION ||
                 n->getType() == EN::INDEX);

      auto* documentNode = ExecutionNode::castTo<DocumentProducingNode*>(n);
      if (documentNode->projections().hasOutputRegisters()) {
        // Some late materialize rule sets output registers
        continue;
      }
      modified |= documentNode->recalculateProjections(plan.get());
      modified |= replace(n, documentNode->projections(),
                          documentNode->outVariable(), /*index*/ 0);
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}