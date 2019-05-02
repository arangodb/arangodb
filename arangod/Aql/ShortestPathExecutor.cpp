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

#include "ShortestPathExecutor.h"
#include "Aql/AqlValue.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Graph/ShortestPathFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"

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
}  // namespace

ShortestPathExecutorInfos::ShortestPathExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unique_ptr<graph::ShortestPathFinder>&& finder,
    std::unordered_map<OutputName, RegisterId, OutputNameHash>&& registerMapping,
    InputVertex&& source, InputVertex&& target)
    : ExecutorInfos(std::move(inputRegisters), std::move(outputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _finder(std::move(finder)),
      _registerMapping(std::move(registerMapping)),
      _source(std::move(source)),
      _target(std::move(target)) {}

ShortestPathExecutorInfos::ShortestPathExecutorInfos(ShortestPathExecutorInfos&&) = default;
ShortestPathExecutorInfos::~ShortestPathExecutorInfos() = default;

arangodb::graph::ShortestPathFinder& ShortestPathExecutorInfos::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

bool ShortestPathExecutorInfos::useRegisterForInput(bool isTarget) const {
  if (isTarget) {
    return _target.type == InputVertex::REGISTER;
  }
  return _source.type == InputVertex::REGISTER;
}

RegisterId ShortestPathExecutorInfos::getInputRegister(bool isTarget) const {
  TRI_ASSERT(useRegisterForInput(isTarget));
  if (isTarget) {
    return _target.reg;
  }
  return _source.reg;
}

std::string const& ShortestPathExecutorInfos::getInputValue(bool isTarget) const {
  TRI_ASSERT(!useRegisterForInput(isTarget));
  if (isTarget) {
    return _target.value;
  }
  return _source.value;
}

bool ShortestPathExecutorInfos::usesOutputRegister(OutputName type) const {
  return _registerMapping.find(type) != _registerMapping.end();
}

static std::string typeToString(ShortestPathExecutorInfos::OutputName type) {
  switch(type) {
    case ShortestPathExecutorInfos::VERTEX:
      return std::string{"VERTEX"};
    case ShortestPathExecutorInfos::EDGE:
      return std::string{"EDGE"};
    default:
      return std::string{"<INVALID("} + std::to_string(type) + ")>";
  }
}

RegisterId ShortestPathExecutorInfos::findRegisterChecked(OutputName type) const {
  auto const& it = _registerMapping.find(type);
  if (ADB_UNLIKELY(it == _registerMapping.end())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "Logic error: requested unused register type " + typeToString(type));
  }
  return it->second;
}

RegisterId ShortestPathExecutorInfos::getOutputRegister(OutputName type) const {
  TRI_ASSERT(usesOutputRegister(type));
  return findRegisterChecked(type);
}

graph::TraverserCache* ShortestPathExecutorInfos::cache() const {
  return _finder->options().cache();
}

ShortestPathExecutor::ShortestPathExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _input{InvalidInputAqlItemRow},
      _rowState(ExecutionState::HASMORE),
      _finder{infos.finder()},
      _path{new arangodb::graph::ShortestPathResult{}},
      _posInPath(1),
      _sourceBuilder{},
      _targetBuilder{} {
  if (!_infos.useRegisterForInput(false)) {
    _sourceBuilder.add(VPackValue(_infos.getInputValue(false)));
  }
  if (!_infos.useRegisterForInput(true)) {
    _targetBuilder.add(VPackValue(_infos.getInputValue(true)));
  }
}

ShortestPathExecutor::~ShortestPathExecutor() = default;

// Shutdown query
std::pair<ExecutionState, Result> ShortestPathExecutor::shutdown(int errorCode) {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, NoStats> ShortestPathExecutor::produceRows(OutputAqlItemRow& output) {
  NoStats s;

  // Can be length 0 but never nullptr.
  TRI_ASSERT(_path);
  while (true) {
    if (_posInPath < _path->length()) {
      if (_infos.usesOutputRegister(ShortestPathExecutorInfos::VERTEX)) {
        AqlValue vertex = _path->vertexToAqlValue(_infos.cache(), _posInPath);
        AqlValueGuard guard{vertex, true};
        output.moveValueInto(_infos.getOutputRegister(ShortestPathExecutorInfos::VERTEX),
                             _input, guard);
      }
      if (_infos.usesOutputRegister(ShortestPathExecutorInfos::EDGE)) {
        AqlValue edge = _path->edgeToAqlValue(_infos.cache(), _posInPath);
        AqlValueGuard guard{edge, true};
        output.moveValueInto(_infos.getOutputRegister(ShortestPathExecutorInfos::EDGE),
                             _input, guard);
      }
      _posInPath++;
      return {computeState(), s};
    }
    TRI_ASSERT(_posInPath >= _path->length());
    if (!fetchPath()) {
      TRI_ASSERT(_posInPath >= _path->length());
      // Either WAITING or DONE
      return {_rowState, s};
    }
    TRI_ASSERT(_posInPath < _path->length());
  }
}

bool ShortestPathExecutor::fetchPath() {
  VPackSlice start;
  VPackSlice end;
  do {
    auto res = _fetcher.fetchRow();
    _rowState = res.first;
    _input = res.second.get();
    if (!_input.get().isInitialized()) {
      // Either WAITING or DONE and nothing produced.
      TRI_ASSERT(_rowState == ExecutionState::WAITING || _rowState == ExecutionState::DONE);
      return false;
    }
    if (!getVertexId(false, start) || !getVertexId(true, end)) {
      // Fetch another row
      continue;
    }
    TRI_ASSERT(start.isString());
    TRI_ASSERT(end.isString());
    _path->clear();
  } while (!_finder.shortestPath(start, end, *_path));
  _posInPath = 0;
  return true;
}

ExecutionState ShortestPathExecutor::computeState() const {
  if (_rowState == ExecutionState::HASMORE || _posInPath < _path->length()) {
    return ExecutionState::HASMORE;
  }
  return ExecutionState::DONE;
}

bool ShortestPathExecutor::getVertexId(bool isTarget, VPackSlice& id) {
  if (_infos.useRegisterForInput(isTarget)) {
    // The input row stays valid until the next fetchRow is executed.
    // So the slice can easily point to it.
    RegisterId reg = _infos.getInputRegister(isTarget);
    AqlValue const& in = _input.get().getValue(reg);
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
  } else {
    if (isTarget) {
      id = _targetBuilder.slice();
    } else {
      id = _sourceBuilder.slice();
    }
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
