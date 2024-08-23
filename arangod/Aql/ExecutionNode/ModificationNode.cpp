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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ModificationNode.h"

#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Variable.h"

using namespace arangodb::aql;

namespace arangodb::aql {

ModificationNode::ModificationNode(ExecutionPlan* plan,
                                   arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      CollectionAccessingNode(plan, base),
      _options(base),
      _outVariableOld(
          Variable::varFromVPack(plan->getAst(), base, "outVariableOld", true)),
      _outVariableNew(
          Variable::varFromVPack(plan->getAst(), base, "outVariableNew", true)),
      _countStats(base.get("countStats").getBool()),
      _producesResults(base.hasKey("producesResults")
                           ? base.get("producesResults").getBool()
                           : true) {}

/// @brief toVelocyPack
void ModificationNode::doToVelocyPack(VPackBuilder& builder,
                                      unsigned flags) const {
  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);
  CollectionAccessingNode::toVelocyPackHelperPrimaryIndex(builder);

  // Now put info about vocbase and cid in there
  builder.add("countStats", VPackValue(_countStats));

  builder.add("producesResults", VPackValue(_producesResults));

  // add out variables
  if (_outVariableOld != nullptr) {
    builder.add(VPackValue("outVariableOld"));
    _outVariableOld->toVelocyPack(builder);
  }
  if (_outVariableNew != nullptr) {
    builder.add(VPackValue("outVariableNew"));
    _outVariableNew->toVelocyPack(builder);
  }
  builder.add(VPackValue("modificationFlags"));

  _options.toVelocyPack(builder);
}

/// @brief estimateCost
/// Note that all the modifying nodes use this estimateCost method which is
/// why we can make it final here.
CostEstimate ModificationNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  if (_outVariableOld == nullptr && _outVariableNew == nullptr &&
      !_producesResults) {
    // node produces no output
    estimate.estimatedNrItems = 0;
  }
  return estimate;
}

std::vector<Variable const*> ModificationNode::getVariablesSetHere() const {
  std::vector<Variable const*> v;
  if (_outVariableOld != nullptr) {
    v.emplace_back(_outVariableOld);
  }
  if (_outVariableNew != nullptr) {
    v.emplace_back(_outVariableNew);
  }
  return v;
}

AsyncPrefetchEligibility ModificationNode::canUseAsyncPrefetching()
    const noexcept {
  return AsyncPrefetchEligibility::kDisableGlobally;
}

void ModificationNode::cloneCommon(ModificationNode* c) const {
  if (!_countStats) {
    c->disableStatistics();
  }
  c->producesResults(_producesResults);
  CollectionAccessingNode::cloneInto(*c);
}

}  // namespace arangodb::aql
