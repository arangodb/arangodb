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
    std::unique_ptr<graph::ShortestPathFinder>&& finder,
    std::unordered_map<OutputName, RegisterId, OutputNameHash>&& registerMapping,
    InputVertex&& source, InputVertex&& target)
    : _finder(std::move(finder)),
      _registerMapping(std::move(registerMapping)),
      _source(std::move(source)),
      _target(std::move(target)) {}

arangodb::graph::ShortestPathFinder& ShortestPathExecutorInfos::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

bool ShortestPathExecutorInfos::useRegisterForSourceInput() const {
  return _source.type == InputVertex::Type::REGISTER;
}

bool ShortestPathExecutorInfos::useRegisterForTargetInput() const {
  return _target.type == InputVertex::Type::REGISTER;
}

RegisterId ShortestPathExecutorInfos::getSourceInputRegister() const {
  TRI_ASSERT(useRegisterForSourceInput());
  return _source.reg;
}

RegisterId ShortestPathExecutorInfos::getTargetInputRegister() const {
  TRI_ASSERT(useRegisterForTargetInput());
  return _target.reg;
}

std::string const& ShortestPathExecutorInfos::getSourceInputValue() const {
  TRI_ASSERT(!useRegisterForSourceInput());
  return _source.value;
}

std::string const& ShortestPathExecutorInfos::getTargetInputValue() const {
  TRI_ASSERT(!useRegisterForTargetInput());
  return _target.value;
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
      _finder{infos.finder()},
      _path{new arangodb::graph::ShortestPathResult{}},
      _posInPath(0),
      _sourceBuilder{},
      _targetBuilder{} {
  if (!_infos.useRegisterForSourceInput()) {
    _sourceBuilder.add(VPackValue(_infos.getSourceInputValue()));
  }
  if (!_infos.useRegisterForTargetInput()) {
    _targetBuilder.add(VPackValue(_infos.getTargetInputValue()));
  }
  // Make sure the finder does not contain any leftovers in case of
  // the executor being reconstructed.
  _finder.clear();
}

// Shutdown query
std::pair<ExecutionState, Result> ShortestPathExecutor::shutdown(int errorCode) {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

auto ShortestPathExecutor::doOutputPath(OutputAqlItemRow& output) -> void {
  while (!output.isFull() && _posInPath < _path->length()) {
    if (_infos.usesOutputRegister(ShortestPathExecutorInfos::VERTEX)) {
      AqlValue vertex = _path->vertexToAqlValue(_infos.cache(), _posInPath);
      AqlValueGuard guard{vertex, true};
      output.moveValueInto(_infos.getOutputRegister(ShortestPathExecutorInfos::VERTEX),
                           _inputRow, guard);
    }
    if (_infos.usesOutputRegister(ShortestPathExecutorInfos::EDGE)) {
      AqlValue edge = _path->edgeToAqlValue(_infos.cache(), _posInPath);
      AqlValueGuard guard{edge, true};
      output.moveValueInto(_infos.getOutputRegister(ShortestPathExecutorInfos::EDGE),
                           _inputRow, guard);
    }
    output.advanceRow();
    _posInPath++;
  }
}

auto ShortestPathExecutor::doSkipPath(AqlCall& call) -> size_t {
  auto skip = size_t{0};

  // call.getOffset() > 0 means we're in SKIP mode
  if (call.getOffset() > 0) {
    if (call.getOffset() < pathLengthAvailable()) {
      skip = call.getOffset();
    } else {
      skip = pathLengthAvailable();
    }
  } else {
    // call.getOffset() == 0, we might be in SKIP, PRODUCE, or
    // FASTFORWARD/FULLCOUNT, but we only FASTFORWARD/FULLCOUNT if
    // call.getLimit() == 0 as well.
    if (call.needsFullCount() && call.getLimit() == 0) {
      skip = pathLengthAvailable();
    }
  }
  _posInPath += skip;
  call.didSkip(skip);
  return skip;
}

auto ShortestPathExecutor::fetchPath(AqlItemBlockInputRange& input) -> bool {
  // We only want to call fetchPath if we don't have a path currently available
  TRI_ASSERT(pathLengthAvailable() == 0);
  _finder.clear();
  _path->clear();
  _posInPath = 0;

  while (input.hasDataRow()) {
    auto source = VPackSlice{};
    auto target = VPackSlice{};
    std::tie(std::ignore, _inputRow) = input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(_inputRow.isInitialized());

    // Ordering important here.
    // Read source and target vertex, then try to find a shortest path (if both worked).
    if (getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder, source) &&
        getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder, target) &&
        _finder.shortestPath(source, target, *_path)) {
      return true;
    }
  }
  // Note that we only return false if
  // the input does not have a data row, so if we return false
  // here, we are DONE (we cannot produce any output anymore).
  return false;
}
auto ShortestPathExecutor::pathLengthAvailable() -> size_t {
  // Subtraction must not undeflow
  TRI_ASSERT(_posInPath <= _path->length());
  return _path->length() - _posInPath;
}

auto ShortestPathExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  while (true) {
    if (pathLengthAvailable() > 0) {
      // TODO maybe have doOutput report whether output is full?
      doOutputPath(output);
      if (output.isFull()) {
        if (pathLengthAvailable() > 0) {
          return {ExecutorState::HASMORE, NoStats{}, AqlCall{}};
        } else {
          // We don't have rows available for output. If
          // upstream is DONE, we will not be able to produce more
          // if upstream HASMORE, we do not know, so we say HASMORE.
          return {input.upstreamState(), NoStats{}, AqlCall{}};
        }
      }
    } else {
      // If fetchPath fails, this means that input has not given us a dataRow
      // that yielded a path.
      // If upstream is DONE, we are done too, and if upstream
      // HASMORE, we can potentially make more.
      if (!fetchPath(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), NoStats{}, AqlCall{}};
      }
    }
  }
}

auto ShortestPathExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto skipped = size_t{0};

  while (true) {
    skipped += doSkipPath(call);

    if (pathLengthAvailable() == 0) {
      if (!fetchPath(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), NoStats{}, skipped, AqlCall{}};
      }
    } else {
      // if we end up here there is path available, but
      // we have skipped as much as we were asked to.
      TRI_ASSERT(call.getOffset() == 0);
      return {ExecutorState::HASMORE, NoStats{}, skipped, AqlCall{}};
    }
  }
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
          _finder.options().query().warnings().registerWarning(
              TRI_ERROR_BAD_PARAMETER,
              "Invalid input for Shortest Path: "
              "Only id strings or objects with "
              "_id are allowed");
          return false;
        }
        return true;
      } else {
        _finder.options().query().warnings().registerWarning(
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
        _finder.options().query().warnings().registerWarning(
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
