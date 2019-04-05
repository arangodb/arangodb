////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_EXECUTION_BLOCK_MOCK_H
#define ARANGODB_IRESEARCH__IRESEARCH_EXECUTION_BLOCK_MOCK_H 1

#include "Aql/ExecutionBlock.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
class InputAqlItemRow;
class ExecutionEngine;
class ExecutionNode;

}  // namespace aql
}  // namespace arangodb

template <typename Node>
class MockNode : public Node {
 public:
  MockNode(size_t id = 0) : Node(nullptr, id) {
    Node::setVarUsageValid();
    Node::planRegisters();
  }
};

class ExecutionNodeMock final : public arangodb::aql::ExecutionNode {
 public:
  explicit ExecutionNodeMock(size_t id = 0);

  /// @brief return the type of the node
  virtual NodeType getType() const override;

  virtual std::unique_ptr<arangodb::aql::ExecutionBlock> createBlock(
      arangodb::aql::ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, arangodb::aql::ExecutionBlock*> const& cache) const override;

  /// @brief clone execution Node recursively, this makes the class abstract
  virtual ExecutionNode* clone(arangodb::aql::ExecutionPlan* plan, bool withDependencies,
                               bool withProperties) const override;

  /// @brief this actually estimates the costs as well as the number of items
  /// coming out of the node
  virtual arangodb::aql::CostEstimate estimateCost() const override {
    arangodb::aql::CostEstimate estimate = arangodb::aql::CostEstimate::empty();
    estimate.estimatedCost = 1.;
    return estimate;
  }

  /// @brief toVelocyPack
  virtual void toVelocyPackHelper(arangodb::velocypack::Builder& nodes,
                                  unsigned flags) const override;
};  // ExecutionNodeMock

class ExecutionBlockMock final : public arangodb::aql::ExecutionBlock {
 public:
  ExecutionBlockMock(arangodb::aql::AqlItemBlock const& data,
                     arangodb::aql::ExecutionEngine& engine,
                     arangodb::aql::ExecutionNode const& node);

  // here we release our docs from this collection
  std::pair<arangodb::aql::ExecutionState, arangodb::Result> initializeCursor(
      arangodb::aql::InputAqlItemRow const& input) override;

  std::pair<arangodb::aql::ExecutionState, std::unique_ptr<arangodb::aql::AqlItemBlock>> getSome(
      size_t atMost) override;

  virtual std::pair<arangodb::aql::ExecutionState, arangodb::Result> shutdown(int errorCode) override;

  // skip between atLeast and atMost returns the number actually skipped . . .
  // will only return less than atLeast if there aren't atLeast many
  // things to skip overall.
  std::pair<arangodb::aql::ExecutionState, size_t> skipSome(size_t atMost) override;

 private:
  arangodb::aql::AqlItemBlock const* _data;
  size_t _pos_in_data{};
  size_t _inflight;

};  // ExecutionBlockMock

#endif  // ARANGODB_IRESEARCH__IRESEARCH_EXECUTION_BLOCK_MOCK_H
