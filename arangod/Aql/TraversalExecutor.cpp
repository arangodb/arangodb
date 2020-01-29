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
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep, std::unique_ptr<Traverser>&& traverser,
    std::unordered_map<OutputName, RegisterId, OutputNameHash> registerMapping,
    std::string fixedSource, RegisterId inputRegister,
    std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables)
    : ExecutorInfos(std::move(inputRegisters), std::move(outputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _traverser(std::move(traverser)),
      _registerMapping(std::move(registerMapping)),
      _fixedSource(std::move(fixedSource)),
      _inputRegister(inputRegister),
      _filterConditionVariables(std::move(filterConditionVariables)) {
  TRI_ASSERT(_traverser != nullptr);
  TRI_ASSERT(!_registerMapping.empty());
  // _fixedSource XOR _inputRegister
  // note: _fixedSource can be the empty string here
  TRI_ASSERT(_fixedSource.empty() ||
             (!_fixedSource.empty() && _inputRegister == RegisterPlan::MaxRegisterId));
}

TraversalExecutorInfos::TraversalExecutorInfos(TraversalExecutorInfos&& other) = default;

TraversalExecutorInfos::~TraversalExecutorInfos() = default;

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
    : _infos(infos), _inputRow{CreateInvalidInputRowHint{}}, _traverser(infos.traverser()) {}

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

// Shutdown query
std::pair<ExecutionState, Result> TraversalExecutor::shutdown(int errorCode) {
  _traverser.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, TraversalStats> TraversalExecutor::produceRows(OutputAqlItemRow& output) {
  // TODO: Remove me!
  TRI_ASSERT(false);
}

auto TraversalExecutor::doOutput(OutputAqlItemRow& output) -> void {
  // TODO check whether _traverser.hasMore is obsolete here
  while (!output.isFull() && _traverser.hasMore() && _traverser.next()) {
    TRI_ASSERT(_inputRow.isInitialized());

    // traverser now has next v, e, p values
    if (_infos.useVertexOutput()) {
      AqlValue vertex = _traverser.lastVertexToAqlValue();
      output.cloneValueInto(_infos.vertexRegister(), _inputRow, vertex);
    }
    if (_infos.useEdgeOutput()) {
      AqlValue edge = _traverser.lastEdgeToAqlValue();
      output.cloneValueInto(_infos.edgeRegister(), _inputRow, edge);
    }
    if (_infos.usePathOutput()) {
      transaction::BuilderLeaser tmp(_traverser.trx());
      AqlValue path = _traverser.pathToAqlValue(*tmp.builder());
      output.cloneValueInto(_infos.pathRegister(), _inputRow, path);
    }
    output.advanceRow();
  }
}

auto TraversalExecutor::doSkip(AqlCall& call) -> size_t {
  auto skip = size_t{0};

  while (call.shouldSkip() && _traverser.next()) {
    skip++;
    call.didSkip(1);
  }

  return skip;
}

auto TraversalExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TraversalStats stats;

  while (true) {
    if (_traverser.hasMore()) {
      TRI_ASSERT(_inputRow.isInitialized());
      doOutput(output);
      if (output.isFull()) {
        if (_traverser.hasMore()) {
          return {ExecutorState::HASMORE, stats, AqlCall{}};
        } else {
          return {input.upstreamState(), stats, AqlCall{}};
        }
      }
    } else {
      if (!initTraverser(input)) {
        return {input.upstreamState(), stats, AqlCall{}};
      }
      TRI_ASSERT(_inputRow.isInitialized());
    }
  }

  stats.addFiltered(_traverser.getAndResetFilteredPaths());
  stats.addScannedIndex(_traverser.getAndResetReadDocuments());
  stats.addHttpRequests(_traverser.getAndResetHttpRequests());
  return {ExecutorState::DONE, stats, AqlCall{}};
}

auto TraversalExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, size_t, AqlCall> {
  auto skipped = size_t{0};

  while (true) {
    skipped += doSkip(call);

    if (!_traverser.hasMore()) {
      if (!initTraverser(input)) {
        return {input.upstreamState(), skipped, AqlCall{}};
      }
    } else {
      TRI_ASSERT(call.getOffset() == 0);
      return {ExecutorState::HASMORE, skipped, AqlCall{}};
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
    std::tie(std::ignore, _inputRow) = input.nextDataRow();
    TRI_ASSERT(_inputRow.isInitialized());

    if (opts->usesPrune()) {
      auto* evaluator = opts->getPruneEvaluator();
      // Replace by inputRow
      evaluator->prepareContext(_inputRow);
      TRI_ASSERT(_inputRow.isInitialized());
    }

    TRI_ASSERT(_inputRow.isInitialized());
    if (_infos.usesFixedSource()) {
      auto pos = _infos.getFixedSource().find('/');
      if (pos == std::string::npos) {
        _traverser.options()->query()->registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for traversal: "
            "Only id strings or objects with "
            "_id are allowed");
      } else {
        // Use constant value
        _traverser.setStartVertex(_infos.getFixedSource());
        TRI_ASSERT(_inputRow.isInitialized());
        return true;
      }
    } else {
      AqlValue const& in = _inputRow.getValue(_infos.getInputRegister());
      if (in.isObject()) {
        try {
          _traverser.setStartVertex(
              _traverser.options()->trx()->extractIdString(in.slice()));
          TRI_ASSERT(_inputRow.isInitialized());
          return true;
        } catch (...) {
          // on purpose ignore this error.
        }
      } else if (in.isString()) {
        _traverser.setStartVertex(in.slice().copyString());
        TRI_ASSERT(_inputRow.isInitialized());
        return true;
      } else {
        // _id or _key not present we cannot start here, register warning take next
        _traverser.options()->query()->registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for traversal: Only "
            "id strings or objects with _id are "
            "allowed");
      }
    }
  }
  return false;
}
