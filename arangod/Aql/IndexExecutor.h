////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/Ast.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionState.h"
#include "Aql/IndexNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"

#include <memory>

namespace arangodb {
class IndexIterator;
struct ResourceMonitor;

namespace aql {

class ExecutionEngine;
class ExecutorExpressionContext;
class RegisterInfos;
class Expression;
class InputAqlItemRow;
class Projections;
class QueryContext;

template<BlockPassthrough>
class SingleRowFetcher;

struct AstNode;
struct Collection;
struct NonConstExpression;

class IndexExecutorInfos {
 public:
  IndexExecutorInfos(
      RegisterId outputRegister, QueryContext& query,
      Collection const* collection, Variable const* outVariable,
      bool produceResult, Expression* filter, aql::Projections projections,
      aql::Projections filterProjections,
      std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs,
      NonConstExpressionContainer&& nonConstExpressions, bool count,
      ReadOwnWrites readOwnWrites, AstNode const* condition,
      bool oneIndexCondition,
      std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast,
      IndexIteratorOptions options,
      IndexNode::IndexValuesVars const& outNonMaterializedIndVars,
      IndexNode::IndexValuesRegisters&& outNonMaterializedIndRegs);

  IndexExecutorInfos() = delete;
  IndexExecutorInfos(IndexExecutorInfos&&) = default;
  IndexExecutorInfos(IndexExecutorInfos const&) = delete;
  ~IndexExecutorInfos() = default;

  Collection const* getCollection() const;
  Variable const* getOutVariable() const;
  arangodb::aql::Projections const& getProjections() const noexcept;
  arangodb::aql::Projections const& getFilterProjections() const noexcept;
  aql::QueryContext& query() noexcept;
  Expression* getFilter() const noexcept;
  ResourceMonitor& getResourceMonitor() noexcept;
  bool getProduceResult() const noexcept;
  std::vector<transaction::Methods::IndexHandle> const& getIndexes()
      const noexcept;
  AstNode const* getCondition() const noexcept;
  bool getV8Expression() const noexcept;
  RegisterId getOutputRegisterId() const noexcept;
  std::vector<std::unique_ptr<NonConstExpression>> const&
  getNonConstExpressions() const noexcept;
  bool hasMultipleExpansions() const noexcept;
  bool getCount() const noexcept;

  ReadOwnWrites canReadOwnWrites() const noexcept { return _readOwnWrites; }

  /// @brief whether or not all indexes are accessed in reverse order
  IndexIteratorOptions getOptions() const;
  bool isAscending() const noexcept;

  Ast* getAst() const noexcept;

  std::vector<std::pair<VariableId, RegisterId>> const& getVarsToRegister()
      const noexcept;

  std::vector<std::pair<VariableId, RegisterId>> const&
  getFilterVarsToRegister() const noexcept;

  bool hasNonConstParts() const;

  bool isLateMaterialized() const noexcept {
    return !_outNonMaterializedIndRegs.second.empty();
  }

  IndexNode::IndexValuesVars const& getOutNonMaterializedIndVars()
      const noexcept {
    return _outNonMaterializedIndVars;
  }

  IndexNode::IndexValuesRegisters const& getOutNonMaterializedIndRegs()
      const noexcept {
    return _outNonMaterializedIndRegs;
  }

  bool isOneIndexCondition() const noexcept { return _oneIndexCondition; }

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

  QueryContext& _query;
  Collection const* _collection;
  Variable const* _outVariable;
  Expression* _filter;
  arangodb::aql::Projections _projections;
  arangodb::aql::Projections _filterProjections;

  std::vector<std::pair<VariableId, RegisterId>> _filterVarsToRegs;

  NonConstExpressionContainer _nonConstExpressions;

  RegisterId _outputRegisterId;

  IndexNode::IndexValuesVars const& _outNonMaterializedIndVars;
  IndexNode::IndexValuesRegisters _outNonMaterializedIndRegs;

  /// @brief true if one of the indexes uses more than one expanded attribute,
  /// e.g. the index is on values[*].name and values[*].type
  bool const _hasMultipleExpansions;

  bool const _produceResult;

  /// @brief Counter how many documents have been returned/skipped
  ///        during one call. Retained during WAITING situations.
  ///        Needs to be 0 after we return a result.
  bool const _count;

  bool const _oneIndexCondition;

  ReadOwnWrites const _readOwnWrites;
};

/**
 * @brief Implementation of Index Node
 */
class IndexExecutor {
 private:
  struct CursorStats {
    std::uint64_t cursorsCreated = 0;
    std::uint64_t cursorsRearmed = 0;
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;

    void incrCursorsCreated(std::uint64_t value = 1) noexcept;
    void incrCursorsRearmed(std::uint64_t value = 1) noexcept;
    void incrCacheHits(std::uint64_t value = 1) noexcept;
    void incrCacheMisses(std::uint64_t value = 1) noexcept;

    [[nodiscard]] std::uint64_t getAndResetCursorsCreated() noexcept;
    [[nodiscard]] std::uint64_t getAndResetCursorsRearmed() noexcept;
    [[nodiscard]] std::uint64_t getAndResetCacheHits() noexcept;
    [[nodiscard]] std::uint64_t getAndResetCacheMisses() noexcept;
  };

  struct CursorReader {
   public:
    CursorReader(transaction::Methods& trx, IndexExecutorInfos& infos,
                 AstNode const* condition, std::shared_ptr<Index> const& index,
                 DocumentProducingFunctionContext& context,
                 CursorStats& cursorStats, bool checkUniqueness);
    bool readIndex(OutputAqlItemRow& output,
                   DocumentProducingFunctionContext const&
                       documentProducingFunctionContext);
    size_t skipIndex(size_t toSkip);
    void reset();

    bool hasMore() const;

    bool isCovering() const;

    CursorReader(const CursorReader&) = delete;
    CursorReader& operator=(const CursorReader&) = delete;
    CursorReader(CursorReader&& other) noexcept = default;

   private:
    enum Type {
      // no need to produce any result. we can scan over the index
      // but do not have to look into its values
      NoResult,

      // index covers all projections of the query. we can get
      // away with reading data from the index only
      Covering,

      // index covers the IndexNode's filter condition only,
      // but not the rest of the query. that means we can use the
      // index data to evaluate the IndexNode's post-filter condition,
      // but for any entries that pass the filter, we will need to
      // read the full documents in addition
      CoveringFilterOnly,

      // index does not cover the required data. we will need to
      // read the full documents for all index entries
      Document,

      // late materialization
      LateMaterialized,

      // we only need to count the number of index entries
      Count
    };

    transaction::Methods& _trx;
    IndexExecutorInfos& _infos;
    AstNode const* _condition;
    std::shared_ptr<Index> const& _index;
    std::unique_ptr<IndexIterator> _cursor;
    DocumentProducingFunctionContext& _context;
    CursorStats& _cursorStats;
    Type const _type;
    bool const _checkUniqueness;

    // Only one of _documentProducer and _documentNonProducer is set at a time,
    // depending on _type. As std::function is not trivially destructible, it's
    // safer not to use a union.
    IndexIterator::LocalDocumentIdCallback _documentNonProducer;
    IndexIterator::DocumentCallback _documentProducer;
    IndexIterator::DocumentCallback _documentSkipper;
    IndexIterator::CoveringCallback _coveringProducer;
    IndexIterator::CoveringCallback _coveringSkipper;
  };

 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = IndexExecutorInfos;
  using Stats = IndexStats;

  IndexExecutor() = delete;
  IndexExecutor(IndexExecutor&&) = delete;
  IndexExecutor(IndexExecutor const&) = delete;
  IndexExecutor(Fetcher& fetcher, Infos&);
  ~IndexExecutor();

  /**
   * @brief This Executor in some cases knows how many rows it will produce and
   * most by itself
   */
  [[nodiscard]] auto expectedNumberOfRows(AqlItemBlockInputRange const& input,
                                          AqlCall const& call) const noexcept
      -> size_t;

  auto produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  void initializeCursor();

 private:
  bool advanceCursor();
  void executeExpressions(InputAqlItemRow const& input);
  void initIndexes(InputAqlItemRow const& input);

  CursorReader& getCursor();

  bool needsUniquenessCheck() const noexcept;

  auto returnState() const noexcept -> ExecutorState;

 private:
  transaction::Methods _trx;
  InputAqlItemRow _input;
  ExecutorState _state;

  DocumentProducingFunctionContext _documentProducingFunctionContext;
  Infos& _infos;
  std::unique_ptr<ExecutorExpressionContext> _expressionContext;

  // an AST owned by the IndexExecutor, used to store data of index
  // expressions
  Ast _ast;

  /// @brief a vector of cursors for the index block
  /// cursors can be reused
  std::vector<CursorReader> _cursors;

  /// @brief current position in _indexes
  size_t _currentIndex;

  /// @brief Count how many documents have been skipped during one call.
  ///        Retained during WAITING situations.
  ///        Needs to be 0 after we return a result.
  size_t _skipped;

  /// statistics for cursors. is shared by reference with CursorReader instances
  CursorStats _cursorStats;
};

}  // namespace aql
}  // namespace arangodb
