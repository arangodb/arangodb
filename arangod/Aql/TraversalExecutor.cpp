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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraversalExecutor.h"

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutorExpressionContext.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/system-compiler.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"

#include "Graph/algorithm-aliases.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::traverser;

namespace {
auto toHashedStringRef(std::string const& id) -> arangodb::velocypack::HashedStringRef {
  return arangodb::velocypack::HashedStringRef(id.data(), static_cast<uint32_t>(id.length()));
}
}  // namespace

template <class FinderType>
TraversalExecutorInfos<FinderType>::TraversalExecutorInfos(
    std::unique_ptr<FinderType>&& finder, QueryContext& query,
    std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId, TraversalExecutorInfosHelper::OutputNameHash> registerMapping,
    std::string fixedSource, RegisterId inputRegister,
    std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables, Ast* ast)
    : _query(query),
      _finder(std::move(finder)),
      _registerMapping(std::move(registerMapping)),
      _fixedSource(std::move(fixedSource)),
      _inputRegister(inputRegister),
      _filterConditionVariables(std::move(filterConditionVariables)),
      _ast(ast) {
  TRI_ASSERT(_finder != nullptr);
  // _fixedSource XOR _inputRegister
  // note: _fixedSource can be the empty string here
  TRI_ASSERT(_fixedSource.empty() ||
             (!_fixedSource.empty() && _inputRegister.value() == RegisterId::maxRegisterId));
  // All Nodes are located in the AST it cannot be non existing.
  TRI_ASSERT(_ast != nullptr);
}

template <class FinderType>
FinderType& TraversalExecutorInfos<FinderType>::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

template <class FinderType>
QueryContext& TraversalExecutorInfos<FinderType>::query() noexcept {
  return _query;
}

template <class FinderType>
bool TraversalExecutorInfos<FinderType>::usesOutputRegister(TraversalExecutorInfosHelper::OutputName type) const {
  return _registerMapping.find(type) != _registerMapping.end();
}

template <class FinderType>
bool TraversalExecutorInfos<FinderType>::useVertexOutput() const {
  return usesOutputRegister(TraversalExecutorInfosHelper::OutputName::VERTEX);
}

template <class FinderType>
bool TraversalExecutorInfos<FinderType>::useEdgeOutput() const {
  return usesOutputRegister(TraversalExecutorInfosHelper::OutputName::EDGE);
}

template <class FinderType>
bool TraversalExecutorInfos<FinderType>::usePathOutput() const {
  return usesOutputRegister(TraversalExecutorInfosHelper::OutputName::PATH);
}

template <class FinderType>
Ast* TraversalExecutorInfos<FinderType>::getAst() const {
  return _ast;
}

template <class FinderType>
std::string TraversalExecutorInfos<FinderType>::typeToString(TraversalExecutorInfosHelper::OutputName type) const {
  switch (type) {
    case TraversalExecutorInfosHelper::VERTEX:
      return std::string{"VERTEX"};
    case TraversalExecutorInfosHelper::EDGE:
      return std::string{"EDGE"};
    case TraversalExecutorInfosHelper::PATH:
      return std::string{"PATH"};
    default:
      return std::string{"<INVALID("} + std::to_string(type) + ")>";
  }
}

template <class FinderType>
RegisterId TraversalExecutorInfos<FinderType>::findRegisterChecked(TraversalExecutorInfosHelper::OutputName type) const {
  auto const& it = _registerMapping.find(type);
  if (ADB_UNLIKELY(it == _registerMapping.end())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic error: requested unused register type " + typeToString(type));
  }
  return it->second;
}

template <class FinderType>
RegisterId TraversalExecutorInfos<FinderType>::getOutputRegister(TraversalExecutorInfosHelper::OutputName type) const {
  TRI_ASSERT(usesOutputRegister(type));
  return findRegisterChecked(type);
}

template <class FinderType>
RegisterId TraversalExecutorInfos<FinderType>::vertexRegister() const {
  return getOutputRegister(TraversalExecutorInfosHelper::OutputName::VERTEX);
}

template <class FinderType>
RegisterId TraversalExecutorInfos<FinderType>::edgeRegister() const {
  return getOutputRegister(TraversalExecutorInfosHelper::OutputName::EDGE);
}

template <class FinderType>
RegisterId TraversalExecutorInfos<FinderType>::pathRegister() const {
  return getOutputRegister(TraversalExecutorInfosHelper::OutputName::PATH);
}

template <class FinderType>
bool TraversalExecutorInfos<FinderType>::usesFixedSource() const {
  return _inputRegister.value() == RegisterId::maxRegisterId;
}

template <class FinderType>
std::string const& TraversalExecutorInfos<FinderType>::getFixedSource() const {
  TRI_ASSERT(usesFixedSource());
  return _fixedSource;
}

template <class FinderType>
RegisterId TraversalExecutorInfos<FinderType>::getInputRegister() const {
  TRI_ASSERT(!usesFixedSource());
  TRI_ASSERT(_inputRegister.isValid());
  return _inputRegister;
}

template <class FinderType>
std::vector<std::pair<Variable const*, RegisterId>> const&
TraversalExecutorInfos<FinderType>::filterConditionVariables() const {
  return _filterConditionVariables;
}

template <class FinderType>
TraversalExecutor<FinderType>::TraversalExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _trx(infos.query().newTrxContext()),
      _inputRow{CreateInvalidInputRowHint{}},
      _finder(infos.finder()) {
  // reset the traverser, so that no residual state is left in it. This is
  // important because the TraversalExecutor is sometimes reconstructed (in
  // place) with the same TraversalExecutorInfos<FinderType> as before. Those
  // infos contain the traverser which might contain state from a previous run.
  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    _finder.done();
  } else {
    // refactored variant
    _finder.clear(false);  // TODO [GraphRefactor]: check - potentially call reset instead
  }
}

template <class FinderType>
TraversalExecutor<FinderType>::~TraversalExecutor() {
  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    auto opts = _finder.options();
    if (opts != nullptr) {
      // The InAndOutRowExpressionContext in the PruneExpressionEvaluator
      // holds an InputAqlItemRow. As the Plan holds the
      // PruneExpressionEvaluator and is destroyed after the Engine, this must
      // be deleted by unPrepareContext() - otherwise, the
      // SharedAqlItemBlockPtr referenced by the row will return its
      // AqlItemBlock to an already destroyed AqlItemBlockManager.
      if (opts->usesPrune()) {
        auto* evaluator = opts->getPruneEvaluator();
        if (evaluator != nullptr) {
          evaluator->unPrepareContext();
        }
      }
      if (opts->usesPostFilter()) {
        auto* evaluator = opts->getPostFilterEvaluator();
        if (evaluator != nullptr) {
          evaluator->unPrepareContext();
        }
      }
    }
  } else {
    _finder.clear(false);
  }
};

template <class FinderType>
auto TraversalExecutor<FinderType>::doOutput(OutputAqlItemRow& output) -> void {

  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    while (!output.isFull() && _finder.hasMore() && _finder.next()) {
      TRI_ASSERT(_inputRow.isInitialized());

      // traverser now has next v, e, p values
      if (_infos.useVertexOutput()) {
        AqlValue vertex = _finder.lastVertexToAqlValue();
        AqlValueGuard guard{vertex, true};
        output.moveValueInto(_infos.vertexRegister(), _inputRow, guard);
      }
      if (_infos.useEdgeOutput()) {
        AqlValue edge = _finder.lastEdgeToAqlValue();
        AqlValueGuard guard{edge, true};
        output.moveValueInto(_infos.edgeRegister(), _inputRow, guard);
      }
      if (_infos.usePathOutput()) {
        transaction::BuilderLeaser tmp(_finder.trx());
        AqlValue path = _finder.pathToAqlValue(*tmp.builder());
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.pathRegister(), _inputRow, guard);
      }

      // No output is requested from the register plan. We still need
      // to copy the inputRow for the query to yield correct results.
      if (!_infos.useVertexOutput() && !_infos.useEdgeOutput() && !_infos.usePathOutput()) {
        output.copyRow(_inputRow);
      }

      output.advanceRow();
    }
  } else {
    // Refactored variant
    while (auto currentPath = _finder.getNextPath()) {
      TRI_ASSERT(_inputRow.isInitialized());

      // traverser now has next v, e, p values
      transaction::BuilderLeaser tmp{&_trx};
      LOG_DEVEL << "[GraphRefactor]: doOutput - path found";

      // Vertex variable (v)
      if (_infos.useVertexOutput()) {
        tmp->clear();
        currentPath->lastVertexToVelocyPack(*tmp.builder());
        AqlValue path{tmp->slice()};
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.vertexRegister(), _inputRow, guard);
      }

      // Edge variable (e)
      if (_infos.useEdgeOutput()) {
        tmp->clear();
        currentPath->lastEdgeToVelocyPack(*tmp.builder());
        AqlValue path{tmp->slice()};
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.edgeRegister(), _inputRow, guard);
      }

      // Path variable (p)
      if (_infos.usePathOutput()) {
        tmp->clear();
        currentPath->toVelocyPack(*tmp.builder());
        AqlValue path{tmp->slice()};
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.pathRegister(), _inputRow, guard);
      }

      // No output is requested from the register plan. We still need
      // to copy the inputRow for the query to yield correct results.
      if (!_infos.useVertexOutput() && !_infos.useEdgeOutput() && !_infos.usePathOutput()) {
        output.copyRow(_inputRow);
      }

      output.advanceRow();
    }
  }
}

template <class FinderType>
auto TraversalExecutor<FinderType>::doSkip(AqlCall& call) -> size_t {
  auto skip = size_t{0};

  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    while (call.shouldSkip() && _finder.hasMore() && _finder.next()) {
      TRI_ASSERT(_inputRow.isInitialized());
      skip++;
      call.didSkip(1);
    }
  } else {
    // refactored variant
    while (call.shouldSkip()) {
      if (_finder.skipPath()) {
        TRI_ASSERT(_inputRow.isInitialized());
        skip++;
        call.didSkip(1);
      }
    }
  }

  return skip;
}

template <class FinderType>
auto TraversalExecutor<FinderType>::produceRows(AqlItemBlockInputRange& input,
                                                OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TraversalStats oldStats;
  ExecutorState state{ExecutorState::HASMORE};

  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    while (true) {
      if (_finder.hasMore()) {
        TRI_ASSERT(_inputRow.isInitialized());
        doOutput(output);

        if (output.isFull()) {
          if (_finder.hasMore()) {
            state = ExecutorState::HASMORE;
          } else {
            state = input.upstreamState();
          }
          break;
        }
      } else {
        if (!initTraverser(input)) {
          state = input.upstreamState();
          break;
        }
        TRI_ASSERT(_inputRow.isInitialized());
      }
    }

    oldStats.addFiltered(_finder.getAndResetFilteredPaths());
    oldStats.addScannedIndex(_finder.getAndResetReadDocuments());
    oldStats.addHttpRequests(_finder.getAndResetHttpRequests());

    return {state, oldStats, AqlCall{}};
  } else {
    // refactored variant
    LOG_DEVEL << "[GraphRefactor]: produceRows";
    while (!output.isFull()) {
      if (_finder.isDone()) {
        LOG_DEVEL << "[GraphRefactor]: finder is done";
        if (!initTraverser(input)) {  // will set a new start vertex
          TRI_ASSERT(!input.hasDataRow());
          LOG_DEVEL << "[GraphRefactor]: initTraverser returned false";
          return {input.upstreamState(), stats(), AqlCall{}};
        }
      } else {
        LOG_DEVEL << "[GraphRefactor]: before doOutput";
        doOutput(output);
        LOG_DEVEL << "[GraphRefactor]: after doOutput";
      }
    }

    if (_finder.isDone()) {
      return {input.upstreamState(), stats(), AqlCall{}};
    } else {
      return {ExecutorState::HASMORE, stats(), AqlCall{}};
    }
  }

  TRI_ASSERT(false);
  return {state, oldStats, AqlCall{}};
}

template <class FinderType>
auto TraversalExecutor<FinderType>::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TraversalStats oldStats{};
  auto skipped = size_t{0};

  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    while (true) {
      skipped += doSkip(call);

      oldStats.addFiltered(_finder.getAndResetFilteredPaths());
      oldStats.addScannedIndex(_finder.getAndResetReadDocuments());
      oldStats.addHttpRequests(_finder.getAndResetHttpRequests());

      if (!_finder.hasMore()) {
        if (!initTraverser(input)) {
          return {input.upstreamState(), oldStats, skipped, AqlCall{}};
        }
      } else {
        TRI_ASSERT(call.getOffset() == 0);
        return {ExecutorState::HASMORE, oldStats, skipped, AqlCall{}};
      }
    }
  } else {
    // refactored variant
    while (call.shouldSkip()) {
      if (_finder.isDone()) {
        if (!initTraverser(input)) {
          return {input.upstreamState(), oldStats, skipped, AqlCall{}};
        }

        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), stats(), skipped, AqlCall{}};
      } else {
        if (_finder.skipPath()) {
          skipped++;
          call.didSkip(1);
        }
      }
    }

    if (_finder.isDone()) {
      return {input.upstreamState(), stats(), skipped, AqlCall{}};
    } else {
      return {ExecutorState::HASMORE, stats(), skipped, AqlCall{}};
    }
  }

  TRI_ASSERT(false);
}

//
// Set a new start vertex for traversal, for this fetch inputs
// from input until we are either successful or input is unwilling
// to give us more.
//
// TODO: this is quite a big function, refactor
template <class FinderType>
bool TraversalExecutor<FinderType>::initTraverser(AqlItemBlockInputRange& input) {
  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    _finder.clear();
    auto opts = _finder.options();
    opts->clearVariableValues();

    // Now reset the traverser
    // NOTE: It is correct to ask for whether there is a data row here
    //       even if we're using a constant start vertex, as we expect
    //       to provide output for every input row
    while (input.hasDataRow()) {
      // Try to acquire a starting vertex
      std::tie(std::ignore, _inputRow) =
          input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
      TRI_ASSERT(_inputRow.isInitialized());

      for (auto const& pair : _infos.filterConditionVariables()) {
        opts->setVariableValue(pair.first, _inputRow.getValue(pair.second));
      }

      if (opts->usesPrune()) {
        auto* evaluator = opts->getPruneEvaluator();
        // Replace by inputRow
        evaluator->prepareContext(_inputRow);
        TRI_ASSERT(_inputRow.isInitialized());
      }

      if (opts->usesPostFilter()) {
        auto* evaluator = opts->getPostFilterEvaluator();
        // Replace by inputRow
        evaluator->prepareContext(_inputRow);
        TRI_ASSERT(_inputRow.isInitialized());
      }

      // TODO [GraphRefactor]: Check how indices are being calculated
      opts->calculateIndexExpressions(_infos.getAst());

      std::string sourceString;
      TRI_ASSERT(_inputRow.isInitialized());

      if (_infos.usesFixedSource()) {
        sourceString = _infos.getFixedSource();
      } else {
        AqlValue const& in = _inputRow.getValue(_infos.getInputRegister());
        if (in.isObject()) {
          try {
            sourceString = _finder.options()->trx()->extractIdString(in.slice());
          } catch (...) {
            // on purpose ignore this error.
          }
        } else if (in.isString()) {
          sourceString = in.slice().copyString();
        }
      }

      auto pos = sourceString.find('/');

      if (pos == std::string::npos) {
        _finder.options()->query().warnings().registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for traversal: Only "
            "id strings or objects with _id are "
            "allowed");
      } else {
        _finder.setStartVertex(sourceString);
        TRI_ASSERT(_inputRow.isInitialized());
        return true;
      }
    }
    return false;
  } else {
    // refactored variant
    TRI_ASSERT(_finder.isDone());

    while (input.hasDataRow()) {
      std::tie(std::ignore, _inputRow) = input.nextDataRow();

      std::string sourceString;
      TRI_ASSERT(_inputRow.isInitialized());

      if (_infos.usesFixedSource()) {
        sourceString = _infos.getFixedSource();
      } else {
        AqlValue const& in = _inputRow.getValue(_infos.getInputRegister());
        if (in.isObject()) {
          try {
            sourceString = _trx.extractIdString(in.slice());
          } catch (...) {
            // on purpose ignore this error.
          }
        } else if (in.isString()) {
          sourceString = in.slice().copyString();
        }
      }

      auto pos = sourceString.find('/');

      if (pos == std::string::npos) {
        /*_finder.options()->query().warnings().registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for traversal: Only "
            "id strings or objects with _id are "
            "allowed");*/
        LOG_DEVEL << "[GraphRefactor] handle warning.";
      } else {
        /*
         * auto variableSlice = info.getVariables();
if (variableSlice.isArray()) {
  injectVariables(variableSlice);
  _smartTraverser->prepareIndexExpressions(_query.ast());
}
         */

        // inject variables
        //for (auto const& pair : _infos.filterConditionVariables()) {
        //opts->setVariableValue(pair.first, _inputRow.getValue(pair.second));
        //}

        // prepare index
        _finder.prepareIndexExpressions(_infos.getAst());

        // start actual search
        _finder.reset(toHashedStringRef(sourceString)); // TODO [GraphRefactor]: check sourceString memory
        TRI_ASSERT(_inputRow.isInitialized());
        return true;
      }
    }
    return false;
  }

  TRI_ASSERT(false);
}

template <class FinderType>
[[nodiscard]] auto TraversalExecutor<FinderType>::stats() -> Stats {
  if constexpr (std::is_same_v<FinderType, traverser::Traverser>) {
    // No Stats available on original variant
    return TraversalStats{};
  } else {
    return _finder.stealStats();
  }
}

// New refactored variants

/* SingleServerProvider Section */
template class ::arangodb::aql::TraversalExecutorInfos<::arangodb::graph::DFSEnumerator<
    arangodb::graph::SingleServerProvider<arangodb::graph::SingleServerProviderStep>, graph::VertexUniquenessLevel::NONE>>;

template class ::arangodb::aql::TraversalExecutorInfos<::arangodb::graph::DFSEnumerator<
    arangodb::graph::SingleServerProvider<arangodb::graph::SingleServerProviderStep>, graph::VertexUniquenessLevel::PATH>>;
template class ::arangodb::aql::TraversalExecutorInfos<::arangodb::graph::DFSEnumerator<
    arangodb::graph::SingleServerProvider<arangodb::graph::SingleServerProviderStep>, graph::VertexUniquenessLevel::GLOBAL>>;

template class ::arangodb::aql::TraversalExecutor<::arangodb::graph::DFSEnumerator<
    arangodb::graph::SingleServerProvider<arangodb::graph::SingleServerProviderStep>, graph::VertexUniquenessLevel::NONE>>;

template class ::arangodb::aql::TraversalExecutor<::arangodb::graph::DFSEnumerator<
    arangodb::graph::SingleServerProvider<arangodb::graph::SingleServerProviderStep>, graph::VertexUniquenessLevel::PATH>>;
template class ::arangodb::aql::TraversalExecutor<::arangodb::graph::DFSEnumerator<
    arangodb::graph::SingleServerProvider<arangodb::graph::SingleServerProviderStep>, graph::VertexUniquenessLevel::GLOBAL>>;

// Old non refactored variant
template class ::arangodb::aql::TraversalExecutorInfos<arangodb::traverser::Traverser>;
template class ::arangodb::aql::TraversalExecutor<arangodb::traverser::Traverser>;