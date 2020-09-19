////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Graph/KPathFinder.h"
#include "Graph/KShortestPathsFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Transaction/Helpers.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
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

template<class FinderType>
KShortestPathsExecutorInfos<FinderType>::KShortestPathsExecutorInfos(
    RegisterId outputRegister, std::unique_ptr<FinderType>&& finder,
    InputVertex&& source, InputVertex&& target)
    : _finder(std::move(finder)),
      _source(std::move(source)),
      _target(std::move(target)),
      _outputRegister(outputRegister) {}

template<class FinderType>
FinderType& KShortestPathsExecutorInfos<FinderType>::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::useRegisterForSourceInput() const -> bool {
  return _source.type == InputVertex::Type::REGISTER;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::useRegisterForTargetInput() const -> bool {
  return _target.type == InputVertex::Type::REGISTER;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::getSourceInputRegister() const -> RegisterId {
  TRI_ASSERT(useRegisterForSourceInput());
  return _source.reg;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::getTargetInputRegister() const -> RegisterId {
  TRI_ASSERT(useRegisterForTargetInput());
  return _target.reg;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::getSourceInputValue() const -> std::string const& {
  TRI_ASSERT(!useRegisterForSourceInput());
  return _source.value;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::getTargetInputValue() const -> std::string const& {
  TRI_ASSERT(!useRegisterForTargetInput());
  return _target.value;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::getOutputRegister() const -> RegisterId {
  TRI_ASSERT(_outputRegister != RegisterPlan::MaxRegisterId);
  return _outputRegister;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::getSourceVertex() const noexcept
    -> KShortestPathsExecutorInfos<FinderType>::InputVertex {
  return _source;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::getTargetVertex() const noexcept
    -> KShortestPathsExecutorInfos<FinderType>::InputVertex {
  return _target;
}

template<class FinderType>
auto KShortestPathsExecutorInfos<FinderType>::cache() const -> graph::TraverserCache* {
  return _finder->options().cache();
}

template<class FinderType>
KShortestPathsExecutor<FinderType>::KShortestPathsExecutor(Fetcher& fetcher, Infos& infos)
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
template<class FinderType>
auto KShortestPathsExecutor<FinderType>::shutdown(int errorCode) -> std::pair<ExecutionState, Result> {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

template<class FinderType>
auto KShortestPathsExecutor<FinderType>::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
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

template<class FinderType>
auto KShortestPathsExecutor<FinderType>::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
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

template<class FinderType>
auto KShortestPathsExecutor<FinderType>::fetchPaths(AqlItemBlockInputRange& input) -> bool {
  TRI_ASSERT(_finder.isDone());
  _finder.clear();
  while (input.hasDataRow()) {
    auto source = VPackSlice{};
    auto target = VPackSlice{};
    std::tie(std::ignore, _inputRow) = input.nextDataRow();
    TRI_ASSERT(_inputRow.isInitialized());

    // Check start and end for validity
    if (getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder, source) &&
        getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder, target)) {
      if constexpr (std::is_same_v<FinderType, KShortestPathsFinder>) {
        return _finder.startKShortestPathsTraversal(source, target);
      } else {
        _finder.reset(arangodb::velocypack::HashedStringRef(source), arangodb::velocypack::HashedStringRef(target));
        return true;
      }
    }
  }
  return false;
}

template<class FinderType>
auto KShortestPathsExecutor<FinderType>::doOutputPath(OutputAqlItemRow& output) -> void {
  transaction::BuilderLeaser tmp{_finder.options().trx()};
  tmp->clear();

  if constexpr (std::is_same_v<FinderType, KShortestPathsFinder>) {
    if (_finder.getNextPathAql(*tmp.builder())) {
      AqlValue path{tmp->slice()};
      AqlValueGuard guard{path, true};
      output.moveValueInto(_infos.getOutputRegister(), _inputRow, guard);
      output.advanceRow();
    }
  } else {
    if (_finder.getNextPath(*tmp.builder())) {
      AqlValue path{tmp->slice()};
      AqlValueGuard guard{path, true};
      output.moveValueInto(_infos.getOutputRegister(), _inputRow, guard);
      output.advanceRow();
    }
  }

}

template <class FinderType>
auto KShortestPathsExecutor<FinderType>::getVertexId(InputVertex const& vertex,
                                                     InputAqlItemRow& row,
                                                     VPackBuilder& builder,
                                                     VPackSlice& id) -> bool {
  switch (vertex.type) {
    case InputVertex::Type::REGISTER: {
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
    case InputVertex::Type::CONSTANT: {
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

template class ::arangodb::aql::KShortestPathsExecutorInfos<arangodb::graph::KShortestPathsFinder>;
template class ::arangodb::aql::KShortestPathsExecutorInfos<arangodb::graph::KPathFinder>;
template class ::arangodb::aql::KShortestPathsExecutor<arangodb::graph::KShortestPathsFinder>;
template class ::arangodb::aql::KShortestPathsExecutor<arangodb::graph::KPathFinder>;
