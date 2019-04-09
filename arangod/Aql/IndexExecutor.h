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

template <bool pass>
class SingleRowFetcher;

class IndexExecutorInfos : public ExecutorInfos {
 public:
  IndexExecutorInfos(
      RegisterId outputRegister, RegisterId nrInputRegisters,
      RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
      std::unordered_set<RegisterId> registersToKeep, ExecutionEngine* engine,
      Collection const* collection, Variable const* outVariable, bool produceResult,
      std::vector<std::string> const& projections, transaction::Methods* trxPtr,
      std::vector<size_t> const& coveringIndexAttributePositions, bool useRawDocumentPointers,
      std::vector<std::unique_ptr<NonConstExpression>>&& nonConstExpression,
      std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs,
      bool hasV8Expression, AstNode const* condition,
      std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast,
      IndexIteratorOptions options);

  IndexExecutorInfos() = delete;
  IndexExecutorInfos(IndexExecutorInfos&&) = default;
  IndexExecutorInfos(IndexExecutorInfos const&) = delete;
  ~IndexExecutorInfos() = default;

  ExecutionEngine* getEngine() { return _engine; }
  Collection const* getCollection() { return _collection; }
  Variable const* getOutVariable() { return _outVariable; }
  std::vector<std::string> const& getProjections() { return _projections; }
  transaction::Methods* getTrxPtr() { return _trxPtr; }
  std::vector<size_t> const& getCoveringIndexAttributePositions() {
    return _coveringIndexAttributePositions;
  }
  bool getProduceResult() { return _produceResult; }
  bool getUseRawDocumentPointers() { return _useRawDocumentPointers; }
  std::vector<transaction::Methods::IndexHandle> const& getIndexes() {
    return _indexes;
  }
  AstNode const* getCondition() { return _condition; }
  bool getV8Expression() { return _hasV8Expression; }
  RegisterId getOutputRegisterId() { return _outputRegisterId; }
  std::vector<std::unique_ptr<NonConstExpression>> const& getNonConstExpressions() {
    return _nonConstExpression;
  }
  bool hasMultipleExpansions() { return _hasMultipleExpansions; }

  /// @brief whether or not all indexes are accessed in reverse order
  IndexIteratorOptions getOptions() const { return _options; }
  bool isAscending() { return _options.ascending; }

  Ast* getAst() { return _ast; }

  std::vector<Variable const*> const& getExpInVars() const {
    return _expInVars;
  }
  std::vector<RegisterId> const& getExpInRegs() const { return _expInRegs; }

  // setter
  void setHasMultipleExpansions(bool flag) { _hasMultipleExpansions = flag; }

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

  /// @brief the index sort order - this is the same order for all indexes
  IndexIteratorOptions _options;

  RegisterId _outputRegisterId;
  ExecutionEngine* _engine;
  Collection const* _collection;
  Variable const* _outVariable;
  std::vector<std::string> const& _projections;
  transaction::Methods* _trxPtr;
  std::vector<Variable const*> _expInVars;  // input variables for expresseion
  std::vector<RegisterId> _expInRegs;       // input registers for expression

  std::vector<size_t> const& _coveringIndexAttributePositions;
  bool _useRawDocumentPointers;
  std::vector<std::unique_ptr<NonConstExpression>> _nonConstExpression;
  bool _produceResult;
  /// @brief Counter how many documents have been returned/skipped
  ///        during one call. Retained during WAITING situations.
  ///        Needs to be 0 after we return a result.
  bool _hasV8Expression;
};

/**
 * @brief Implementation of Index Node
 */
class IndexExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = false;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
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

 public:
  typedef std::function<void(InputAqlItemRow&, OutputAqlItemRow&, arangodb::velocypack::Slice, RegisterId)> DocumentProducingFunction;

  void setProducingFunction(DocumentProducingFunction documentProducer) {
    _documentProducer = std::move(documentProducer);
  }

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number fo rows not supported");
  }

 private:
  bool advanceCursor();
  void executeExpressions(InputAqlItemRow& input);
  bool initIndexes(InputAqlItemRow& input);

  /// @brief create an iterator object
  void createCursor();

  /// @brief continue fetching of documents
  bool readIndex(IndexIterator::DocumentCallback const&, bool& hasWritten);

  /// @brief reset the cursor at given position
  void resetCursor(size_t pos) { _cursors[pos]->reset(); };

  /// @brief reset and initialize the cursor at given position
  OperationCursor* resetCursor(size_t pos, std::unique_ptr<OperationCursor> cursor) {
    _cursors[pos] = std::move(cursor);
    return getCursor(pos);
  }

  /// @brief order a cursor for the index at the specified position
  OperationCursor* orderCursor(size_t currentIndex);

  /// @brief set a new cursor
  void setCursor(arangodb::OperationCursor* cursor) { _cursor = cursor; }

  inline arangodb::OperationCursor* getCursor() { return _cursor; }
  inline arangodb::OperationCursor* getCursor(size_t pos) {
    return _cursors[pos].get();
  }

  void setIndexesExhausted(bool flag) { _indexesExhausted = flag; }
  bool getIndexesExhausted() { return _indexesExhausted; }

  bool isLastIndex() { return _isLastIndex; }
  void setIsLastIndex(bool flag) { _isLastIndex = flag; }

  void setCurrentIndex(size_t pos) { _currentIndex = pos; }
  void decrCurrentIndex() {
    _currentIndex--;
  }
  void incrCurrentIndex() {
    _currentIndex++;
  }
  size_t getCurrentIndex() const noexcept { return _currentIndex; }

 private:
  Infos& _infos;
  Fetcher& _fetcher;
  DocumentProducingFunction _documentProducer;
  ExecutionState _state;
  InputAqlItemRow _input;

  /// @brief whether or not we are allowed to use the covering index
  /// optimization in a callback
  bool _allowCoveringIndexOptimization;

  /// @brief _cursor: holds the current index cursor found using
  /// createCursor (if any) so that it can be read in chunks and not
  /// necessarily all at once.
  arangodb::OperationCursor* _cursor;

  /// @brief a vector of cursors for the index block. cursors can be
  /// reused
  std::vector<std::unique_ptr<OperationCursor>> _cursors;

  /// @brief current position in _indexes
  size_t _currentIndex;

  /// @brief set of already returned documents. Used to make the result distinct
  std::unordered_set<TRI_voc_rid_t> _alreadyReturned;

  /// @brief Flag if all indexes are exhausted to be maintained accross several
  /// getSome() calls
  bool _indexesExhausted;

  /// @brief Flag if the current index pointer is the last of the list.
  ///        Used in uniqueness checks.
  bool _isLastIndex;
};

}  // namespace aql
}  // namespace arangodb

#endif
