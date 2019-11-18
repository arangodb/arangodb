////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "KShortestPathsExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Graph/KShortestPathsFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Transaction/Helpers.h"

#include <velocypack/Builder.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
static bool isValidId(VPackSlice id) {
  if (!id.isString()) {
    return false;
  }
  TRI_ASSERT(id.isString());
  arangodb::velocypack::StringRef tester(id);
  return tester.find('/') != std::string::npos;
}
static RegisterId selectOutputRegister(
    std::shared_ptr<std::unordered_set<RegisterId> const> const& outputRegisters) {
  TRI_ASSERT(outputRegisters != nullptr);
  TRI_ASSERT(outputRegisters->size() == 1);
  RegisterId reg = *(outputRegisters->begin());
  TRI_ASSERT(reg != RegisterPlan::MaxRegisterId);
  return reg;
}
}  // namespace

KShortestPathsExecutorInfos::KShortestPathsExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unique_ptr<graph::KShortestPathsFinder>&& finder, InputVertex&& source,
    InputVertex&& target)
    : ExecutorInfos(std::move(inputRegisters), std::move(outputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _finder(std::move(finder)),
      _source(std::move(source)),
      _target(std::move(target)),
      _outputRegister(::selectOutputRegister(_outRegs)) {}

KShortestPathsExecutorInfos::KShortestPathsExecutorInfos(KShortestPathsExecutorInfos&&) = default;
KShortestPathsExecutorInfos::~KShortestPathsExecutorInfos() = default;

arangodb::graph::KShortestPathsFinder& KShortestPathsExecutorInfos::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

bool KShortestPathsExecutorInfos::useRegisterForInput(bool isTarget) const {
  if (isTarget) {
    return _target.type == InputVertex::Type::REGISTER;
  }
  return _source.type == InputVertex::Type::REGISTER;
}

RegisterId KShortestPathsExecutorInfos::getInputRegister(bool isTarget) const {
  TRI_ASSERT(useRegisterForInput(isTarget));
  if (isTarget) {
    return _target.reg;
  }
  return _source.reg;
}

std::string const& KShortestPathsExecutorInfos::getInputValue(bool isTarget) const {
  TRI_ASSERT(!useRegisterForInput(isTarget));
  if (isTarget) {
    return _target.value;
  }
  return _source.value;
}

RegisterId KShortestPathsExecutorInfos::getOutputRegister() const {
  TRI_ASSERT(_outputRegister != RegisterPlan::MaxRegisterId);
  return _outputRegister;
}

KShortestPathsExecutorInfos::InputVertex KShortestPathsExecutorInfos::getSourceVertex() const
    noexcept {
  return _source;
}

KShortestPathsExecutorInfos::InputVertex KShortestPathsExecutorInfos::getTargetVertex() const
    noexcept {
  return _target;
}

graph::TraverserCache* KShortestPathsExecutorInfos::cache() const {
  return _finder->options().cache();
}

KShortestPathsExecutor::KShortestPathsExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _inputRow{CreateInvalidInputRowHint{}},
      _rowState(ExecutionState::HASMORE),
      _finder{infos.finder()},
      _sourceBuilder{},
      _targetBuilder{} {
  if (!_infos.useRegisterForInput(false)) {
    _sourceBuilder.add(VPackValue(_infos.getInputValue(false)));
  }
  if (!_infos.useRegisterForInput(true)) {
    _targetBuilder.add(VPackValue(_infos.getInputValue(true)));
  }
}

// Shutdown query
std::pair<ExecutionState, Result> KShortestPathsExecutor::shutdown(int errorCode) {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, NoStats> KShortestPathsExecutor::produceRows(OutputAqlItemRow& output) {
  NoStats s;

  while (true) {
    // We will have paths available, or return
    if (!_finder.isPathAvailable()) {
      if (!fetchPaths()) {
        return {_rowState, s};
      }
    }

    // Now we have a path available, so we go and output it
    transaction::BuilderLeaser tmp(_finder.options().trx());
    tmp->clear();
    if (_finder.getNextPathAql(*tmp.builder())) {
      AqlValue path = AqlValue(*tmp.builder());
      AqlValueGuard guard{path, true};
      output.moveValueInto(_infos.getOutputRegister(), _inputRow, guard);
      return {computeState(), s};
    }
  }
}

bool KShortestPathsExecutor::fetchPaths() {
  VPackSlice start;
  VPackSlice end;
  while (true) {
    // Fetch a row from upstream
    std::tie(_rowState, _inputRow) = _fetcher.fetchRow();
    if (!_inputRow.isInitialized()) {
      // Either WAITING or DONE, in either case we cannot produce any paths.
      TRI_ASSERT(_rowState == ExecutionState::WAITING || _rowState == ExecutionState::DONE);
      return false;
    }
    // Check start and end for validity
    if (!getVertexId(false, start) || !getVertexId(true, end)) {
      // Fetch another row
      continue;
    }
    TRI_ASSERT(start.isString());
    TRI_ASSERT(end.isString());
    if (_finder.startKShortestPathsTraversal(start, end)) {
      break;
    }
  }
  return true;
}

ExecutionState KShortestPathsExecutor::computeState() const {
  if (_rowState == ExecutionState::HASMORE || _finder.isPathAvailable()) {
    return ExecutionState::HASMORE;
  }
  return ExecutionState::DONE;
}

bool KShortestPathsExecutor::getVertexId(bool isTarget, VPackSlice& id) {
  if (_infos.useRegisterForInput(isTarget)) {
    // The input row stays valid until the next fetchRow is executed.
    // So the slice can easily point to it.
    RegisterId reg = _infos.getInputRegister(isTarget);
    AqlValue const& in = _inputRow.getValue(reg);
    if (in.isObject()) {
      try {
        auto idString = _finder.options().trx()->extractIdString(in.slice());
        if (isTarget) {
          _targetBuilder.clear();
          _targetBuilder.add(VPackValue(idString));
          id = _targetBuilder.slice();
        } else {
          _sourceBuilder.clear();
          _sourceBuilder.add(VPackValue(idString));
          id = _sourceBuilder.slice();
        }
        // Guranteed by extractIdValue
        TRI_ASSERT(::isValidId(id));
      } catch (...) {
        // _id or _key not present... ignore this error and fall through
        // returning no path
        return false;
      }
      return true;
    } else if (in.isString()) {
      id = in.slice();
      // Validation
      if (!::isValidId(id)) {
        _finder.options().query()->registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for k Shortest Paths: "
            "Only id strings or objects with "
            "_id are allowed");
        return false;
      }
      return true;
    } else {
      _finder.options().query()->registerWarning(
          TRI_ERROR_BAD_PARAMETER,
          "Invalid input for k Shortest Paths: "
          "Only id strings or objects with "
          "_id are allowed");
      return false;
    }
  } else {
    if (isTarget) {
      id = _targetBuilder.slice();
    } else {
      id = _sourceBuilder.slice();
    }
    if (!::isValidId(id)) {
      _finder.options().query()->registerWarning(
          TRI_ERROR_BAD_PARAMETER,
          "Invalid input for k Shortest Paths: "
          "Only id strings or objects with "
          "_id are allowed");
      return false;
    }
    return true;
  }
}

std::tuple<ExecutorState, NoStats, AqlCall> KShortestPathsExecutor::produceRows(
    size_t atMost, AqlItemBlockInputRange& input, OutputAqlItemRow& output) {
  auto stats = NoStats{};
  auto upstreamCall = AqlCall{};
  auto nrOutput = size_t{0};

  while (nrOutput < atMost) {
    // We will have paths available, or return
    if (!_finder.isPathAvailable()) {
      if (!fetchPaths(input)) {
        return {ExecutorState::DONE, stats, upstreamCall};
      }
    }

    // Now we have a path available, so we go and output it
    transaction::BuilderLeaser tmp(_finder.options().trx());
    tmp->clear();
    if (_finder.getNextPathAql(*tmp.builder())) {
      AqlValue path = AqlValue(*tmp.builder());
      output.cloneValueInto(_infos.getOutputRegister(), _inputRow, path);
      output.advanceRow();
      nrOutput++;
    }
  }

  // if we reached here, we output atMost, so we might have more
  // paths available
  return {ExecutorState::HASMORE, stats, upstreamCall};
}

bool KShortestPathsExecutor::fetchPaths(AqlItemBlockInputRange& input) {
  VPackSlice source;
  VPackSlice target;
  ExecutorState state;

  while (input.hasMore()) {
    std::tie(state, _inputRow) = input.next();
    TRI_ASSERT(_inputRow.isInitialized());

    // Check start and end for validity
    if (!getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder, source) ||
        !getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder, target)) {
      // Fetch another row
      continue;
    }
    TRI_ASSERT(source.isString());
    TRI_ASSERT(target.isString());
    if (_finder.startKShortestPathsTraversal(source, target)) {
      return true;
    }
  }
  return false;
}

bool KShortestPathsExecutor::getVertexId(KShortestPathsExecutorInfos::InputVertex const& vertex,
                                         InputAqlItemRow& row,
                                         VPackBuilder& builder, VPackSlice& id) {
  switch (vertex.type) {
    case KShortestPathsExecutorInfos::InputVertex::Type::REGISTER: {
      AqlValue const& in = row.getValue(vertex.reg);
      if (in.isObject()) {
        try {
          auto idString = _finder.options().trx()->extractIdString(in.slice());
          builder.clear();
          builder.add(VPackValue(idString));
          id = builder.slice();
          // Guranteed by extractIdValue
          TRI_ASSERT(::isValidId(id));
        } catch (...) {
          // _id or _key not present... ignore this error and fall through
          // returning no path
          return false;
        }
        return true;
      } else if (in.isString()) {
        id = in.slice();
        // Validation
        if (!::isValidId(id)) {
          _finder.options().query()->registerWarning(
              TRI_ERROR_BAD_PARAMETER,
              "Invalid input for Shortest Path: "
              "Only id strings or objects with "
              "_id are allowed");
          return false;
        }
        return true;
      } else {
        _finder.options().query()->registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for Shortest Path: "
            "Only id strings or objects with "
            "_id are allowed");
        return false;
      }
    }
    case KShortestPathsExecutorInfos::InputVertex::Type::CONSTANT: {
      id = builder.slice();
      if (!::isValidId(id)) {
        _finder.options().query()->registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for Shortest Path: "
            "Only id strings or objects with "
            "_id are allowed");
        return false;
      }
      return true;
    }
  }
  return false;
}
