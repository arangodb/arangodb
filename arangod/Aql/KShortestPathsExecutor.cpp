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
}  // namespace

KShortestPathsExecutorInfos::KShortestPathsExecutorInfos(
    RegisterId outputRegister, std::unique_ptr<graph::KShortestPathsFinder>&& finder,
    InputVertex&& source, InputVertex&& target)
    : _finder(std::move(finder)),
      _source(std::move(source)),
      _target(std::move(target)),
      _outputRegister(outputRegister) {}

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
  // Make sure the finder is in a defined state, because we could
  // get any old junk here, because infos are not recreated in between
  // initializeCursor calls.
  _finder.clear();
}

// Shutdown query
auto KShortestPathsExecutor::shutdown(int errorCode) -> std::pair<ExecutionState, Result> {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

auto KShortestPathsExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  while (!output.isFull()) {
    if (_finder.isDone()) {
      if (!fetchPaths(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), NoStats{}, AqlCall{}};
      }
    } else {
      doOutputPath(output);
    }
  }

  if (_finder.isDone()) {
    return {input.upstreamState(), NoStats{}, AqlCall{}};
  } else {
    return {ExecutorState::HASMORE, NoStats{}, AqlCall{}};
  }
}

auto KShortestPathsExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto stats = NoStats{};
  auto skipped = size_t{0};

  while (call.shouldSkip()) {
    // _finder.isDone() == true means that there is currently no path available
    // from the _finder, we can try calling fetchPaths to make one available,
    // but if that fails too, we must be DONE
    if (_finder.isDone()) {
      if (!fetchPaths(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), stats, skipped, AqlCall{}};
      }
    } else {
      if (_finder.skipPath()) {
        skipped++;
        call.didSkip(1);
      }
    }
  }

  if (_finder.isDone()) {
    return {input.upstreamState(), stats, skipped, AqlCall{}};
  } else {
    return {ExecutorState::HASMORE, stats, skipped, AqlCall{}};
  }
}

auto KShortestPathsExecutor::fetchPaths(AqlItemBlockInputRange& input) -> bool {
  TRI_ASSERT(_finder.isDone());
  _finder.clear();
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
  auto tmp = transaction::BuilderLeaser{_finder.options().trx()};
  tmp->clear();

  if (_finder.getNextPathAql(*tmp.builder())) {
    AqlValue path{*tmp.builder()};
    AqlValueGuard guard{path, true};
    output.moveValueInto(_infos.getOutputRegister(), _inputRow, guard);
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
    case KShortestPathsExecutorInfos::InputVertex::Type::CONSTANT: {
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
