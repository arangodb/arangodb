////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_TESTS_MOCK_TYPED_NODE_H
#define ARANGODB_TESTS_MOCK_TYPED_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"

namespace arangodb::tests::aql {

// An execution node pretending to be of an arbitrary type.
class MockTypedNode : public ::arangodb::aql::ExecutionNode {
  friend class ExecutionBlock;

 public:
  MockTypedNode(::arangodb::aql::ExecutionPlan* plan, arangodb::aql::ExecutionNodeId id, NodeType);

  // return mocked type
  NodeType getType() const override final;

  // Necessary overrides, all not implemented:

  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override;

  std::unique_ptr<::arangodb::aql::ExecutionBlock> createBlock(
      ::arangodb::aql::ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ::arangodb::aql::ExecutionBlock*> const&) const override;

  ExecutionNode* clone(::arangodb::aql::ExecutionPlan* plan,
                       bool withDependencies, bool withProperties) const override;

  ::arangodb::aql::CostEstimate estimateCost() const override;
 private:
  NodeType _mockedType{};
};

}  // namespace arangodb::tests::aql

#endif  // ARANGODB_TESTS_MOCK_TYPED_NODE_H
