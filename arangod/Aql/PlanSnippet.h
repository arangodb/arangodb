////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

// Included for GatherNode internal enums
#include "Aql/ClusterNodes.h"

#include <iosfwd>

namespace arangodb {
namespace aql {
class ExecutionNode;
class CollectNode;
class SortNode;
struct Variable;

class PlanSnippet {
 private:
  class DistributionInput {
   public:
    explicit DistributionInput(ExecutionNode*);

    bool testAndMoveInputIfShardKeysUntouched(CalculationNode const* node);

    Variable const* variable() const;
    Collection const* collection() const;
    ExecutionNodeId targetId() const;
    bool createKeys() const;

    ExecutionNode* createDistributeNode(ExecutionPlan*) const;
    ExecutionNode* createDistributeInputNode(ExecutionPlan*) const;

   private:
    ExecutionNode* _distributeOnNode;
    bool _createKeys;
    Variable const* _variable;
  };

  class GatherOutput {
   public:
    GatherOutput();

    ExecutionNode* createGatherNode(ExecutionPlan*) const;

    bool tryAndIncludeSortNode(SortNode const* sort);

    void memorizeCollect(CollectNode* collect);

    ExecutionNode* eventuallyCreateCollectNode(ExecutionPlan* plan);

   private:
    GatherNode::SortMode getGatherSortMode() const;
    GatherNode::Parallelism getGatherParallelism() const;

    void adjustSortElements(
        ExecutionPlan* plan,
        std::unordered_map<arangodb::aql::Variable const*,
                           arangodb::aql::Variable const*> const& replacements);

    aql::CollectNode* _collect;
    arangodb::aql::SortElementVector _elements;
  };

 public:
  static void optimizeAdjacentSnippets(
      std::shared_ptr<PlanSnippet> upperSnippet,
      std::shared_ptr<PlanSnippet> lowerSnippet);

  explicit PlanSnippet(ExecutionNode* node);

  bool tryJoinAbove(ExecutionNode* node);

  bool tryJoinBelow(ExecutionNode* node);

  bool canStealTopNode() const;

  void stealTopNode();

  // Can only be called ONCE per snippet.
  // Afterwards the Snippet is ready to be deployed
  void insertCommunicationNodes();

  bool isLastNodeInSnippet(ExecutionNode const* node) const;

  ExecutionNode* getLowestNode() const;

  bool isOnCoordinator() const;

  friend std::ostream& operator<<(std::ostream&, PlanSnippet const&);

 private:
  void assertInvariants() const;

  void addRemoteAbove();

  void addDistributeAbove();

  void addRemoteBelow();

  void addCollectBelow();

  void addGatherBelow();

 private:
  ExecutionNode* _topMost;
  ExecutionNode* _last;

  bool _isOnCoordinator;
  std::unique_ptr<DistributionInput> _distributeOn;
  GatherOutput _gatherOutput;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool _communicationNodesInserted{false};
#endif
};

}  // namespace aql
}  // namespace arangodb
