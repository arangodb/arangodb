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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_INDEX_EXECUTOR_H
#define ARANGOD_AQL_INDEX_EXECUTOR_H

#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/IndexNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/types.h"
#include "Indexes/IndexIterator.h"
#include "Utils/OperationCursor.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ExecutorInfos;
class SingleRowFetcher;

class IndexExecutorInfos : public ExecutorInfos {
 public:
  IndexExecutorInfos(
      RegisterId outputRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
      std::unordered_set<RegisterId> registersToClear, ExecutionEngine* engine,
      Collection const* collection, Variable const* outVariable, bool produceResult,
      std::vector<std::string> const& projections, transaction::Methods* trxPtr,
      std::vector<size_t> const& coveringIndexAttributePositions,
      bool allowCoveringIndexOptimization, bool useRawDocumentPointers,
      std::vector<std::unique_ptr<NonConstExpression>>&& nonConstExpression,
      std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs,
      bool hasV8Expression, AstNode const* condition,
      std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast);

  IndexExecutorInfos() = delete;
  IndexExecutorInfos(IndexExecutorInfos&&) = default;
  IndexExecutorInfos(IndexExecutorInfos const&) = delete;
  ~IndexExecutorInfos() = default;

  // getter
  ExecutionEngine* getEngine() { return _engine; };
  Collection const* getCollection() { return _collection; };
  Variable const* getOutVariable() { return _outVariable; };
  std::vector<std::string> const& getProjections() { return _projections; };
  transaction::Methods* getTrxPtr() { return _trxPtr; };
  std::vector<size_t> const& getCoveringIndexAttributePositions() {
    return _coveringIndexAttributePositions;
  };
  std::vector<std::vector<Variable const*>> getInVars() { return _inVars; };
  std::vector<std::vector<RegisterId>> getInRegs() { return _inRegs; };
  bool getProduceResult() { return _produceResult; };
  bool getAllowCoveringIndexOptimization() {
    return _allowCoveringIndexOptimization;
  };
  bool getUseRawDocumentPointers() { return _useRawDocumentPointers; };
  std::vector<transaction::Methods::IndexHandle>& getIndexes() {
    return _indexes;
  };
  AstNode const* getCondition() { return _condition; };
  bool getV8Expression() { return _hasV8Expression; };
  RegisterId getOutputRegisterId() { return _outputRegisterId; };
  std::vector<std::unique_ptr<NonConstExpression>> const& getNonConstExpressions() {
    return _nonConstExpression;
  };
  bool hasMultipleExpansions() { return _hasMultipleExpansions; };
  bool isLastIndex() { return _isLastIndex; };
  Ast* getAst() { return _ast; };

  std::vector<Variable const*> getExpInVars() { return _expInVars; };
  std::vector<RegisterId> getExpInRegs() { return _expInRegs; };

  // setter
  void setHasMultipleExpansions(bool flag) { _hasMultipleExpansions = flag; };

 private:
  /// @brief _indexes holds all Indexes used in this block
  std::vector<transaction::Methods::IndexHandle> _indexes;

  /// @brief _inVars, a vector containing for each expression above
  /// a vector of Variable*, used to execute the expression
  std::vector<std::vector<Variable const*>> _inVars;

  /// @brief _condition: holds the complete condition this Block can serve for
  AstNode const* _condition;

  /// @brief _ast: holds the ast of the _plan
  Ast* _ast;

  /// @brief _inRegs, a vector containing for each expression above
  /// a vector of RegisterId, used to execute the expression
  std::vector<std::vector<RegisterId>> _inRegs;

  /// @brief true if one of the indexes uses more than one expanded attribute,
  /// e.g. the index is on values[*].name and values[*].type
  bool _hasMultipleExpansions;

  /// @brief Flag if the current index pointer is the last of the list.
  ///        Used in uniqueness checks.
  bool _isLastIndex;

  RegisterId _outputRegisterId;
  ExecutionEngine* _engine;
  Collection const* _collection;
  Variable const* _outVariable;
  std::vector<std::string> const& _projections;
  transaction::Methods* _trxPtr;
  std::vector<Variable const*> _expInVars;  // input variables for expresseion
  std::vector<RegisterId> _expInRegs;       // input registers for expression

  std::vector<size_t> const& _coveringIndexAttributePositions;
  bool _allowCoveringIndexOptimization;
  bool _useRawDocumentPointers;
  std::vector<std::unique_ptr<NonConstExpression>>& _nonConstExpression;
  bool _produceResult;
  bool _hasV8Expression;
};

/**
 * @brief Implementation of Index Node
 */
class IndexExecutor {
 public:
  using Fetcher = SingleRowFetcher;
  using Infos = IndexExecutorInfos;
  using Stats = IndexStats;

  IndexExecutor() = delete;
  IndexExecutor(IndexExecutor&&) = default;
  IndexExecutor(IndexExecutor const&) = delete;
  IndexExecutor(Fetcher& fetcher, Infos&);
  ~IndexExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  typedef std::function<void(InputAqlItemRow&, OutputAqlItemRow&, arangodb::velocypack::Slice, RegisterId)> DocumentProducingFunction;

  void setProducingFunction(DocumentProducingFunction documentProducer) {
    _documentProducer = documentProducer;
  };

 private:
  void executeExpressions(InputAqlItemRow& input, OutputAqlItemRow& output);

 private:
  Infos& _infos;
  Fetcher& _fetcher;
  DocumentProducingFunction _documentProducer;
  std::pair<ExecutionState, InputAqlItemRow> _input;

  /// @brief set of already returned documents. Used to make the result distinct
  std::unordered_set<TRI_voc_rid_t> _alreadyReturned;
};

}  // namespace aql
}  // namespace arangodb

#endif
