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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RemoveUnnecessaryRemoteScatter.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"
#include "VocBase/vocbase.h"

namespace arangodb::aql {
using EN = ExecutionNode;

/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
void removeUnnecessaryRemoteScatterRule(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::REMOTE, false);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
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
      if (!plan->shouldExcludeFromScatterGather(node)) {
        if (node->getType() != EN::SINGLETON &&
            node->getType() != EN::CALCULATION &&
            node->getType() != EN::FILTER) {
          canOptimize = false;
          break;
        }

        if (node->getType() == EN::CALCULATION) {
          auto calc = ExecutionNode::castTo<CalculationNode const*>(node);
          TRI_vocbase_t& vocbase = plan->getAst()->query().vocbase();
          if (!calc->expression()->canRunOnDBServer(vocbase.isOneShard())) {
            canOptimize = false;
            break;
          }
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

  opt->addPlan(std::move(plan), rule, !toUnlink.empty());
}

}  // namespace arangodb::aql
