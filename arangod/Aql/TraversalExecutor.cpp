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
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserOptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::traverser;

TraversalExecutorInfos::TraversalExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear, std::unique_ptr<Traverser>&& traverser,
    std::unordered_map<OutputName, RegisterId> registerMapping,
    std::string const& fixedSource, RegisterId inputRegister,
    std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables)
    : ExecutorInfos(inputRegisters, outputRegisters, nrInputRegisters,
                    nrOutputRegisters, registersToClear),
      _traverser(std::move(traverser)),
      _registerMapping(registerMapping),
      _fixedSource(fixedSource),
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

bool TraversalExecutorInfos::useVertexOutput() const {
  return _registerMapping.find(OutputName::VERTEX) != _registerMapping.end();
}

RegisterId TraversalExecutorInfos::vertexRegister() const {
  TRI_ASSERT(useVertexOutput());
  return _registerMapping.find(OutputName::VERTEX)->second;
}

bool TraversalExecutorInfos::useEdgeOutput() const {
  return _registerMapping.find(OutputName::EDGE) != _registerMapping.end();
}

RegisterId TraversalExecutorInfos::edgeRegister() const {
  TRI_ASSERT(useEdgeOutput());
  return _registerMapping.find(OutputName::EDGE)->second;
}

bool TraversalExecutorInfos::usePathOutput() const {
  return _registerMapping.find(OutputName::PATH) != _registerMapping.end();
}

RegisterId TraversalExecutorInfos::pathRegister() const {
  TRI_ASSERT(usePathOutput());
  return _registerMapping.find(OutputName::PATH)->second;
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
TraversalExecutor::~TraversalExecutor() = default;

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
        return {_rowState, s};
      }
      std::tie(_rowState, _input) = _fetcher.fetchRow();
      if (_rowState == ExecutionState::WAITING) {
        TRI_ASSERT(!_input.isInitialized());
        s.addFiltered(_traverser.getAndResetFilteredPaths());
        s.addScannedIndex(_traverser.getAndResetReadDocuments());
        return {_rowState, s};
      }

      if (!_input.isInitialized()) {
        // We tried to fetch, but no upstream
        TRI_ASSERT(_rowState == ExecutionState::DONE);
        s.addFiltered(_traverser.getAndResetFilteredPaths());
        s.addScannedIndex(_traverser.getAndResetReadDocuments());
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
        output.cloneValueInto(_infos.vertexRegister(), _input,
                              _traverser.lastVertexToAqlValue());
      }
      if (_infos.useEdgeOutput()) {
        output.cloneValueInto(_infos.edgeRegister(), _input,
                              _traverser.lastEdgeToAqlValue());
      }
      if (_infos.usePathOutput()) {
        transaction::BuilderLeaser tmp(_traverser.trx());
        tmp->clear();
        output.cloneValueInto(_infos.pathRegister(), _input,
                              _traverser.pathToAqlValue(*tmp.builder()));
      }
      s.addFiltered(_traverser.getAndResetFilteredPaths());
      s.addScannedIndex(_traverser.getAndResetReadDocuments());
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