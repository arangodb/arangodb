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

#include "TraversalExecutor.h"

#include <utility>

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutorExpressionContext.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/system-compiler.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Steps/ClusterProviderStep.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

#include "Graph/algorithm-aliases.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::traverser;
using namespace arangodb::graph;

namespace {
auto toHashedStringRef(std::string const& id)
    -> arangodb::velocypack::HashedStringRef {
  return {id.data(), static_cast<uint32_t>(id.length())};
}

template<typename Derived, typename Base, typename Del>
std::unique_ptr<Derived, Del> static_unique_ptr_cast(
    std::unique_ptr<Base, Del>&& p) {
  auto d = static_cast<Derived*>(p.release());
  return std::unique_ptr<Derived, Del>(d, std::move(p.get_deleter()));
}
}  // namespace

// on the coordinator
TraversalExecutorInfos::TraversalExecutorInfos(
    std::unique_ptr<traverser::Traverser>&& traverser,
    std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                       TraversalExecutorInfosHelper::OutputNameHash>
        registerMapping,
    std::string fixedSource, RegisterId inputRegister,
    std::vector<std::pair<Variable const*, RegisterId>>
        filterConditionVariables,
    Ast* ast, traverser::TraverserOptions::UniquenessLevel vertexUniqueness,
    traverser::TraverserOptions::UniquenessLevel edgeUniqueness,
    traverser::TraverserOptions::Order order, bool refactor,
    double defaultWeight, std::string weightAttribute,
    transaction::Methods* trx, arangodb::aql::QueryContext& query,
    arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
    arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
    TraverserOptions* opts,
    ClusterBaseProviderOptions&& clusterBaseProviderOptions, bool isSmart)
    : _traverser(std::move(traverser)),
      _registerMapping(std::move(registerMapping)),
      _fixedSource(std::move(fixedSource)),
      _inputRegister(inputRegister),
      _filterConditionVariables(
          std::move(filterConditionVariables)),  // TODO [GraphRefactor]:
                                                 // Remove the _ as soon as we
                                                 // can get rid of the old code
      _ast(ast),
      _uniqueVertices(vertexUniqueness),
      _uniqueEdges(edgeUniqueness),
      _order(order),
      _refactor(refactor),
      _defaultWeight(defaultWeight),
      _weightAttribute(std::move(weightAttribute)),
      _trx(trx),
      _query(query) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  if (!refactor) {
    TRI_ASSERT(_traverser != nullptr);
  }

  // _fixedSource XOR _inputRegister
  // note: _fixedSource can be the empty string here
  TRI_ASSERT(_fixedSource.empty() ||
             (!_fixedSource.empty() &&
              _inputRegister.value() == RegisterId::maxRegisterId));
  // All Nodes are located in the AST it cannot be non existing.
  TRI_ASSERT(_ast != nullptr);
  if (isRefactor()) {
    /*
     * In the refactored variant we need to parse the correct enumerator type
     * here, before we're allowed to use it.
     */
    TRI_ASSERT(_traversalEnumerator == nullptr);

    parseTraversalEnumeratorCluster(
        getOrder(), getUniqueVertices(), getUniqueEdges(), _defaultWeight,
        _weightAttribute, query, std::move(clusterBaseProviderOptions),
        std::move(pathValidatorOptions), std::move(enumeratorOptions), isSmart);

    TRI_ASSERT(_traversalEnumerator != nullptr);
  }
}

// on non-coordinator (single server or DB-server)
TraversalExecutorInfos::TraversalExecutorInfos(
    std::unique_ptr<traverser::Traverser>&& traverser,
    std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                       TraversalExecutorInfosHelper::OutputNameHash>
        registerMapping,
    std::string fixedSource, RegisterId inputRegister,
    std::vector<std::pair<Variable const*, RegisterId>>
        filterConditionVariables,
    Ast* ast, traverser::TraverserOptions::UniquenessLevel vertexUniqueness,
    traverser::TraverserOptions::UniquenessLevel edgeUniqueness,
    traverser::TraverserOptions::Order order, bool refactor,
    double defaultWeight, std::string weightAttribute,
    transaction::Methods* trx, arangodb::aql::QueryContext& query,
    arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
    arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
    TraverserOptions* opts,
    graph::SingleServerBaseProviderOptions&& singleServerBaseProviderOptions,
    bool isSmart)
    : _traverser(std::move(traverser)),
      _registerMapping(std::move(registerMapping)),
      _fixedSource(std::move(fixedSource)),
      _inputRegister(inputRegister),
      _filterConditionVariables(
          std::move(filterConditionVariables)),  // TODO [GraphRefactor]:
                                                 // Remove the _ as soon as we
                                                 // can get rid of the old code
      _ast(ast),
      _uniqueVertices(vertexUniqueness),
      _uniqueEdges(edgeUniqueness),
      _order(order),
      _refactor(refactor),
      _defaultWeight(defaultWeight),
      _weightAttribute(std::move(weightAttribute)),
      _trx(trx),
      _query(query) {
  if (!refactor) {
    TRI_ASSERT(_traverser != nullptr);
  }

  // _fixedSource XOR _inputRegister
  // note: _fixedSource can be the empty string here
  TRI_ASSERT(_fixedSource.empty() ||
             (!_fixedSource.empty() &&
              _inputRegister.value() == RegisterId::maxRegisterId));
  // All Nodes are located in the AST it cannot be non existing.
  TRI_ASSERT(_ast != nullptr);
  if (isRefactor()) {
    TRI_ASSERT(!ServerState::instance()->isCoordinator());
    /*
     * In the refactored variant we need to parse the correct enumerator type
     * here, before we're allowed to use it.
     */
    TRI_ASSERT(_traversalEnumerator == nullptr);

    parseTraversalEnumeratorSingleServer(
        getOrder(), getUniqueVertices(), getUniqueEdges(), _defaultWeight,
        _weightAttribute, query, std::move(singleServerBaseProviderOptions),
        std::move(pathValidatorOptions), std::move(enumeratorOptions), isSmart);

    TRI_ASSERT(_traversalEnumerator != nullptr);
  }
}

// REFACTOR
arangodb::graph::TraversalEnumerator*
TraversalExecutorInfos::traversalEnumerator() const {
  TRI_ASSERT(_traversalEnumerator != nullptr);
  return _traversalEnumerator.get();
}

// OLD
Traverser* TraversalExecutorInfos::traverser() {
  TRI_ASSERT(_traverser != nullptr);
  return _traverser.get();
}

bool TraversalExecutorInfos::usesOutputRegister(
    TraversalExecutorInfosHelper::OutputName type) const {
  return _registerMapping.find(type) != _registerMapping.end();
}

bool TraversalExecutorInfos::useVertexOutput() const {
  return usesOutputRegister(TraversalExecutorInfosHelper::OutputName::VERTEX);
}

bool TraversalExecutorInfos::useEdgeOutput() const {
  return usesOutputRegister(TraversalExecutorInfosHelper::OutputName::EDGE);
}

bool TraversalExecutorInfos::usePathOutput() const {
  return usesOutputRegister(TraversalExecutorInfosHelper::OutputName::PATH);
}

Ast* TraversalExecutorInfos::getAst() const { return _ast; }

std::string TraversalExecutorInfos::typeToString(
    TraversalExecutorInfosHelper::OutputName type) {
  switch (type) {
    case TraversalExecutorInfosHelper::VERTEX:
      return std::string{"VERTEX"};
    case TraversalExecutorInfosHelper::EDGE:
      return std::string{"EDGE"};
    case TraversalExecutorInfosHelper::PATH:
      return std::string{"PATH"};
    default:
      return std::string{"<INVALID("} + std::to_string(type) + ")>";
  }
}

RegisterId TraversalExecutorInfos::findRegisterChecked(
    TraversalExecutorInfosHelper::OutputName type) const {
  auto const& it = _registerMapping.find(type);
  if (ADB_UNLIKELY(it == _registerMapping.end())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic error: requested unused register type " + typeToString(type));
  }
  return it->second;
}

RegisterId TraversalExecutorInfos::getOutputRegister(
    TraversalExecutorInfosHelper::OutputName type) const {
  TRI_ASSERT(usesOutputRegister(type));
  return findRegisterChecked(type);
}

RegisterId TraversalExecutorInfos::vertexRegister() const {
  return getOutputRegister(TraversalExecutorInfosHelper::OutputName::VERTEX);
}

RegisterId TraversalExecutorInfos::edgeRegister() const {
  return getOutputRegister(TraversalExecutorInfosHelper::OutputName::EDGE);
}

RegisterId TraversalExecutorInfos::pathRegister() const {
  return getOutputRegister(TraversalExecutorInfosHelper::OutputName::PATH);
}

bool TraversalExecutorInfos::usesFixedSource() const {
  return _inputRegister.value() == RegisterId::maxRegisterId;
}

std::string const& TraversalExecutorInfos::getFixedSource() const {
  TRI_ASSERT(usesFixedSource());
  return _fixedSource;
}

RegisterId TraversalExecutorInfos::getInputRegister() const {
  TRI_ASSERT(!usesFixedSource());
  TRI_ASSERT(_inputRegister.isValid());
  return _inputRegister;
}

std::vector<std::pair<Variable const*, RegisterId>> const&
TraversalExecutorInfos::filterConditionVariables() const {
  return _filterConditionVariables;
}

TraverserOptions::UniquenessLevel TraversalExecutorInfos::getUniqueVertices()
    const {
  return _uniqueVertices;
}

TraverserOptions::UniquenessLevel TraversalExecutorInfos::getUniqueEdges()
    const {
  return _uniqueEdges;
}

TraverserOptions::Order TraversalExecutorInfos::getOrder() const {
  return _order;
}

void TraversalExecutorInfos::setOrder(TraverserOptions::Order order) {
  _order = order;
}

bool TraversalExecutorInfos::isRefactor() const { return _refactor; }

transaction::Methods* TraversalExecutorInfos::getTrx() { return _trx; }
arangodb::aql::QueryWarnings& TraversalExecutorInfos::getWarnings() {
  return _query.warnings();
}

std::pair<arangodb::graph::VertexUniquenessLevel,
          arangodb::graph::EdgeUniquenessLevel>
TraversalExecutorInfos::convertUniquenessLevels() const {
  // TODO [GraphRefactor]: This should be a temporary function as we remove
  // TraverserOptions in total after the graph refactor is done.
  auto vertexUniquenessLevel = graph::VertexUniquenessLevel::NONE;
  auto edgeUniquenessLevel = graph::EdgeUniquenessLevel::NONE;

  if (getUniqueVertices() == traverser::TraverserOptions::PATH) {
    vertexUniquenessLevel = graph::VertexUniquenessLevel::PATH;
    edgeUniquenessLevel = graph::EdgeUniquenessLevel::PATH;
  } else if (getUniqueVertices() == traverser::TraverserOptions::GLOBAL) {
    vertexUniquenessLevel = graph::VertexUniquenessLevel::GLOBAL;
    edgeUniquenessLevel = graph::EdgeUniquenessLevel::PATH;
  }

  if (getUniqueEdges() == traverser::TraverserOptions::PATH ||
      getUniqueEdges() == traverser::TraverserOptions::GLOBAL) {
    edgeUniquenessLevel = graph::EdgeUniquenessLevel::PATH;
  }

  return std::make_pair(vertexUniquenessLevel, edgeUniquenessLevel);
}

// TODO [GraphRefactor]: Add a parameter to toggle tracing variants of
// enumerators.
auto TraversalExecutorInfos::parseTraversalEnumeratorSingleServer(
    TraverserOptions::Order order,
    TraverserOptions::UniquenessLevel uniqueVertices,
    TraverserOptions::UniquenessLevel uniqueEdges, double defaultWeight,
    std::string const& weightAttribute, QueryContext& query,
    SingleServerBaseProviderOptions&& baseProviderOptions,
    PathValidatorOptions&& pathValidatorOptions,
    OneSidedEnumeratorOptions&& enumeratorOptions, bool isSmart) -> void {
  if (order == TraverserOptions::Order::WEIGHTED) {
    // It is valid to not have set a weightAttribute.
    // TRI_ASSERT(_opts->hasWeightAttribute());
    if (weightAttribute.empty()) {
      baseProviderOptions.setWeightEdgeCallback(
          [defaultWeight](double previousWeight, VPackSlice edge) -> double {
            return previousWeight + defaultWeight;
          });
    } else {
      baseProviderOptions.setWeightEdgeCallback(
          [weightAttribute = weightAttribute, defaultWeight](
              double previousWeight, VPackSlice edge) -> double {
            auto const weight =
                arangodb::basics::VelocyPackHelper::getNumericValue<double>(
                    edge, weightAttribute, defaultWeight);
            if (weight < 0.) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT);
            }

            return previousWeight + weight;
          });
    }
  }
  // This Assert is not necessary, we could be smart here, for disjoints.
  // However current implementation does not hand in isSmart==true
  // in the valid combination.
  TRI_ASSERT(!isSmart);
  bool useTracing = false;
  TRI_ASSERT(_traversalEnumerator == nullptr);

  _traversalEnumerator = TraversalEnumerator::createEnumerator<
      SingleServerProvider<SingleServerProviderStep>>(
      order, uniqueVertices, uniqueEdges, query, std::move(baseProviderOptions),
      std::move(pathValidatorOptions), std::move(enumeratorOptions),
      useTracing);

  TRI_ASSERT(_traversalEnumerator != nullptr);
}

auto TraversalExecutorInfos::parseTraversalEnumeratorCluster(
    traverser::TraverserOptions::Order order,
    traverser::TraverserOptions::UniquenessLevel uniqueVertices,
    traverser::TraverserOptions::UniquenessLevel uniqueEdges,
    double defaultWeight, const std::string& weightAttribute,
    arangodb::aql::QueryContext& query,
    arangodb::graph::ClusterBaseProviderOptions&& baseProviderOptions,
    arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
    arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
    bool isSmart) -> void {
  if (order == TraverserOptions::Order::WEIGHTED) {
    // It is valid to not have set a weightAttribute.
    // TRI_ASSERT(_opts->hasWeightAttribute());
    if (weightAttribute.empty()) {
      baseProviderOptions.setWeightEdgeCallback(
          [defaultWeight](double previousWeight, VPackSlice edge) -> double {
            return previousWeight + defaultWeight;
          });
    } else {
      baseProviderOptions.setWeightEdgeCallback(
          [weightAttribute = weightAttribute, defaultWeight](
              double previousWeight, VPackSlice edge) -> double {
            auto const weight =
                arangodb::basics::VelocyPackHelper::getNumericValue<double>(
                    edge, weightAttribute, defaultWeight);
            if (weight < 0.) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT);
            }

            return previousWeight + weight;
          });
    }
  }
  TRI_ASSERT(baseProviderOptions.getCache() != nullptr);
  bool useTracing = false;
  TRI_ASSERT(_traversalEnumerator == nullptr);

#ifdef USE_ENTERPRISE
  if (isSmart) {
    baseProviderOptions.setRPCCommunicator(
        std::make_unique<enterprise::SmartGraphRPCCommunicator>(
            query, query.resourceMonitor(), baseProviderOptions.engines()));
    _traversalEnumerator = TraversalEnumerator::createEnumerator<
        enterprise::SmartGraphProvider<ClusterProviderStep>>(
        order, uniqueVertices, uniqueEdges, query,
        std::move(baseProviderOptions), std::move(pathValidatorOptions),
        std::move(enumeratorOptions), useTracing);
  } else {
    _traversalEnumerator = TraversalEnumerator::createEnumerator<
        ClusterProvider<ClusterProviderStep>>(
        order, uniqueVertices, uniqueEdges, query,
        std::move(baseProviderOptions), std::move(pathValidatorOptions),
        std::move(enumeratorOptions), useTracing);
  }
#else
  _traversalEnumerator = TraversalEnumerator::createEnumerator<
      ClusterProvider<ClusterProviderStep>>(
      order, uniqueVertices, uniqueEdges, query, std::move(baseProviderOptions),
      std::move(pathValidatorOptions), std::move(enumeratorOptions),
      useTracing);
#endif

  TRI_ASSERT(_traversalEnumerator != nullptr);
}

TraversalExecutor::TraversalExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _inputRow{CreateInvalidInputRowHint{}},
      _traverser(nullptr),
      _traversalEnumerator(nullptr) {
  // reset the traverser, so that no residual state is left in it. This is
  // important because the TraversalExecutor is sometimes reconstructed (in
  // place) with the same TraversalExecutorInfos as before. Those
  // infos contain the traverser which might contain state from a previous run.
  if (infos.isRefactor()) {
    _traversalEnumerator = infos.traversalEnumerator();
    traversalEnumerator()->clear(false);
  } else {
    _traverser = infos.traverser();
    traverser()->done();
  }
}

TraversalExecutor::~TraversalExecutor() {
  if (!_infos.isRefactor()) {
    auto opts = traverser()->options();
    if (opts != nullptr) {
      // The InAndOutRowExpressionContext in the PruneExpressionEvaluator
      // holds an InputAqlItemRow. As the Plan holds the
      // PruneExpressionEvaluator and is destroyed after the Engine, this must
      // be deleted by unPrepareContext() - otherwise, the
      // SharedAqlItemBlockPtr referenced by the row will return its
      // AqlItemBlock to an already destroyed AqlItemBlockManager.
      if (opts->usesPrune()) {
        auto* evaluator = opts->getPruneEvaluator();
        if (evaluator != nullptr) {
          evaluator->unPrepareContext();
        }
      }
      if (opts->usesPostFilter()) {
        auto* evaluator = opts->getPostFilterEvaluator();
        if (evaluator != nullptr) {
          evaluator->unPrepareContext();
        }
      }
    }
  } else {
    traversalEnumerator()->clear(false);
    traversalEnumerator()->unprepareValidatorContext();
  }
}

auto TraversalExecutor::doOutput(OutputAqlItemRow& output) -> void {
  if (!_infos.isRefactor()) {
    while (!output.isFull() && traverser()->hasMore() && traverser()->next()) {
      TRI_ASSERT(_inputRow.isInitialized());

      // traverser now has next v, e, p values
      if (_infos.useVertexOutput()) {
        AqlValue vertex = traverser()->lastVertexToAqlValue();
        AqlValueGuard guard{vertex, true};
        output.moveValueInto(_infos.vertexRegister(), _inputRow, guard);
      }
      if (_infos.useEdgeOutput()) {
        AqlValue edge = traverser()->lastEdgeToAqlValue();
        AqlValueGuard guard{edge, true};
        output.moveValueInto(_infos.edgeRegister(), _inputRow, guard);
      }
      if (_infos.usePathOutput()) {
        transaction::BuilderLeaser tmp(traverser()->trx());
        AqlValue path = traverser()->pathToAqlValue(*tmp.builder());
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.pathRegister(), _inputRow, guard);
      }

      // No output is requested from the register plan. We still need
      // to copy the inputRow for the query to yield correct results.
      if (!_infos.useVertexOutput() && !_infos.useEdgeOutput() &&
          !_infos.usePathOutput()) {
        output.copyRow(_inputRow);
      }

      output.advanceRow();
    }
  } else {
    // Refactored variant
    auto currentPath = traversalEnumerator()->getNextPath();
    if (currentPath != nullptr) {
      TRI_ASSERT(_inputRow.isInitialized());

      // traverser now has next v, e, p values
      transaction::BuilderLeaser tmp{_infos.getTrx()};

      // Vertex variable (v)
      if (_infos.useVertexOutput()) {
        tmp->clear();
        currentPath->lastVertexToVelocyPack(*tmp.builder());
        AqlValue path{tmp->slice()};
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.vertexRegister(), _inputRow, guard);
      }

      // Edge variable (e)
      if (_infos.useEdgeOutput()) {
        tmp->clear();
        currentPath->lastEdgeToVelocyPack(*tmp.builder());
        AqlValue path{tmp->slice()};
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.edgeRegister(), _inputRow, guard);
      }

      // Path variable (p)
      if (_infos.usePathOutput()) {
        tmp->clear();
        currentPath->toVelocyPack(*tmp.builder());
        AqlValue path{tmp->slice()};
        AqlValueGuard guard{path, true};
        output.moveValueInto(_infos.pathRegister(), _inputRow, guard);
      }

      // No output is requested from the register plan. We still need
      // to copy the inputRow for the query to yield correct results.
      if (!_infos.useVertexOutput() && !_infos.useEdgeOutput() &&
          !_infos.usePathOutput()) {
        output.copyRow(_inputRow);
      }

      output.advanceRow();
    }
  }
}

auto TraversalExecutor::doSkip(AqlCall& call) -> size_t {
  auto skip = size_t{0};

  if (!_infos.isRefactor()) {
    while (call.shouldSkip() && traverser()->hasMore() && traverser()->next()) {
      TRI_ASSERT(_inputRow.isInitialized());
      skip++;
      call.didSkip(1);
    }
  } else {
    // refactored variant
    while (call.shouldSkip()) {
      if (traversalEnumerator()->skipPath()) {
        TRI_ASSERT(_inputRow.isInitialized());
        skip++;
        call.didSkip(1);
      }
    }
  }

  return skip;
}

auto TraversalExecutor::produceRows(AqlItemBlockInputRange& input,
                                    OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  if (!_infos.isRefactor()) {
    ExecutorState state{ExecutorState::HASMORE};
    TraversalStats oldStats;
    while (true) {
      if (traverser()->hasMore()) {
        TRI_ASSERT(_inputRow.isInitialized());
        doOutput(output);

        if (output.isFull()) {
          if (traverser()->hasMore()) {
            state = ExecutorState::HASMORE;
          } else {
            state = input.upstreamState();
          }
          break;
        }
      } else {
        if (!initTraverser(input)) {
          state = input.upstreamState();
          break;
        }
        TRI_ASSERT(_inputRow.isInitialized());
      }
    }

    oldStats.incrScannedIndex(traverser()->getAndResetReadDocuments());
    oldStats.incrFiltered(traverser()->getAndResetFiltered());
    oldStats.incrHttpRequests(traverser()->getAndResetHttpRequests());
    // note: deprecated.
    // cursorsCreated, cursorsRearmed, cacheHits, cacheMisses are
    // intentionally not added here.

    return {state, oldStats, AqlCall{}};
  } else {
    // refactored variant
    while (!output.isFull()) {
      if (traversalEnumerator()->isDone()) {
        if (!initTraverser(input)) {  // will set a new start vertex
          TRI_ASSERT(!input.hasDataRow());
          return {input.upstreamState(), stats(), AqlCall{}};
        }
      } else {
        doOutput(output);
      }
    }

    if (traversalEnumerator()->isDone()) {
      return {input.upstreamState(), stats(), AqlCall{}};
    } else {
      return {ExecutorState::HASMORE, stats(), AqlCall{}};
    }
  }
}

auto TraversalExecutor::skipRowsRange(AqlItemBlockInputRange& input,
                                      AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto skipped = size_t{0};

  if (!_infos.isRefactor()) {
    TraversalStats oldStats{};
    while (true) {
      skipped += doSkip(call);

      oldStats.incrScannedIndex(traverser()->getAndResetReadDocuments());
      oldStats.incrFiltered(traverser()->getAndResetFiltered());
      oldStats.incrHttpRequests(traverser()->getAndResetHttpRequests());
      // note: deprecated.
      // cursorsCreated, cursorsRearmed, cacheHits, cacheMisses are
      // intentionally not added here.

      if (!traverser()->hasMore()) {
        if (!initTraverser(input)) {
          return {input.upstreamState(), oldStats, skipped, AqlCall{}};
        }
      } else {
        TRI_ASSERT(call.getOffset() == 0);
        return {ExecutorState::HASMORE, oldStats, skipped, AqlCall{}};
      }
    }
  } else {
    // refactored variant
    while (call.shouldSkip()) {
      if (traversalEnumerator()->isDone()) {
        if (!initTraverser(input)) {
          TRI_ASSERT(!input.hasDataRow());
          return {input.upstreamState(), stats(), skipped, AqlCall{}};
        }
      } else {
        if (traversalEnumerator()->skipPath()) {
          skipped++;
          call.didSkip(1);
        }
      }
    }

    if (traversalEnumerator()->isDone()) {
      return {input.upstreamState(), stats(), skipped, AqlCall{}};
    } else {
      return {ExecutorState::HASMORE, stats(), skipped, AqlCall{}};
    }
  }

  TRI_ASSERT(false);
}

//
// Set a new start vertex for traversal, for this fetch inputs
// from input until we are either successful or input is unwilling
// to give us more.
//
// TODO: this is quite a big function, refactor
bool TraversalExecutor::initTraverser(AqlItemBlockInputRange& input) {
  if (!_infos.isRefactor()) {
    traverser()->clear();
    auto opts = traverser()->options();
    opts->clearVariableValues();

    // Now reset the traverser
    // NOTE: It is correct to ask for whether there is a data row here
    //       even if we're using a constant start vertex, as we expect
    //       to provide output for every input row
    while (input.hasDataRow()) {
      // Try to acquire a starting vertex
      std::tie(std::ignore, _inputRow) =
          input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
      TRI_ASSERT(_inputRow.isInitialized());

      // register temporary variables in expression context
      for (auto const& pair : _infos.filterConditionVariables()) {
        opts->setVariableValue(pair.first, _inputRow.getValue(pair.second));
      }

      if (opts->usesPrune()) {
        auto* evaluator = opts->getPruneEvaluator();
        // Replace by inputRow
        evaluator->prepareContext(_inputRow);
        TRI_ASSERT(_inputRow.isInitialized());
      }

      if (opts->usesPostFilter()) {
        auto* evaluator = opts->getPostFilterEvaluator();
        // Replace by inputRow
        evaluator->prepareContext(_inputRow);
        TRI_ASSERT(_inputRow.isInitialized());
      }

      // TODO [GraphRefactor]: Check how indices are being calculated
      opts->calculateIndexExpressions(_infos.getAst());

      std::string sourceString;
      TRI_ASSERT(_inputRow.isInitialized());

      if (_infos.usesFixedSource()) {
        sourceString = _infos.getFixedSource();
      } else {
        AqlValue const& in = _inputRow.getValue(_infos.getInputRegister());
        if (in.isObject()) {
          try {
            sourceString =
                traverser()->options()->trx()->extractIdString(in.slice());
          } catch (...) {
            // on purpose ignore this error.
          }
        } else if (in.isString()) {
          sourceString = in.slice().copyString();
        }
      }

      auto pos = sourceString.find('/');

      if (pos == std::string::npos) {
        traverser()->options()->query().warnings().registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for traversal: Only "
            "id strings or objects with _id are "
            "allowed");
      } else {
        traverser()->setStartVertex(sourceString);
        TRI_ASSERT(_inputRow.isInitialized());
        return true;
      }
    }
    return false;
  } else {
    // refactored variant
    TRI_ASSERT(traversalEnumerator()->isDone());

    while (input.hasDataRow()) {
      std::tie(std::ignore, _inputRow) = input.nextDataRow();

      std::string sourceString;
      TRI_ASSERT(_inputRow.isInitialized());

      traversalEnumerator()->unprepareValidatorContext();
      traversalEnumerator()->setValidatorContext(_inputRow);

      if (_infos.usesFixedSource()) {
        sourceString = _infos.getFixedSource();
      } else {
        AqlValue const& in = _inputRow.getValue(_infos.getInputRegister());
        if (in.isObject()) {
          try {
            sourceString = _infos.getTrx()->extractIdString(in.slice());
          } catch (...) {
            // on purpose ignore this error.
          }
        } else if (in.isString()) {
          sourceString = in.slice().copyString();
        }
      }

      auto pos = sourceString.find('/');

      if (pos == std::string::npos) {
        _infos.getWarnings().registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for traversal: Only "
            "id strings or objects with _id are "
            "allowed");
      } else {
        // prepare index
        traversalEnumerator()->prepareIndexExpressions(_infos.getAst());

        // start actual search
        traversalEnumerator()->reset(toHashedStringRef(
            sourceString));  // TODO [GraphRefactor]: check sourceString memory
        TRI_ASSERT(_inputRow.isInitialized());
        return true;
      }
    }
    return false;
  }
}

[[nodiscard]] auto TraversalExecutor::stats() -> Stats {
  if (!_infos.isRefactor()) {
    // No Stats available on original variant
    return TraversalStats{};
  } else {
    return traversalEnumerator()->stealStats();
  }
}
traverser::Traverser* TraversalExecutor::traverser() {
  TRI_ASSERT(_traverser != nullptr);
  return _traverser;
}

arangodb::graph::TraversalEnumerator* TraversalExecutor::traversalEnumerator() {
  TRI_ASSERT(_traversalEnumerator != nullptr);
  return _traversalEnumerator;
}
