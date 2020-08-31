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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraversalExecutor.h"

#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/system-compiler.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::traverser;

TraversalExecutorInfos::TraversalExecutorInfos(
    std::unique_ptr<Traverser>&& traverser,
    std::unordered_map<OutputName, RegisterId, OutputNameHash> registerMapping,
    std::string fixedSource, RegisterId inputRegister,
    std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables)
    : _traverser(std::move(traverser)),
      _registerMapping(std::move(registerMapping)),
      _fixedSource(std::move(fixedSource)),
      _inputRegister(inputRegister),
      _filterConditionVariables(std::move(filterConditionVariables)) {
  TRI_ASSERT(_traverser != nullptr);
  // _fixedSource XOR _inputRegister
  // note: _fixedSource can be the empty string here
  TRI_ASSERT(_fixedSource.empty() ||
             (!_fixedSource.empty() && _inputRegister == RegisterPlan::MaxRegisterId));
}

Traverser& TraversalExecutorInfos::traverser() {
  TRI_ASSERT(_traverser != nullptr);
  return *_traverser.get();
}

bool TraversalExecutorInfos::usesOutputRegister(OutputName type) const {
  return _registerMapping.find(type) != _registerMapping.end();
}

bool TraversalExecutorInfos::useVertexOutput() const {
  return usesOutputRegister(OutputName::VERTEX);
}

bool TraversalExecutorInfos::useEdgeOutput() const {
  return usesOutputRegister(OutputName::EDGE);
}

bool TraversalExecutorInfos::usePathOutput() const {
  return usesOutputRegister(OutputName::PATH);
}

static std::string typeToString(TraversalExecutorInfos::OutputName type) {
  switch (type) {
    case TraversalExecutorInfos::VERTEX:
      return std::string{"VERTEX"};
    case TraversalExecutorInfos::EDGE:
      return std::string{"EDGE"};
    case TraversalExecutorInfos::PATH:
      return std::string{"PATH"};
    default:
      return std::string{"<INVALID("} + std::to_string(type) + ")>";
  }
}

RegisterId TraversalExecutorInfos::findRegisterChecked(OutputName type) const {
  auto const& it = _registerMapping.find(type);
  if (ADB_UNLIKELY(it == _registerMapping.end())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic error: requested unused register type " + typeToString(type));
  }
  return it->second;
}

RegisterId TraversalExecutorInfos::getOutputRegister(OutputName type) const {
  TRI_ASSERT(usesOutputRegister(type));
  return findRegisterChecked(type);
}

RegisterId TraversalExecutorInfos::vertexRegister() const {
  return getOutputRegister(OutputName::VERTEX);
}

RegisterId TraversalExecutorInfos::edgeRegister() const {
  return getOutputRegister(OutputName::EDGE);
}

RegisterId TraversalExecutorInfos::pathRegister() const {
  return getOutputRegister(OutputName::PATH);
}

bool TraversalExecutorInfos::usesFixedSource() const {
  return _inputRegister == RegisterPlan::MaxRegisterId;
}

std::string const& TraversalExecutorInfos::getFixedSource() const {
  TRI_ASSERT(usesFixedSource());
  return _fixedSource;
}

RegisterId TraversalExecutorInfos::getInputRegister() const {
  TRI_ASSERT(!usesFixedSource());
  TRI_ASSERT(_inputRegister != RegisterPlan::MaxRegisterId);
  return _inputRegister;
}

std::vector<std::pair<Variable const*, RegisterId>> const& TraversalExecutorInfos::filterConditionVariables() const {
  return _filterConditionVariables;
}

TraversalExecutor::TraversalExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _inputRow{CreateInvalidInputRowHint{}}, _traverser(infos.traverser()) {
  // reset the traverser, so that no residual state is left in it. This is
  // important because the TraversalExecutor is sometimes reconstructed (in
  // place) with the same TraversalExecutorInfos as before. Those infos contain
  // the traverser which might contain state from a previous run.
  _traverser.done();
}

TraversalExecutor::~TraversalExecutor() {
  auto opts = _traverser.options();
  if (opts != nullptr && opts->usesPrune()) {
    auto* evaluator = opts->getPruneEvaluator();
    if (evaluator != nullptr) {
      // The InAndOutRowExpressionContext in the PruneExpressionEvaluator holds
      // an InputAqlItemRow. As the Plan holds the PruneExpressionEvaluator and
      // is destroyed after the Engine, this must be deleted by
      // unPrepareContext() - otherwise, the SharedAqlItemBlockPtr referenced by
      // the row will return its AqlItemBlock to an already destroyed
      // AqlItemBlockManager.
      evaluator->unPrepareContext();
    }
  }
};

auto TraversalExecutor::doOutput(OutputAqlItemRow& output) -> void {
  while (!output.isFull() && _traverser.hasMore() && _traverser.next()) {
    TRI_ASSERT(_inputRow.isInitialized());

    // traverser now has next v, e, p values
    if (_infos.useVertexOutput()) {
      AqlValue vertex = _traverser.lastVertexToAqlValue();
      AqlValueGuard guard{vertex, true};
      output.moveValueInto(_infos.vertexRegister(), _inputRow, guard);
    }
    if (_infos.useEdgeOutput()) {
      AqlValue edge = _traverser.lastEdgeToAqlValue();
      AqlValueGuard guard{edge, true};
      output.moveValueInto(_infos.edgeRegister(), _inputRow, guard);
    }
    if (_infos.usePathOutput()) {
      transaction::BuilderLeaser tmp(_traverser.trx());
      AqlValue path = _traverser.pathToAqlValue(*tmp.builder());
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

auto TraversalExecutor::doSkip(AqlCall& call) -> size_t {
  auto skip = size_t{0};

  while (call.shouldSkip() && _traverser.hasMore() && _traverser.next()) {
    TRI_ASSERT(_inputRow.isInitialized());
    skip++;
    call.didSkip(1);
  }

  return skip;
}

auto TraversalExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TraversalStats stats;
  ExecutorState state{ExecutorState::HASMORE};

  while (true) {
    if (_traverser.hasMore()) {
      TRI_ASSERT(_inputRow.isInitialized());
      doOutput(output);

      if (output.isFull()) {
        if (_traverser.hasMore()) {
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

  stats.addFiltered(_traverser.getAndResetFilteredPaths());
  stats.addScannedIndex(_traverser.getAndResetReadDocuments());
  stats.addHttpRequests(_traverser.getAndResetHttpRequests());

  return {state, stats, AqlCall{}};
}

auto TraversalExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TraversalStats stats{};
  auto skipped = size_t{0};

  while (true) {
    skipped += doSkip(call);

    stats.addFiltered(_traverser.getAndResetFilteredPaths());
    stats.addScannedIndex(_traverser.getAndResetReadDocuments());
    stats.addHttpRequests(_traverser.getAndResetHttpRequests());

    if (!_traverser.hasMore()) {
      if (!initTraverser(input)) {
        return {input.upstreamState(), stats, skipped, AqlCall{}};
      }
    } else {
      TRI_ASSERT(call.getOffset() == 0);
      return {ExecutorState::HASMORE, stats, skipped, AqlCall{}};
    }
  }
}

//
// Set a new start vertex for traversal, for this fetch inputs
// from input until we are either successful or input is unwilling
// to give us more.
//
// TODO: this is quite a big function, refactor
bool TraversalExecutor::initTraverser(AqlItemBlockInputRange& input) {
  _traverser.clear();
  auto opts = _traverser.options();
  opts->clearVariableValues();

  // Now reset the traverser
  // NOTE: It is correct to ask for whether there is a data row here
  //       even if we're using a constant start vertex, as we expect
  //       to provide output for every input row
  while (input.hasDataRow()) {
    // Try to acquire a starting vertex
    std::tie(std::ignore, _inputRow) = input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
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

    std::string sourceString;
    TRI_ASSERT(_inputRow.isInitialized());

    if (_infos.usesFixedSource()) {
      sourceString = _infos.getFixedSource();
    } else {
      AqlValue const& in = _inputRow.getValue(_infos.getInputRegister());
      if (in.isObject()) {
        try {
          sourceString = _traverser.options()->trx()->extractIdString(in.slice());
        } catch (...) {
          // on purpose ignore this error.
        }
      } else if (in.isString()) {
        sourceString = in.slice().copyString();
      }
    }

    auto pos = sourceString.find('/');

    if (pos == std::string::npos) {
      _traverser.options()->query().warnings().registerWarning(
        TRI_ERROR_BAD_PARAMETER,
        "Invalid input for traversal: Only "
        "id strings or objects with _id are "
        "allowed");
    } else {
      _traverser.setStartVertex(sourceString);
      TRI_ASSERT(_inputRow.isInitialized());
      return true;
    }
  }
  return false;
}
