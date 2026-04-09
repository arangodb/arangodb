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

#include "UndistributeRemoveAfterEnumColl.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ReplaceNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Basics/StaticStrings.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"
#include "VocBase/vocbase.h"

#include <initializer_list>

namespace {

using EN = arangodb::aql::ExecutionNode;

static constexpr std::initializer_list<EN::NodeType>
    undistributeNodeTypes{EN::UPDATE, EN::REPLACE, EN::REMOVE};

/// WalkerWorker for undistributeRemoveAfterEnumColl
class RemoveToEnumCollFinder final
    : public arangodb::aql::WalkerWorker<
          arangodb::aql::ExecutionNode,
          arangodb::aql::WalkerUniqueness::NonUnique> {
  arangodb::aql::ExecutionPlan* _plan;
  ::arangodb::containers::HashSet<arangodb::aql::ExecutionNode*>& _toUnlink;
  bool _foundModification;
  bool _foundScatter;
  bool _foundGather;
  arangodb::aql::ExecutionNode* _enumColl;
  arangodb::aql::ExecutionNode* _setter;
  arangodb::aql::Variable const* _variable;

 public:
  RemoveToEnumCollFinder(
      arangodb::aql::ExecutionPlan* plan,
      ::arangodb::containers::HashSet<arangodb::aql::ExecutionNode*>& toUnlink)
      : _plan(plan),
        _toUnlink(toUnlink),
        _foundModification(false),
        _foundScatter(false),
        _foundGather(false),
        _enumColl(nullptr),
        _setter(nullptr),
        _variable(nullptr) {}

  bool before(arangodb::aql::ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::UPDATE:
      case EN::REPLACE:
      case EN::REMOVE: {
        if (_foundModification) {
          break;
        }

        auto rn =
            EN::castTo<arangodb::aql::ModificationNode*>(en);
        arangodb::aql::Variable const* toRemove = nullptr;

        if (en->getType() == EN::REPLACE) {
          toRemove =
              EN::castTo<arangodb::aql::ReplaceNode const*>(en)
                  ->inKeyVariable();
        } else if (en->getType() == EN::UPDATE) {
          toRemove =
              EN::castTo<arangodb::aql::UpdateNode const*>(en)
                  ->inKeyVariable();

          if (toRemove == nullptr) {
            toRemove =
                EN::castTo<arangodb::aql::UpdateNode const*>(en)
                    ->inDocVariable();
          }
        } else if (en->getType() == EN::REMOVE) {
          toRemove =
              EN::castTo<arangodb::aql::RemoveNode const*>(en)->inVariable();
        } else {
          TRI_ASSERT(false);
        }

        if (toRemove == nullptr) {
          break;
        }

        _setter = _plan->getVarSetBy(toRemove->id);
        TRI_ASSERT(_setter != nullptr);
        auto enumColl = _setter;

        if (_setter->getType() == EN::CALCULATION) {
          auto cn =
              EN::castTo<arangodb::aql::CalculationNode*>(_setter);

          auto expr = cn->expression();
          if (expr->isAttributeAccess()) {
            if (cn->outVariable() != toRemove) {
              break;
            }
            std::vector<std::string> shardKeys =
                rn->collection()->shardKeys(false);
            if (shardKeys.size() != 1 ||
                shardKeys[0] != arangodb::StaticStrings::KeyString) {
              break;
            }

            arangodb::aql::VarSet varsToRemove;
            cn->getVariablesUsedHere(varsToRemove);
            TRI_ASSERT(varsToRemove.size() == 1);
            toRemove = *(varsToRemove.begin());
            enumColl = _plan->getVarSetBy(toRemove->id);
            TRI_ASSERT(_setter != nullptr);
          } else if (expr->node() && expr->node()->isObject()) {
            auto n = expr->node();

            if (n == nullptr) {
              break;
            }

            auto shardKeys = rn->collection()->shardKeys(false);
            std::unordered_set<std::string> toFind;
            for (auto const& it : shardKeys) {
              toFind.emplace(it);
            }
            toFind.emplace(arangodb::StaticStrings::KeyString);

            arangodb::aql::Variable const* lastVariable = nullptr;
            bool doOptimize = true;

            for (size_t i = 0; i < n->numMembers(); ++i) {
              auto sub = n->getMember(i);

              if (sub->type != arangodb::aql::NODE_TYPE_OBJECT_ELEMENT) {
                continue;
              }

              arangodb::aql::ast::ObjectElementNode objElem(sub);
              std::string attributeName = sub->getString();
              auto it = toFind.find(attributeName);

              if (it != toFind.end()) {
                auto value = objElem.getValue();

                if (value->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS &&
                    value->getStringView() == attributeName) {
                  arangodb::aql::ast::AttributeAccessNode attrAccess(value);
                  auto var = attrAccess.getObject();
                  if (var->type == arangodb::aql::NODE_TYPE_REFERENCE) {
                    arangodb::aql::ast::ReferenceNode ref(var);
                    auto accessedVariable = ref.getVariable();

                    if (lastVariable == nullptr) {
                      lastVariable = accessedVariable;
                    } else if (lastVariable != accessedVariable) {
                      doOptimize = false;
                      break;
                    }

                    toFind.erase(it);
                  }
                }
              }
            }

            if (!toFind.empty() || !doOptimize || lastVariable == nullptr) {
              break;
            }

            TRI_ASSERT(lastVariable != nullptr);
            enumColl = _plan->getVarSetBy(lastVariable->id);
          } else {
            break;
          }
        }

        if (enumColl->getType() != EN::ENUMERATE_COLLECTION &&
            enumColl->getType() != EN::INDEX) {
          break;
        }

        auto const& projections =
            dynamic_cast<arangodb::aql::DocumentProducingNode const*>(enumColl)
                ->projections();
        if (projections.isSingle(arangodb::StaticStrings::KeyString)) {
          break;
        }

        _enumColl = enumColl;

        if (arangodb::aql::utils::getCollection(_enumColl) !=
            rn->collection()) {
          break;
        }

        _variable = toRemove;
        _foundModification = true;
        return false;
      }
      case EN::REMOTE: {
        _toUnlink.emplace(en);
        return false;
      }
      case EN::DISTRIBUTE:
      case EN::SCATTER: {
        if (_foundScatter) {
          break;
        }
        _foundScatter = true;
        _toUnlink.emplace(en);
        return false;
      }
      case EN::GATHER: {
        if (_foundGather) {
          break;
        }
        _foundGather = true;
        _toUnlink.emplace(en);
        return false;
      }
      case EN::FILTER: {
        return false;
      }
      case EN::CALCULATION: {
        TRI_vocbase_t& vocbase = _plan->getAst()->query().vocbase();
        auto calculationNode =
            EN::castTo<arangodb::aql::CalculationNode*>(en);
        auto expr = calculationNode->expression();

        if (!expr->canRunOnDBServer(vocbase.isOneShard())) {
          break;
        }
        return false;
      }
      case EN::WINDOW: {
        return false;
      }
      case EN::ENUMERATE_COLLECTION:
      case EN::INDEX: {
        TRI_ASSERT(_enumColl != nullptr);
        if (en->id() != _enumColl->id()) {
          break;
        }
        return true;
      }
      case EN::SINGLETON:
      case EN::ENUMERATE_LIST:
      case EN::ENUMERATE_IRESEARCH_VIEW:
      case EN::SUBQUERY:
      case EN::COLLECT:
      case EN::INSERT:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::LIMIT:
      case EN::SORT:
      case EN::TRAVERSAL:
      case EN::ENUMERATE_PATHS:
      case EN::SHORTEST_PATH: {
        break;
      }

      default: {
        TRI_ASSERT(false);
      }
    }

    _toUnlink.clear();
    return true;
  }
};

}  // namespace

namespace arangodb::aql {

/// @brief recognizes that a RemoveNode can be moved to the shards.
void undistributeRemoveAfterEnumCollRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, undistributeNodeTypes, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto& n : nodes) {
    RemoveToEnumCollFinder finder(plan.get(), toUnlink);
    n->walk(finder);
  }

  bool modified = false;
  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    modified = true;
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
