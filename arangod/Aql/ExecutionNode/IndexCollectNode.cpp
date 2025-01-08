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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/IndexDistinctScanExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Aql/ExecutionEngine.h"
#include "IndexCollectNode.h"
#include <velocypack/Iterator.h>
#include "Indexes/Index.h"

using namespace arangodb;
using namespace arangodb::aql;

ExecutionNode* IndexCollectNode::clone(ExecutionPlan* plan,
                                       bool withDependencies) const {
  auto c = std::make_unique<IndexCollectNode>(plan, _id, collection(), _index,
                                              _oldIndexVariable, _groups,
                                              _collectOptions);
  CollectionAccessingNode::cloneInto(*c);
  return cloneHelper(std::move(c), withDependencies);
}

std::unique_ptr<ExecutionBlock> IndexCollectNode::createBlock(
    ExecutionEngine& engine) const {
  IndexDistinctScanInfos infos;
  infos.groupRegisters.reserve(_groups.size());
  infos.scanOptions.distinctFields.reserve(_groups.size());
  infos.index = _index;
  infos.collection = CollectionAccessingNode::collection();
  infos.query = &engine.getQuery();

  RegIdSet writableOutputRegisters;
  for (auto const& group : _groups) {
    auto reg = infos.groupRegisters.emplace_back(
        getRegisterPlan()->variableToRegisterId(group.outVariable));
    infos.scanOptions.distinctFields.emplace_back(group.indexField);
    writableOutputRegisters.emplace(reg);
  }

  infos.scanOptions.sorted = _collectOptions.requiresSortedInput();

  auto registerInfos = createRegisterInfos({}, writableOutputRegisters);
  return std::make_unique<ExecutionBlockImpl<IndexDistinctScanExecutor>>(
      &engine, this, registerInfos, std::move(infos));
}

IndexCollectNode::IndexCollectNode(ExecutionPlan* plan,
                                   arangodb::velocypack::Slice slice)
    : ExecutionNode(plan, slice),
      CollectionAccessingNode(plan, slice),
      _collectOptions(slice.get("collectOptions")) {
  std::string iid = slice.get("index").get("id").copyString();
  _index = CollectionAccessingNode::collection()->indexByIdentifier(iid);
  _oldIndexVariable =
      Variable::varFromVPack(plan->getAst(), slice, "oldIndexVariable");
  auto groups = slice.get("groups");
  for (auto gs : VPackArrayIterator(groups)) {
    auto& g = _groups.emplace_back();
    g.outVariable = Variable::varFromVPack(plan->getAst(), gs, "outVariable");
    g.indexField = gs.get("indexField").getNumericValue<std::size_t>();
  }
}

IndexCollectNode::IndexCollectNode(ExecutionPlan* plan, ExecutionNodeId id,
                                   const aql::Collection* collection,
                                   std::shared_ptr<arangodb::Index> index,
                                   Variable const* oldIndexVariable,
                                   IndexCollectGroups groups,
                                   CollectOptions collectOptions)
    : ExecutionNode(plan, id),
      CollectionAccessingNode(collection),
      _index(std::move(index)),
      _groups(std::move(groups)),
      _oldIndexVariable(oldIndexVariable),
      _collectOptions(collectOptions) {}

void IndexCollectNode::doToVelocyPack(velocypack::Builder& builder,
                                      unsigned int flags) const {
  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue("index"));
  _index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Estimates));
  builder.add(VPackValue("oldIndexVariable"));
  _oldIndexVariable->toVelocyPack(builder);

  {
    VPackArrayBuilder ab(&builder, "groups");
    for (auto const& grp : _groups) {
      VPackObjectBuilder ob(&builder);
      builder.add("indexField", grp.indexField);
      builder.add(VPackValue("outVariable"));
      grp.outVariable->toVelocyPack(builder);
      {
        VPackArrayBuilder ar(&builder, "attribute");
        for (auto const& p : _index->coveredFields()[grp.indexField]) {
          builder.add(VPackValue(p.name));
        }
      }
    }
  }

  builder.add(VPackValue("collectOptions"));
  _collectOptions.toVelocyPack(builder);
}

void IndexCollectNode::replaceVariables(
    const std::unordered_map<VariableId, const Variable*>& replacements) {
  ExecutionNode::replaceVariables(replacements);
}

void IndexCollectNode::replaceAttributeAccess(
    const ExecutionNode* self, const Variable* searchVariable,
    std::span<std::string_view> attribute, const Variable* replaceVariable,
    size_t index) {
  ExecutionNode::replaceAttributeAccess(self, searchVariable, attribute,
                                        replaceVariable, index);
}

void IndexCollectNode::getVariablesUsedHere(VarSet& vars) const {
  ExecutionNode::getVariablesUsedHere(vars);
}

std::vector<const Variable*> IndexCollectNode::getVariablesSetHere() const {
  std::vector<const Variable*> result;
  result.reserve(_groups.size());
  for (auto const& grp : _groups) {
    result.emplace_back(grp.outVariable);
  }
  return result;
}

CostEstimate IndexCollectNode::estimateCost() const {
  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  if (trx.status() != transaction::Status::RUNNING) {
    return CostEstimate::empty();
  }

  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  auto documentsInCollection =
      collection()->count(&trx, transaction::CountType::kTryCache);

  double selectivity = _index->selectivityEstimate();

  estimate.estimatedNrItems = selectivity * double(documentsInCollection);
  estimate.estimatedCost += selectivity * log(double(documentsInCollection));
  return estimate;
}
