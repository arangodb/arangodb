////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SUBQUERY_END_EXECUTION_NODE_H
#define ARANGOD_AQL_SUBQUERY_END_EXECUTION_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"

namespace arangodb {
namespace aql {

class SubqueryEndNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  SubqueryEndNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  SubqueryEndNode(ExecutionPlan* plan, ExecutionNodeId id, Variable const* inVariable,
                  Variable const* outVariable);

  CostEstimate estimateCost() const override final;

  NodeType getType() const override final { return SUBQUERY_END; }

  Variable const* inVariable() const { return _inVariable; }

  Variable const* outVariable() const { return _outVariable; }

  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  bool isEqualTo(ExecutionNode const& other) const override final;

  void getVariablesUsedHere(VarSet& usedVars) const override final {
    if (_inVariable != nullptr) {
      usedVars.emplace(_inVariable);
    }
  }

  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  void replaceOutVariable(Variable const* var);

  // We only override this to TRI_ASSERT(false), because
  // noone should ever ask this node whether it is a modification
  // node
  bool isModificationNode() const override;

 private:
  Variable const* _inVariable;
  Variable const* _outVariable;
};

}  // namespace aql
}  // namespace arangodb

#endif
