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

#include "IndexCollectNode.h"
#include "Indexes/Index.h"

using namespace arangodb;
using namespace arangodb::aql;

ExecutionNode* IndexCollectNode::clone(ExecutionPlan* plan,
                                       bool withDependencies) const {
  auto c = std::make_unique<IndexCollectNode>(plan, _id, collection(), _index);
  CollectionAccessingNode::cloneInto(*c);
  return cloneHelper(std::move(c), withDependencies);
}

std::unique_ptr<ExecutionBlock> IndexCollectNode::createBlock(
    ExecutionEngine& engine) const {
  TRI_ASSERT(false);
}

IndexCollectNode::IndexCollectNode(ExecutionPlan* plan,
                                   arangodb::velocypack::Slice slice)
    : ExecutionNode(plan, slice), CollectionAccessingNode(plan, slice) {
  TRI_ASSERT(false);
}

IndexCollectNode::IndexCollectNode(ExecutionPlan* plan, ExecutionNodeId id,
                                   const aql::Collection* collection,
                                   std::shared_ptr<arangodb::Index> index)
    : ExecutionNode(plan, id),
      CollectionAccessingNode(collection),
      _index(std::move(index)) {}

void IndexCollectNode::doToVelocyPack(velocypack::Builder& builder,
                                      unsigned int flags) const {
  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue("index"));
  _index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Estimates));
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
  return ExecutionNode::getVariablesSetHere();
}

CostEstimate IndexCollectNode::estimateCost() const {
  return CostEstimate(1, 1);
}
