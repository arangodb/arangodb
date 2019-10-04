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
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"

#include <memory>

namespace arangodb {
struct OperationCursor;
namespace aql {

class ExecutionEngine;
class ExecutorInfos;
class InputAqlItemRow;

template <BlockPassthrough>
class SingleRowFetcher;

struct AstNode;
struct Collection;
struct NonConstExpression;

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

  ExecutionEngine* getEngine() const;
  Collection const* getCollection() const;
  Variable const* getOutVariable() const;
  std::vector<std::string> const& getProjections() const noexcept;
  transaction::Methods* getTrxPtr() const noexcept;
  std::vector<size_t> const& getCoveringIndexAttributePositions() const noexcept;
  bool getProduceResult() const noexcept;
  bool getUseRawDocumentPointers() const noexcept;
  std::vector<transaction::Methods::IndexHandle> const& getIndexes() const noexcept;
  AstNode const* getCondition() const noexcept;
  bool getV8Expression() const noexcept;
  RegisterId getOutputRegisterId() const noexcept;
  std::vector<std::unique_ptr<NonConstExpression>> const& getNonConstExpressions() const noexcept;
  bool hasMultipleExpansions() const noexcept;

  /// @brief whether or not all indexes are accessed in reverse order
  IndexIteratorOptions getOptions() const;
  bool isAscending() const noexcept;

  Ast* getAst() const noexcept;

  std::vector<Variable const*> const& getExpInVars() const noexcept;
  std::vector<RegisterId> const& getExpInRegs() const noexcept;

  // setter
  void setHasMultipleExpansions(bool flag);

  bool hasNonConstParts() const;

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

  /// @brief the index sort order - this is the same order for all indexes
  IndexIteratorOptions _options;

  ExecutionEngine* _engine;
  Collection const* _collection;
  Variable const* _outVariable;
  std::vector<std::string> const& _projections;
  std::vector<size_t> const& _coveringIndexAttributePositions;
  transaction::Methods* _trxPtr;
  std::vector<Variable const*> _expInVars;  // input variables for expresseion
  std::vector<RegisterId> _expInRegs;       // input registers for expression

  std::vector<std::unique_ptr<NonConstExpression>> _nonConstExpression;

  RegisterId _outputRegisterId;
  /// @brief true if one of the indexes uses more than one expanded attribute,
  /// e.g. the index is on values[*].name and values[*].type
  bool _hasMultipleExpansions;

  bool _useRawDocumentPointers;
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
 private:
  struct CursorReader {
   public:
    CursorReader(IndexExecutorInfos const& infos, AstNode const* condition,
                 transaction::Methods::IndexHandle const& index,
                 DocumentProducingFunctionContext& context, bool checkUniqueness);
    bool readIndex(OutputAqlItemRow& output);
    size_t skipIndex(size_t toSkip);
    void reset();

    bool hasMore() const;

    bool isCovering() const;

    CursorReader(const CursorReader&) = delete;
    CursorReader& operator=(const CursorReader&) = delete;
    CursorReader(CursorReader&& other) noexcept;

   private:
    enum Type { NoResult, Covering, Document };

    IndexExecutorInfos const& _infos;
    AstNode const* _condition;
    transaction::Methods::IndexHandle const& _index;
    std::unique_ptr<OperationCursor> _cursor;
    Type const _type;

    union CallbackMethod {
      IndexIterator::LocalDocumentIdCallback noProduce;
      DocumentProducingFunction produce;

      CallbackMethod() : noProduce(nullptr) {}
      ~CallbackMethod() {}
    } _callback;
  };

 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
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
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);
  std::tuple<ExecutionState, Stats, size_t> skipRows(size_t toSkip);

 public:
  void initializeCursor();

 private:
  bool advanceCursor();
  void executeExpressions(InputAqlItemRow& input);
  void initIndexes(InputAqlItemRow& input);

  CursorReader& getCursor();

  bool needsUniquenessCheck() const noexcept;

 private:
  Infos& _infos;
  Fetcher& _fetcher;
  DocumentProducingFunctionContext _documentProducingFunctionContext;
  ExecutionState _state;
  InputAqlItemRow _input;

  /// @brief a vector of cursors for the index block
  /// cursors can be reused
  std::vector<CursorReader> _cursors;

  /// @brief current position in _indexes
  size_t _currentIndex;

  /// @brief Count how many documents have been skipped during one call.
  ///        Retained during WAITING situations.
  ///        Needs to be 0 after we return a result.
  size_t _skipped;
};

}  // namespace aql
}  // namespace arangodb

#endif
