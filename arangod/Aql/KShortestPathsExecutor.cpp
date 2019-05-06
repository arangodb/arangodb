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
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Graph/KShortestPathsFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Transaction/Helpers.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
static bool isValidId(VPackSlice id) {
  TRI_ASSERT(id.isString());
  arangodb::velocypack::StringRef tester(id);
  return tester.find('/') != std::string::npos;
}
static RegisterId selectOutputRegister(
    std::shared_ptr<std::unordered_set<RegisterId> const> const& outputRegisters) {
  TRI_ASSERT(outputRegisters != nullptr);
  TRI_ASSERT(outputRegisters->size() == 1);
  RegisterId reg = *(outputRegisters->begin());
  TRI_ASSERT(reg != ExecutionNode::MaxRegisterId);
  return reg;
}
}  // namespace

KShortestPathsExecutorInfos::KShortestPathsExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unique_ptr<graph::KShortestPathsFinder>&& finder,
    InputVertex&& source, InputVertex&& target)
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
    return _target.type == InputVertex::REGISTER;
  }
  return _source.type == InputVertex::REGISTER;
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
  TRI_ASSERT(_outputRegister != ExecutionNode::MaxRegisterId);
  return _outputRegister;
}

graph::TraverserCache* KShortestPathsExecutorInfos::cache() const {
  return _finder->options().cache();
}

KShortestPathsExecutor::KShortestPathsExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _input{CreateInvalidInputRowHint{}},
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

KShortestPathsExecutor::~KShortestPathsExecutor() = default;

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
      output.moveValueInto(_infos.getOutputRegister(), _input, guard);
      return {computeState(), s};
    }
  }
}

bool KShortestPathsExecutor::fetchPaths() {
  VPackSlice start;
  VPackSlice end;
  do {
    // Fetch a row from upstream
    std::tie(_rowState, _input) = _fetcher.fetchRow();
    if (!_input.isInitialized()) {
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
  } while (!_finder.startKShortestPathsTraversal(start, end));
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
    AqlValue const& in = _input.getValue(reg);
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
