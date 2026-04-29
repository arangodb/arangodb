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

// static node types used by some optimizer rules
// having them statically available avoids having to build the lists over
// and over for each AQL query
static constexpr std::initializer_list<EN::NodeType> undistributeNodeTypes{
    EN::UPDATE, EN::REPLACE, EN::REMOVE};

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

        // find the variable we are removing . . .
        auto rn = EN::castTo<arangodb::aql::ModificationNode*>(en);
        arangodb::aql::Variable const* toRemove = nullptr;

        if (en->getType() == EN::REPLACE) {
          toRemove = EN::castTo<arangodb::aql::ReplaceNode const*>(en)
                         ->inKeyVariable();
        } else if (en->getType() == EN::UPDATE) {
          // first try if we have the pattern UPDATE <key> WITH <doc> IN
          // collection. if so, then toRemove will contain <key>.
          toRemove =
              EN::castTo<arangodb::aql::UpdateNode const*>(en)->inKeyVariable();

          if (toRemove == nullptr) {
            // if we don't have that pattern, we can if instead have
            // UPDATE <doc> IN collection.
            // in this case toRemove will contain <doc>.
            toRemove = EN::castTo<arangodb::aql::UpdateNode const*>(en)
                           ->inDocVariable();
          }
        } else if (en->getType() == EN::REMOVE) {
          toRemove =
              EN::castTo<arangodb::aql::RemoveNode const*>(en)->inVariable();
        } else {
          TRI_ASSERT(false);
        }

        if (toRemove == nullptr) {
          // abort
          break;
        }

        _setter = _plan->getVarSetBy(toRemove->id);
        TRI_ASSERT(_setter != nullptr);
        auto enumColl = _setter;

        if (_setter->getType() == EN::CALCULATION) {
          // this should be an attribute access for _key
          auto cn = EN::castTo<arangodb::aql::CalculationNode*>(_setter);

          auto expr = cn->expression();
          if (expr->isAttributeAccess()) {
            // check the variable is the same as the remove variable
            if (cn->outVariable() != toRemove) {
              break;  // abort . . .
            }
            // check that the modification node's collection is sharded over
            // _key
            std::vector<std::string> shardKeys =
                rn->collection()->shardKeys(false);
            if (shardKeys.size() != 1 ||
                shardKeys[0] != arangodb::StaticStrings::KeyString) {
              break;  // abort . . .
            }

            // set the varsToRemove to the variable in the expression of this
            // node and also define enumColl
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

            // note for which shard keys we need to look for
            auto shardKeys = rn->collection()->shardKeys(false);
            std::unordered_set<std::string> toFind;
            for (auto const& it : shardKeys) {
              toFind.emplace(it);
            }
            // for UPDATE/REPLACE/REMOVE, we must also know the _key value,
            // otherwise they will not work.
            toFind.emplace(arangodb::StaticStrings::KeyString);

            // go through the input object attribute by attribute
            // and look for our shard keys
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
                // we found one of the shard keys!
                // remove the attribute from our to-do list
                auto value = objElem.getValue();

                // check if we have something like: { key: source.key }
                if (value->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS &&
                    value->getStringView() == attributeName) {
                  // check if all values for the shard keys are referring to
                  // the same FOR loop variable
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
              // not all shard keys covered, or different source variables in
              // use
              break;
            }

            TRI_ASSERT(lastVariable != nullptr);
            enumColl = _plan->getVarSetBy(lastVariable->id);
          } else {
            // cannot optimize this type of input
            break;
          }
        }

        if (enumColl->getType() != EN::ENUMERATE_COLLECTION &&
            enumColl->getType() != EN::INDEX) {
          break;  // abort . . .
        }

        auto const& projections =
            dynamic_cast<arangodb::aql::DocumentProducingNode const*>(enumColl)
                ->projections();
        if (projections.isSingle(arangodb::StaticStrings::KeyString)) {
          // cannot handle projections
          break;
        }

        _enumColl = enumColl;

        if (arangodb::aql::utils::getCollection(_enumColl) !=
            rn->collection()) {
          break;  // abort . . .
        }

        _variable = toRemove;  // the variable we'll remove
        _foundModification = true;
        return false;  // continue . . .
      }
      case EN::REMOTE: {
        _toUnlink.emplace(en);
        return false;  // continue . . .
      }
      case EN::DISTRIBUTE:
      case EN::SCATTER: {
        if (_foundScatter) {  // met more than one scatter node
          break;              // abort . . .
        }
        _foundScatter = true;
        _toUnlink.emplace(en);
        return false;  // continue . . .
      }
      case EN::GATHER: {
        if (_foundGather) {  // met more than one gather node
          break;             // abort . . .
        }
        _foundGather = true;
        _toUnlink.emplace(en);
        return false;  // continue . . .
      }
      case EN::FILTER: {
        return false;  // continue . . .
      }
      case EN::CALCULATION: {
        TRI_vocbase_t& vocbase = _plan->getAst()->query().vocbase();
        auto calculationNode = EN::castTo<arangodb::aql::CalculationNode*>(en);
        auto expr = calculationNode->expression();

        // If we find an expression that is not allowed to run on a DBServer,
        // we cannot undistribute (as then the expression *would* run on a
        // dbserver)
        if (!expr->canRunOnDBServer(vocbase.isOneShard())) {
          break;
        }
        return false;  // continue . . .
      }
      case EN::WINDOW: {
        return false;  // continue . . .
      }
      case EN::ENUMERATE_COLLECTION:
      case EN::INDEX: {
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
        // if we meet any of the above, then we abort . . .
        break;
      }

      default: {
        // should not reach this point
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
