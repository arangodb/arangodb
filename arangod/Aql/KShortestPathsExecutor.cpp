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

auto KShortestPathsExecutorInfos::useRegisterForSourceInput() const -> bool {
  return _source.type == InputVertex::Type::REGISTER;
}

auto KShortestPathsExecutorInfos::useRegisterForTargetInput() const -> bool {
  return _target.type == InputVertex::Type::REGISTER;
}

auto KShortestPathsExecutorInfos::getSourceInputRegister() const -> RegisterId {
  TRI_ASSERT(useRegisterForSourceInput());
  return _source.reg;
}

auto KShortestPathsExecutorInfos::getTargetInputRegister() const -> RegisterId {
  TRI_ASSERT(useRegisterForTargetInput());
  return _target.reg;
}

auto KShortestPathsExecutorInfos::getSourceInputValue() const -> std::string const& {
  TRI_ASSERT(!useRegisterForSourceInput());
  return _source.value;
}

auto KShortestPathsExecutorInfos::getTargetInputValue() const -> std::string const& {
  TRI_ASSERT(!useRegisterForTargetInput());
  return _target.value;
}

auto KShortestPathsExecutorInfos::getOutputRegister() const -> RegisterId {
  TRI_ASSERT(_outputRegister != RegisterPlan::MaxRegisterId);
  return _outputRegister;
}

auto KShortestPathsExecutorInfos::getSourceVertex() const noexcept
    -> KShortestPathsExecutorInfos::InputVertex {
  return _source;
}

auto KShortestPathsExecutorInfos::getTargetVertex() const noexcept
    -> KShortestPathsExecutorInfos::InputVertex {
  return _target;
}

auto KShortestPathsExecutorInfos::cache() const -> graph::TraverserCache* {
  return _finder->options().cache();
}

KShortestPathsExecutor::KShortestPathsExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _inputRow{CreateInvalidInputRowHint{}},
      _rowState(ExecutionState::HASMORE),
      _finder{infos.finder()},
      _sourceBuilder{},
      _targetBuilder{} {
  if (!_infos.useRegisterForSourceInput()) {
    _sourceBuilder.add(VPackValue(_infos.getSourceInputValue()));
  }
  if (!_infos.useRegisterForTargetInput()) {
    _targetBuilder.add(VPackValue(_infos.getTargetInputValue()));
  }
}

// Shutdown query
auto KShortestPathsExecutor::shutdown(int errorCode) -> std::pair<ExecutionState, Result> {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

auto KShortestPathsExecutor::produceRows(OutputAqlItemRow& output)
    -> std::pair<ExecutionState, NoStats> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto KShortestPathsExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  while (true) {
    if (_finder.isPathAvailable()) {
      doOutputPath(output);
      if (output.isFull()) {
        if (_finder.isPathAvailable()) {
          return {ExecutorState::HASMORE, NoStats{}, AqlCall{}};
        } else {
          return {input.upstreamState(), NoStats{}, AqlCall{}};
        }
      }
    } else {
      if (!fetchPaths(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), NoStats{}, AqlCall{}};
      }
    }
  }
}

auto KShortestPathsExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, size_t, AqlCall> {
  auto skipped = size_t{0};

  while (call.shouldSkip()) {
    if (_finder.isPathAvailable()) {
      skipped++;
      call.didSkip(1);
      _finder.skipPath();
    } else {
      if (!fetchPaths(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), skipped, AqlCall{}};
      }
    }
  }

  //
  if (_finder.isPathAvailable()) {
    return {ExecutorState::HASMORE, skipped, AqlCall{}};
  } else {
    return {input.upstreamState(), skipped, AqlCall{}};
  }
}

auto KShortestPathsExecutor::fetchPaths(AqlItemBlockInputRange& input) -> bool {
  TRI_ASSERT(!_finder.isPathAvailable());
  while (input.hasDataRow()) {
    auto source = VPackSlice{};
    auto target = VPackSlice{};
    std::tie(std::ignore, _inputRow) = input.nextDataRow();
    TRI_ASSERT(_inputRow.isInitialized());

    // Check start and end for validity
    if (getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder, source) &&
        getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder, target) &&
        _finder.startKShortestPathsTraversal(source, target)) {
      return true;
    }
  }
  return false;
}

auto KShortestPathsExecutor::doOutputPath(OutputAqlItemRow& output) -> void {
  while (!output.isFull() && _finder.isPathAvailable()) {
    auto tmp = transaction::BuilderLeaser{_finder.options().trx()};
    tmp->clear();
    _finder.getNextPathAql(*tmp.builder());
    output.cloneValueInto(_infos.getOutputRegister(), _inputRow, AqlValue(*tmp.builder()));
    output.advanceRow();
  }
}

auto KShortestPathsExecutor::getVertexId(KShortestPathsExecutorInfos::InputVertex const& vertex,
                                         InputAqlItemRow& row, VPackBuilder& builder,
                                         VPackSlice& id) -> bool {
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
