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
#include "Basics/system-compiler.h"
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

arangodb::graph::ShortestPathFinder& ShortestPathExecutorInfos::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

bool ShortestPathExecutorInfos::useRegisterForInput(bool isTarget) const {
  if (isTarget) {
    return _target.type == InputVertex::Type::REGISTER;
  }
  return _source.type == InputVertex::Type::REGISTER;
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

ShortestPathExecutorInfos::InputVertex ShortestPathExecutorInfos::getSourceVertex() const noexcept {
  return _source;
}

ShortestPathExecutorInfos::InputVertex ShortestPathExecutorInfos::getTargetVertex() const noexcept {
  return _target;
}

static std::string typeToString(ShortestPathExecutorInfos::OutputName type) {
  switch (type) {
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

ShortestPathExecutor::ShortestPathExecutor(Fetcher&, Infos& infos)
    : _infos(infos),
      _inputRow{CreateInvalidInputRowHint{}},
      _myState{State::PATH_FETCH},
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

// Shutdown query
std::pair<ExecutionState, Result> ShortestPathExecutor::shutdown(int errorCode) {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, NoStats> ShortestPathExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// new executor interface
auto ShortestPathExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  auto stats = NoStats{};

  // We always request unlimited rows, because we cannot sensibly bound
  // our output size by the input size at all.
  auto upstreamCall = AqlCall{};

  while (true) {
    switch (_myState) {
      case State::PATH_FETCH: {
        // We don't want any old path muck lying around
        _path->clear();
        _posInPath = 0;

        if (input.hasDataRow()) {
          auto source = VPackSlice{};
          auto target = VPackSlice{};
          std::tie(std::ignore, _inputRow) = input.nextDataRow();
          TRI_ASSERT(_inputRow.isInitialized());

          // Ordering important here.
          // Read source and target vertex, then try to find a shortest path (if both worked).
          if (getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder, source) &&
              getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder, target) &&
              _finder.shortestPath(source, target, *_path)) {
            _myState = State::PATH_OUTPUT;
          }
        } else {
          // input.state() can be DONE in which case we are done too,
          // or HASMORE, in which case we wait for ExecutionBlockImpl
          // to produce us some inputs to process
          return {input.upstreamState(), stats, upstreamCall};
        }
      } break;
      case State::PATH_OUTPUT: {
        if (output.isFull()) {
          if (_posInPath < _path->length()) {
            // We have more locally
            return {ExecutorState::HASMORE, stats, upstreamCall};
          }
          // We are also done locally, report from upstream.
          return {input.upstreamState(), stats, upstreamCall};
        } else {
          if (_posInPath < _path->length()) {
            if (_infos.usesOutputRegister(ShortestPathExecutorInfos::VERTEX)) {
              AqlValue vertex = _path->vertexToAqlValue(_infos.cache(), _posInPath);
              output.cloneValueInto(_infos.getOutputRegister(ShortestPathExecutorInfos::VERTEX),
                                    _inputRow, vertex);
            }
            if (_infos.usesOutputRegister(ShortestPathExecutorInfos::EDGE)) {
              AqlValue edge = _path->edgeToAqlValue(_infos.cache(), _posInPath);
              output.cloneValueInto(_infos.getOutputRegister(ShortestPathExecutorInfos::EDGE),
                                    _inputRow, edge);
            }
            _posInPath++;
            output.advanceRow();
          } else {
            _myState = State::PATH_FETCH;
          }
        }
      } break;
      default:
        TRI_ASSERT(false);
    }
  }

  TRI_ASSERT(false);  // never should a while(true) be exited.
  return {ExecutorState::DONE, stats, upstreamCall};
}

bool ShortestPathExecutor::getVertexId(ShortestPathExecutorInfos::InputVertex const& vertex,
                                       InputAqlItemRow& row,
                                       VPackBuilder& builder, VPackSlice& id) {
  switch (vertex.type) {
    case ShortestPathExecutorInfos::InputVertex::Type::REGISTER: {
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
    case ShortestPathExecutorInfos::InputVertex::Type::CONSTANT: {
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
