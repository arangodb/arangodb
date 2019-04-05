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
#include "Aql/OutputAqlItemRow.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Graph/Traverser.h"
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
  TRI_ASSERT((_fixedSource.empty() && _inputRegister != ExecutionNode::MaxRegisterId) ||
             (!_fixedSource.empty() && _inputRegister == ExecutionNode::MaxRegisterId));
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
  switch(type) {
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
  return !_fixedSource.empty();
}

std::string const& TraversalExecutorInfos::getFixedSource() const {
  TRI_ASSERT(usesFixedSource());
  return _fixedSource;
}

RegisterId TraversalExecutorInfos::getInputRegister() const {
  TRI_ASSERT(!usesFixedSource());
  TRI_ASSERT(_inputRegister != ExecutionNode::MaxRegisterId);
  return _inputRegister;
}

std::vector<std::pair<Variable const*, RegisterId>> const& TraversalExecutorInfos::filterConditionVariables() const {
  return _filterConditionVariables;
}

TraversalExecutor::TraversalExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _input{CreateInvalidInputRowHint{}},
      _rowState(ExecutionState::HASMORE),
      _traverser(infos.traverser()) {}

TraversalExecutor::~TraversalExecutor() {
  auto opts = _traverser.options();
  if (opts != nullptr && opts->usesPrune()) {
    auto *evaluator = opts->getPruneEvaluator();
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

std::pair<ExecutionState, TraversalStats> TraversalExecutor::produceRow(OutputAqlItemRow& output) {
  TraversalStats s;

  while (true) {
    if (!_input.isInitialized()) {
      if (_rowState == ExecutionState::DONE) {
        // we are done
        s.addFiltered(_traverser.getAndResetFilteredPaths());
        s.addScannedIndex(_traverser.getAndResetReadDocuments());
        s.addHttpRequests(_traverser.getAndResetHttpRequests());
        return {_rowState, s};
      }
      std::tie(_rowState, _input) = _fetcher.fetchRow();
      if (_rowState == ExecutionState::WAITING) {
        TRI_ASSERT(!_input.isInitialized());
        s.addFiltered(_traverser.getAndResetFilteredPaths());
        s.addScannedIndex(_traverser.getAndResetReadDocuments());
        s.addHttpRequests(_traverser.getAndResetHttpRequests());
        return {_rowState, s};
      }

      if (!_input.isInitialized()) {
        // We tried to fetch, but no upstream
        TRI_ASSERT(_rowState == ExecutionState::DONE);
        s.addFiltered(_traverser.getAndResetFilteredPaths());
        s.addScannedIndex(_traverser.getAndResetReadDocuments());
        s.addHttpRequests(_traverser.getAndResetHttpRequests());
        return {_rowState, s};
      }
      if (!resetTraverser()) {
        // Could not start here, (invalid)
        // Go to next
        _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
        continue;
      }
    }
    if (!_traverser.hasMore() || !_traverser.next()) {
      // Nothing more to read, reset input to refetch
      _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
    } else {
      // traverser now has next v, e, p values
      if (_infos.useVertexOutput()) {
        AqlValue vertex = _traverser.lastVertexToAqlValue();
        AqlValueGuard guard{vertex, true};
        output.moveValueInto(_infos.vertexRegister(), _input, guard);
      }
      if (_infos.useEdgeOutput()) {
        AqlValue edge = _traverser.lastEdgeToAqlValue();
        AqlValueGuard guard{edge, true};
        output.moveValueInto(_infos.edgeRegister(), _input, guard);
      }
      if (_infos.usePathOutput()) {
        transaction::BuilderLeaser tmp(_traverser.trx());
        tmp->clear();
        AqlValue path = _traverser.pathToAqlValue(*tmp.builder());
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.pathRegister(), _input, guard);
      }
      s.addFiltered(_traverser.getAndResetFilteredPaths());
      s.addScannedIndex(_traverser.getAndResetReadDocuments());
      s.addHttpRequests(_traverser.getAndResetHttpRequests());
      return {computeState(), s};
    }
  }

  s.addFiltered(_traverser.getAndResetFilteredPaths());
  s.addScannedIndex(_traverser.getAndResetReadDocuments());
  return {ExecutionState::DONE, s};
}

ExecutionState TraversalExecutor::computeState() const {
  if (_rowState == ExecutionState::DONE && !_traverser.hasMore()) {
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

bool TraversalExecutor::resetTraverser() {
  // Initialize the Expressions within the options.
  // We need to find the variable and read its value here. Everything is
  // computed right now.
  auto opts = _traverser.options();
  opts->clearVariableValues();
  for (auto const& pair : _infos.filterConditionVariables()) {
    opts->setVariableValue(pair.first, _input.getValue(pair.second));
  }
  if (opts->usesPrune()) {
    auto* evaluator = opts->getPruneEvaluator();
    // Replace by inputRow
    evaluator->prepareContext(_input);
  }
  // Now reset the traverser
  if (_infos.usesFixedSource()) {
    auto pos = _infos.getFixedSource().find('/');
    if (pos == std::string::npos) {
      _traverser.options()->query()->registerWarning(
          TRI_ERROR_BAD_PARAMETER,
          "Invalid input for traversal: "
          "Only id strings or objects with "
          "_id are allowed");
      return false;
    } else {
      // Use constant value
      _traverser.setStartVertex(_infos.getFixedSource());
      return true;
    }
  } else {
    AqlValue const& in = _input.getValue(_infos.getInputRegister());
    if (in.isObject()) {
      try {
        _traverser.setStartVertex(_traverser.options()->trx()->extractIdString(in.slice()));
        return true;
      } catch (...) {
        // on purpose ignore this error.
        return false;
      }
      // _id or _key not present we cannot start here, register warning take next
    } else if (in.isString()) {
      _traverser.setStartVertex(in.slice().copyString());
      return true;
    } else {
      _traverser.options()->query()->registerWarning(
          TRI_ERROR_BAD_PARAMETER,
          "Invalid input for traversal: Only "
          "id strings or objects with _id are "
          "allowed");
      return false;
    }
  }
}
