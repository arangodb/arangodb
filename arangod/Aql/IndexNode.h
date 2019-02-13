////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_INDEX_NODE_H
#define ARANGOD_AQL_INDEX_NODE_H 1

#include "Aql/Ast.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Slice.h>

namespace arangodb {

namespace aql {
struct Collection;
class Condition;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionPlan;

/// @brief struct to hold the member-indexes in the _condition node
struct NonConstExpression {
  std::unique_ptr<Expression> expression;
  std::vector<size_t> const indexPath;

  NonConstExpression(std::unique_ptr<Expression> exp, std::vector<size_t>&& idxPath)
      : expression(std::move(exp)), indexPath(std::move(idxPath)) {}
};

/// @brief class IndexNode
class IndexNode : public ExecutionNode, public DocumentProducingNode, public CollectionAccessingNode {
  friend class ExecutionBlock;
  friend class IndexBlock;

 public:
  IndexNode(ExecutionPlan* plan, size_t id, aql::Collection const* collection,
            Variable const* outVariable,
            std::vector<transaction::Methods::IndexHandle> const& indexes,
            std::unique_ptr<Condition> condition, IndexIteratorOptions const&);

  IndexNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  ~IndexNode();

  /// @brief return the type of the node
  NodeType getType() const override final { return INDEX; }

  /// @brief return the condition for the node
  Condition* condition() const { return _condition.get(); }

  /// @brief whether or not all indexes are accessed in reverse order
  IndexIteratorOptions options() const { return _options; }

  /// @brief set reverse mode
  void setAscending(bool value) { _options.ascending = value; }

  /// @brief whether or not the index node needs a post sort of the results
  /// of multiple shards in the cluster (via a GatherNode).
  /// not all queries that use an index will need to produce a sorted result
  /// (e.g. if the index is used only for filtering)
  bool needsGatherNodeSort() const { return _needsGatherNodeSort; }
  void needsGatherNodeSort(bool value) { _needsGatherNodeSort = value; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getIndexes, hand out the indexes used
  std::vector<transaction::Methods::IndexHandle> const& getIndexes() const {
    return _indexes;
  }

  /// @brief called to build up the matching positions of the index values for
  /// the projection attributes (if any)
  void initIndexCoversProjections();

 private:
  void initializeOnce(bool hasV8Expression,
                      std::vector<Variable const*>& inVars,
                      std::vector<RegisterId>& inRegs,
                      std::vector<std::unique_ptr<NonConstExpression>>& nonConstExpressions,
                      transaction::Methods* trxPtr) const;

  /// @brief adds a UNIQUE() to a dynamic IN condition
  arangodb::aql::AstNode* makeUnique(arangodb::aql::AstNode*, transaction::Methods* trx) const;

 private:
  /// @brief the index
  std::vector<transaction::Methods::IndexHandle> _indexes;

  /// @brief the index(es) condition
  std::unique_ptr<Condition> _condition;

  /// @brief the index sort order - this is the same order for all indexes
  bool _needsGatherNodeSort;

  /// @brief the index iterator options - same for all indexes
  IndexIteratorOptions _options;
};

}  // namespace aql
}  // namespace arangodb

#endif
