////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "EnumeratePathsExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/algorithm-aliases.h"
#include "Transaction/Helpers.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>

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
  return id.stringView().find('/') != std::string_view::npos;
}
}  // namespace

template<class FinderType>
EnumeratePathsExecutorInfos<FinderType>::EnumeratePathsExecutorInfos(
    RegisterId outputRegister, QueryContext& query,
    std::unique_ptr<FinderType>&& finder, InputVertex&& source,
    InputVertex&& target)
    : _query(query),
      _finder(std::move(finder)),
      _source(std::move(source)),
      _target(std::move(target)),
      _outputRegister(outputRegister) {}

template<class FinderType>
FinderType& EnumeratePathsExecutorInfos<FinderType>::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

template<class FinderType>
QueryContext& EnumeratePathsExecutorInfos<FinderType>::query() noexcept {
  return _query;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::useRegisterForSourceInput() const
    -> bool {
  return _source.type == InputVertex::Type::REGISTER;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::useRegisterForTargetInput() const
    -> bool {
  return _target.type == InputVertex::Type::REGISTER;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::getSourceInputRegister() const
    -> RegisterId {
  TRI_ASSERT(useRegisterForSourceInput());
  return _source.reg;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::getTargetInputRegister() const
    -> RegisterId {
  TRI_ASSERT(useRegisterForTargetInput());
  return _target.reg;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::getSourceInputValue() const
    -> std::string const& {
  TRI_ASSERT(!useRegisterForSourceInput());
  return _source.value;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::getTargetInputValue() const
    -> std::string const& {
  TRI_ASSERT(!useRegisterForTargetInput());
  return _target.value;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::getOutputRegister() const
    -> RegisterId {
  TRI_ASSERT(_outputRegister.isValid());
  return _outputRegister;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::getSourceVertex() const noexcept
    -> EnumeratePathsExecutorInfos<FinderType>::InputVertex {
  return _source;
}

template<class FinderType>
auto EnumeratePathsExecutorInfos<FinderType>::getTargetVertex() const noexcept
    -> EnumeratePathsExecutorInfos<FinderType>::InputVertex {
  return _target;
}

template<class FinderType>
EnumeratePathsExecutor<FinderType>::EnumeratePathsExecutor(Fetcher& fetcher,
                                                           Infos& infos)
    : _infos(infos),
      _trx(infos.query().newTrxContext()),
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
auto EnumeratePathsExecutor<FinderType>::shutdown(int errorCode)
    -> std::pair<ExecutionState, Result> {
  _finder.destroyEngines();
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

template<class FinderType>
auto EnumeratePathsExecutor<FinderType>::produceRows(
    AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  while (!output.isFull()) {
    if (_finder.isDone()) {
      if (!fetchPaths(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), stats(), AqlCall{}};
      }
    } else {
      doOutputPath(output);
    }
  }

  if (_finder.isDone()) {
    return {input.upstreamState(), stats(), AqlCall{}};
  } else {
    return {ExecutorState::HASMORE, stats(), AqlCall{}};
  }
}

template<class FinderType>
auto EnumeratePathsExecutor<FinderType>::skipRowsRange(
    AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto skipped = size_t{0};

  while (call.needSkipMore()) {
    // _finder.isDone() == true means that there is currently no path available
    // from the _finder, we can try calling fetchPaths to make one available,
    // but if that fails too, we must be DONE
    if (_finder.isDone()) {
      if (!fetchPaths(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), stats(), skipped, AqlCall{}};
      }
    } else {
      if (_finder.skipPath()) {
        skipped++;
        call.didSkip(1);
      }
    }
  }

  if (_finder.isDone()) {
    return {input.upstreamState(), stats(), skipped, AqlCall{}};
  } else {
    return {ExecutorState::HASMORE, stats(), skipped, AqlCall{}};
  }
}

template<class FinderType>
auto EnumeratePathsExecutor<FinderType>::fetchPaths(
    AqlItemBlockInputRange& input) -> bool {
  TRI_ASSERT(_finder.isDone());
  _finder.clear();
  while (input.hasDataRow()) {
    auto source = VPackSlice{};
    auto target = VPackSlice{};
    std::tie(std::ignore, _inputRow) =
        input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(_inputRow.isInitialized());

    // Check start and end for validity
    if (getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder,
                    source) &&
        getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder,
                    target)) {
      _finder.reset(arangodb::velocypack::HashedStringRef(source),
                    arangodb::velocypack::HashedStringRef(target));
      return true;
    }
  }
  return false;
}

template<class FinderType>
auto EnumeratePathsExecutor<FinderType>::doOutputPath(OutputAqlItemRow& output)
    -> void {
  transaction::BuilderLeaser tmp{&_trx};
  tmp->clear();

  if (_finder.getNextPath(*tmp.builder())) {
    AqlValue path{tmp->slice()};
    AqlValueGuard guard{path, true};
    output.moveValueInto(_infos.getOutputRegister(), _inputRow, &guard);
    output.advanceRow();
  }
}

template<class FinderType>
auto EnumeratePathsExecutor<FinderType>::getVertexId(InputVertex const& vertex,
                                                     InputAqlItemRow& row,
                                                     VPackBuilder& builder,
                                                     VPackSlice& id) -> bool {
  switch (vertex.type) {
    case InputVertex::Type::REGISTER: {
      AqlValue const& in = row.getValue(vertex.reg);
      if (in.isObject()) {
        try {
          // TODO:  calculate expression once e.g. header constexpr bool and
          // check then here
          std::string idString = _trx.extractIdString(in.slice());

          builder.clear();
          builder.add(VPackValue(idString));
          id = builder.slice();
          // Guaranteed by extractIdValue
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
          _infos.query().warnings().registerWarning(
              TRI_ERROR_BAD_PARAMETER,
              "Invalid input for Shortest Path: "
              "Only id strings or objects with "
              "_id are allowed");
          return false;
        }
        return true;
      } else {
        _infos.query().warnings().registerWarning(
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
        _infos.query().warnings().registerWarning(
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

template<class FinderType>
[[nodiscard]] auto EnumeratePathsExecutor<FinderType>::stats() -> Stats {
  return _finder.stealStats();
}

/* SingleServerProvider Section */

using SingleServer = SingleServerProvider<SingleServerProviderStep>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    AllShortestPathsEnumerator<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedAllShortestPathsEnumerator<SingleServer>>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    WeightedKShortestPathsEnumerator<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedWeightedKShortestPathsEnumerator<SingleServer>>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    YenEnumeratorWithProvider<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedYenEnumeratorWithProvider<SingleServer>>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    WeightedYenEnumeratorWithProvider<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedWeightedYenEnumeratorWithProvider<SingleServer>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    AllShortestPathsEnumerator<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedAllShortestPathsEnumerator<SingleServer>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    WeightedKShortestPathsEnumerator<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedWeightedKShortestPathsEnumerator<SingleServer>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    YenEnumeratorWithProvider<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedYenEnumeratorWithProvider<SingleServer>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    WeightedYenEnumeratorWithProvider<SingleServer>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedWeightedYenEnumeratorWithProvider<SingleServer>>;

/* ClusterProvider Section */

using ClustProvider = ClusterProvider<ClusterProviderStep>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    AllShortestPathsEnumerator<ClusterProvider<ClusterProviderStep>>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedAllShortestPathsEnumerator<ClusterProvider<ClusterProviderStep>>>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    WeightedKShortestPathsEnumerator<ClusterProvider<ClusterProviderStep>>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedWeightedKShortestPathsEnumerator<
        ClusterProvider<ClusterProviderStep>>>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    YenEnumeratorWithProvider<ClustProvider>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedYenEnumeratorWithProvider<ClustProvider>>;

template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    WeightedYenEnumeratorWithProvider<ClustProvider>>;
template class ::arangodb::aql::EnumeratePathsExecutorInfos<
    TracedWeightedYenEnumeratorWithProvider<ClustProvider>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    AllShortestPathsEnumerator<ClusterProvider<ClusterProviderStep>>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedAllShortestPathsEnumerator<ClusterProvider<ClusterProviderStep>>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    WeightedKShortestPathsEnumerator<ClusterProvider<ClusterProviderStep>>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedWeightedKShortestPathsEnumerator<
        ClusterProvider<ClusterProviderStep>>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    YenEnumeratorWithProvider<ClustProvider>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedYenEnumeratorWithProvider<ClustProvider>>;

template class ::arangodb::aql::EnumeratePathsExecutor<
    WeightedYenEnumeratorWithProvider<ClustProvider>>;
template class ::arangodb::aql::EnumeratePathsExecutor<
    TracedWeightedYenEnumeratorWithProvider<ClustProvider>>;
