////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Graph/algorithm-aliases.h"
#include "Transaction/Helpers.h"

#include <velocypack/Builder.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
static bool isValidId(VPackSlice id) {
  TRI_ASSERT(id.isString());
  return id.stringView().find('/') != std::string_view::npos;
}
}  // namespace

template<class FinderType>
ShortestPathExecutorInfos<FinderType>::ShortestPathExecutorInfos(
    QueryContext& query, std::unique_ptr<FinderType>&& finder,
    std::unordered_map<OutputName, RegisterId, OutputNameHash>&&
        registerMapping,
    GraphNode::InputVertex&& source, GraphNode::InputVertex&& target)
    : _query(query),
      _finder(std::move(finder)),
      _registerMapping(std::move(registerMapping)),
      _source(std::move(source)),
      _target(std::move(target)) {}

template<class FinderType>
FinderType& ShortestPathExecutorInfos<FinderType>::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

template<class FinderType>
QueryContext& ShortestPathExecutorInfos<FinderType>::query() noexcept {
  return _query;
}

template<class FinderType>
bool ShortestPathExecutorInfos<FinderType>::useRegisterForSourceInput() const {
  return _source.type == GraphNode::InputVertex::Type::REGISTER;
}

template<class FinderType>
bool ShortestPathExecutorInfos<FinderType>::useRegisterForTargetInput() const {
  return _target.type == GraphNode::InputVertex::Type::REGISTER;
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::getSourceInputRegister()
    const {
  TRI_ASSERT(useRegisterForSourceInput());
  return _source.reg;
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::getTargetInputRegister()
    const {
  TRI_ASSERT(useRegisterForTargetInput());
  return _target.reg;
}

template<class FinderType>
std::string const& ShortestPathExecutorInfos<FinderType>::getSourceInputValue()
    const {
  TRI_ASSERT(!useRegisterForSourceInput());
  return _source.value;
}

template<class FinderType>
std::string const& ShortestPathExecutorInfos<FinderType>::getTargetInputValue()
    const {
  TRI_ASSERT(!useRegisterForTargetInput());
  return _target.value;
}

template<class FinderType>
bool ShortestPathExecutorInfos<FinderType>::usesOutputRegister(
    OutputName type) const {
  return _registerMapping.find(type) != _registerMapping.end();
}

template<class FinderType>
GraphNode::InputVertex ShortestPathExecutorInfos<FinderType>::getSourceVertex()
    const noexcept {
  return _source;
}

template<class FinderType>
GraphNode::InputVertex ShortestPathExecutorInfos<FinderType>::getTargetVertex()
    const noexcept {
  return _target;
}

template<class FinderType>
static std::string typeToString(
    typename ShortestPathExecutorInfos<FinderType>::OutputName type) {
  switch (type) {
    case ShortestPathExecutorInfos<FinderType>::VERTEX:
      return std::string{"VERTEX"};
    case ShortestPathExecutorInfos<FinderType>::EDGE:
      return std::string{"EDGE"};
    default:
      return std::string{"<INVALID("} + std::to_string(type) + ")>";
  }
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::findRegisterChecked(
    OutputName type) const {
  auto const& it = _registerMapping.find(type);
  if (ADB_UNLIKELY(it == _registerMapping.end())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Logic error: requested unused register type " +
                                typeToString<FinderType>(type));
  }
  return it->second;
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::getOutputRegister(
    OutputName type) const {
  TRI_ASSERT(usesOutputRegister(type));
  return findRegisterChecked(type);
}

template<class FinderType>
graph::TraverserCache* ShortestPathExecutorInfos<FinderType>::cache() const {
  TRI_ASSERT(false);  // TODO [GraphRefactor]:remove me later
  return nullptr;
}

template<class FinderType>
ShortestPathExecutor<FinderType>::ShortestPathExecutor(Fetcher&, Infos& infos)
    : _infos(infos),
      _trx(infos.query().newTrxContext()),
      _inputRow{CreateInvalidInputRowHint{}},
      _finder{infos.finder()},
      _path{new arangodb::graph::ShortestPathResult{}},
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

template<class FinderType>
auto ShortestPathExecutor<FinderType>::doOutputPath(OutputAqlItemRow& output)
    -> void {
  transaction::BuilderLeaser tmp{&_trx};
  tmp->clear();

  bool foundPath = _finder.getNextPath(*tmp.builder());

  if (foundPath) {
    // TODO [GraphRefactor]: This can be optimized. In case we only need to
    // write into the vertex or edge output register, we do not need to build
    // the whole complete path (as we do now).
    TRI_ASSERT(tmp->slice().hasKey(StaticStrings::GraphQueryVertices));
    TRI_ASSERT(tmp->slice().get(StaticStrings::GraphQueryVertices).isArray());
    TRI_ASSERT(tmp->slice().hasKey(StaticStrings::GraphQueryEdges));
    TRI_ASSERT(tmp->slice().get(StaticStrings::GraphQueryEdges).isArray());
    VPackSlice verticesSlice{
        tmp->slice().get(StaticStrings::GraphQueryVertices)};
    VPackSlice edgesSlice{tmp->slice().get(StaticStrings::GraphQueryEdges)};

    for (size_t counter = 0; counter < verticesSlice.length(); counter++) {
      if (_infos.usesOutputRegister(
              ShortestPathExecutorInfos<FinderType>::VERTEX)) {
        AqlValue v{verticesSlice.at(counter)};
        AqlValueGuard vertexGuard{v, true};

        output.moveValueInto(_infos.getOutputRegister(
                                 ShortestPathExecutorInfos<FinderType>::VERTEX),
                             _inputRow, vertexGuard);
      }
      if (_infos.usesOutputRegister(
              ShortestPathExecutorInfos<FinderType>::EDGE)) {
        if (counter == 0) {
          // First Edge is defined as NULL
          AqlValue e{AqlValueHintNull()};
          AqlValueGuard edgeGuard{e, true};
          output.moveValueInto(_infos.getOutputRegister(
                                   ShortestPathExecutorInfos<FinderType>::EDGE),
                               _inputRow, edgeGuard);
        } else {
          AqlValue e{edgesSlice.at(counter - 1)};
          AqlValueGuard edgeGuard{e, true};

          output.moveValueInto(_infos.getOutputRegister(
                                   ShortestPathExecutorInfos<FinderType>::EDGE),
                               _inputRow, edgeGuard);
        }
      }
      output.advanceRow();
    }
  }
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::fetchPath(AqlItemBlockInputRange& input)
    -> bool {
  TRI_ASSERT(_finder.isDone());
  _finder.clear();

  while (input.hasDataRow()) {
    auto source = VPackSlice{};
    auto target = VPackSlice{};
    std::tie(std::ignore, _inputRow) =
        input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(_inputRow.isInitialized());

    // Ordering important here.
    // Read source and target vertex, then try to find a shortest path (if both
    // worked).
    if (getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder,
                    source) &&
        getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder,
                    target)) {
      _finder.reset(arangodb::velocypack::HashedStringRef(source),
                    arangodb::velocypack::HashedStringRef(target));
      return true;
    }
  }
  // Note that we only return false if
  // the input does not have a data row, so if we return false
  // here, we are DONE (we cannot produce any output anymore).
  return false;
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::produceRows(
    AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  while (!output.isFull()) {
    if (_finder.isDone()) {
      if (!fetchPath(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), _finder.stealStats(), AqlCall{}};
      }
    } else {
      doOutputPath(output);
    }
  }

  if (_finder.isDone()) {
    return {input.upstreamState(), _finder.stealStats(), AqlCall{}};
  } else {
    return {ExecutorState::HASMORE, _finder.stealStats(), AqlCall{}};
  }
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::skipRowsRange(
    AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto skipped = size_t{0};

  while (call.shouldSkip()) {
    // _finder.isDone() == true means that there is currently no path available
    // from the _finder, we can try calling fetchPaths to make one available,
    // but if that fails too, we must be DONE
    if (_finder.isDone()) {
      if (!fetchPath(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), _finder.stealStats(), skipped,
                AqlCall{}};
      }
    } else {
      // TODO: Check this. Might be wrong.
      if (_finder.skipPath()) {
        skipped++;
        call.didSkip(1);
      }
    }
  }

  if (_finder.isDone()) {
    return {input.upstreamState(), _finder.stealStats(), skipped, AqlCall{}};
  } else {
    return {ExecutorState::HASMORE, _finder.stealStats(), skipped, AqlCall{}};
  }
}

template<class FinderType>
bool ShortestPathExecutor<FinderType>::getVertexId(
    GraphNode::InputVertex const& vertex, InputAqlItemRow& row,
    VPackBuilder& builder, VPackSlice& id) {
  switch (vertex.type) {
    case GraphNode::InputVertex::Type::REGISTER: {
      AqlValue const& in = row.getValue(vertex.reg);
      if (in.isObject()) {
        try {
          auto idString = _trx.extractIdString(in.slice());
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
    case GraphNode::InputVertex::Type::CONSTANT: {
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

using SingleServer = SingleServerProvider<SingleServerProviderStep>;
using Cluster = ClusterProvider<ClusterProviderStep>;

// Infos
template class ::arangodb::aql::ShortestPathExecutorInfos<
    ShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    TracedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    ShortestPathEnumerator<Cluster>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    TracedShortestPathEnumerator<Cluster>>;

// Executor
template class ::arangodb::aql::ShortestPathExecutor<
    ShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutor<
    ShortestPathEnumerator<Cluster>>;
template class ::arangodb::aql::ShortestPathExecutor<
    TracedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutor<
    TracedShortestPathEnumerator<Cluster>>;
