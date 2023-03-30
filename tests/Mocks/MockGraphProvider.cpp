////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

#include "MockGraphProvider.h"

#include "Basics/StaticStrings.h"

#include "Aql/QueryContext.h"
#include "Futures/Future.h"
#include "Futures/Utilities.h"
#include "Aql/InputAqlItemRow.h"
#include "Graph/EdgeDocumentToken.h"
#include "Logger/LogMacros.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Value.h>
#include <limits>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;

namespace arangodb {
namespace tests {
namespace graph {
auto operator<<(std::ostream& out, MockGraphProvider::Step const& step)
    -> std::ostream& {
  out << step._vertex.getID();
  return out;
}
}  // namespace graph
}  // namespace tests
}  // namespace arangodb

MockGraphProvider::Step::Step(VertexType v, bool isProcessable)
    : arangodb::graph::BaseStep<Step>{std::numeric_limits<size_t>::max(), 0,
                                      0.0},
      _vertex(v),
      _edge({}),
      _isProcessable(isProcessable) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, MockEdgeType e,
                              bool isProcessable)
    : arangodb::graph::BaseStep<Step>{prev},
      _vertex(v),
      _edge(e),
      _isProcessable(isProcessable) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, bool isProcessable,
                              size_t depth)
    : arangodb::graph::BaseStep<Step>{prev, depth},
      _vertex(v),
      _edge({}),
      _isProcessable(isProcessable) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, bool isProcessable,
                              size_t depth, double weight)
    : arangodb::graph::BaseStep<Step>{prev, depth, weight},
      _vertex(v),
      _edge({}),
      _isProcessable(isProcessable) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, MockEdgeType e,
                              bool isProcessable, size_t depth)
    : arangodb::graph::BaseStep<Step>{prev, depth},
      _vertex(v),
      _edge(e),
      _isProcessable(isProcessable) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, MockEdgeType e,
                              bool isProcessable, size_t depth, double weight)
    : arangodb::graph::BaseStep<Step>{prev, depth, weight},
      _vertex(v),
      _edge(e),
      _isProcessable(isProcessable) {}

MockGraphProvider::MockGraphProvider(arangodb::aql::QueryContext& queryContext,
                                     MockGraphProviderOptions opts,
                                     arangodb::ResourceMonitor&)
    : _trx(queryContext.newTrxContext()),
      _reverse(opts.reverse()),
      _looseEnds(opts.looseEnds()),
      _stats{} {
  for (auto const& it : opts.data().edges()) {
    _fromIndex[it._from].push_back(it);
    _toIndex[it._to].push_back(it);
  }
  if (opts._weightCallback.has_value()) {
    setWeightEdgeCallback(opts._weightCallback.value());
  }
}

MockGraphProvider::~MockGraphProvider() {}

auto MockGraphProvider::decideProcessable() const -> bool {
  switch (_looseEnds) {
    case LooseEndBehaviour::NEVER:
      return true;
    case LooseEndBehaviour::ALWAYS:
      return false;
    default:
      return true;
  }
}

auto MockGraphProvider::startVertex(VertexType v, size_t depth, double weight)
    -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Start Vertex:" << v;
  TRI_ASSERT(weight == 0.0);  // Not handled yet
  return Step(v, decideProcessable());
}

auto MockGraphProvider::fetchVertices(const std::vector<Step*>& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  return fetch(looseEnds);
}

auto MockGraphProvider::fetchEdges(const std::vector<Step*>& fetchedVertices)
    -> Result {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

auto MockGraphProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Fetching...";
  std::vector<Step*> result{};
  result.reserve(looseEnds.size());
  for (auto* s : looseEnds) {
    // We can fake them here.
    s->resolve();
    result.emplace_back(s);
  }
  return futures::makeFuture(std::move(result));
}

auto MockGraphProvider::expand(Step const& step, size_t previous,
                               std::function<void(Step)> callback) -> void {
  std::vector<Step> results{};
  results = expand(step, previous);
  for (auto const& s : results) {
    callback(s);
  }
}

auto MockGraphProvider::clear() -> void {}

auto MockGraphProvider::addVertexToBuilder(
    const Step::Vertex& vertex, arangodb::velocypack::Builder& builder)
    -> void {
  std::string id = vertex.getID().toString();
  _stats.incrScannedIndex(1);
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(id.substr(2)));
  builder.add(StaticStrings::IdString, VPackValue(id));
  builder.close();
}

auto MockGraphProvider::addEdgeToBuilder(const Step::Edge& edge,
                                         arangodb::velocypack::Builder& builder)
    -> void {
  std::string fromId = edge.getEdge()._from;
  std::string toId = edge.getEdge()._to;
  std::string keyId = fromId.substr(2) + "-" + toId.substr(2);

  builder.openObject();
  builder.add(StaticStrings::IdString, VPackValue("e/" + keyId));
  builder.add(StaticStrings::KeyString, VPackValue(keyId));
  builder.add(StaticStrings::FromString, VPackValue(fromId));
  builder.add(StaticStrings::ToString, VPackValue(toId));
  builder.add("weight", VPackValue(edge.getEdge()._weight));
  builder.close();
}

auto MockGraphProvider::getEdgeDocumentToken(const Step::Edge& edge)
    -> arangodb::graph::EdgeDocumentToken {
  VPackBuilder builder;
  addEdgeToBuilder(edge, builder);

  // Might require datalake as well, as soon as we really use this method in our
  // cpp tests.
  return arangodb::graph::EdgeDocumentToken{builder.slice()};
}

auto MockGraphProvider::addEdgeIDToBuilder(
    const Step::Edge& edge, arangodb::velocypack::Builder& builder) -> void {
  std::string fromId = edge.getEdge()._from;
  std::string toId = edge.getEdge()._to;
  std::string keyId = fromId.substr(2) + "-" + toId.substr(2);
  builder.add(VPackValue("e/" + keyId));
}

void MockGraphProvider::addEdgeToLookupMap(
    typename Step::Edge const& edge, arangodb::velocypack::Builder& builder) {
  TRI_ASSERT(builder.isOpenObject());
  std::string fromId = edge.getEdge()._from;
  std::string toId = edge.getEdge()._to;
  std::string keyId = fromId.substr(2) + "-" + toId.substr(2);
  builder.add(VPackValue("e/" + keyId));
  builder.openObject();
  builder.add(StaticStrings::IdString, VPackValue("e/" + keyId));
  builder.add(StaticStrings::KeyString, VPackValue(keyId));
  builder.add(StaticStrings::FromString, VPackValue(fromId));
  builder.add(StaticStrings::ToString, VPackValue(toId));
  builder.add("weight", VPackValue(edge.getEdge()._weight));
  builder.close();
}

auto MockGraphProvider::getEdgeId(const Step::Edge& edge) -> std::string {
  std::string fromId = edge.getEdge()._from;
  std::string toId = edge.getEdge()._to;
  std::string keyId = fromId.substr(2) + "-" + toId.substr(2);
  return "e/" + keyId;
}

auto MockGraphProvider::getEdgeIdRef(const Step::Edge& edge)
    -> VPackHashedStringRef {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto MockGraphProvider::expand(Step const& source, size_t previousIndex)
    -> std::vector<Step> {
  LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Expanding...";
  std::vector<Step> result{};

  LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Searching: "
      << source.getVertex().getID().toString();

  if (_reverse) {
    LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
        << "<MockGraphProvider - reverse> _toIndex size: " << _toIndex.size();
    if (_toIndex.find(source.getVertex().getID().toString()) !=
        _toIndex.end()) {
      for (auto const& edge : _toIndex[source.getVertex().getID().toString()]) {
        VPackHashedStringRef fromH{edge._from.c_str(),
                                   static_cast<uint32_t>(edge._from.length())};
        if (_weightCallback.has_value()) {
          VPackBuilder builder;
          edge.addToBuilder(builder);
          result.push_back(
              Step{previousIndex, fromH, edge, decideProcessable(),
                   (source.getDepth() + 1),
                   (*_weightCallback)(source.getWeight(), builder.slice())});
        } else {
          result.push_back(Step{previousIndex, fromH, edge, decideProcessable(),
                                (source.getDepth() + 1)});
        }

        LOG_TOPIC("78158", TRACE, Logger::GRAPHS)
            << "  <MockGraphProvider> added <Step><Vertex>: " << fromH
            << ", Edge: " << edge.toString() << ", previous: " << previousIndex;
      }
    }
  } else {
    LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
        << "<MockGraphProvider - default> _fromIndex size: "
        << _fromIndex.size();
    if (_fromIndex.find(source.getVertex().getID().toString()) !=
        _fromIndex.end()) {
      for (auto const& edge :
           _fromIndex[source.getVertex().getID().toString()]) {
        VPackHashedStringRef toH{edge._to.c_str(),
                                 static_cast<uint32_t>(edge._to.length())};
        if (_weightCallback.has_value()) {
          VPackBuilder builder;
          edge.addToBuilder(builder);
          result.push_back(
              Step{previousIndex, toH, edge, decideProcessable(),
                   (source.getDepth() + 1),
                   (*_weightCallback)(source.getWeight(), builder.slice())});
        } else {
          result.push_back(Step{previousIndex, toH, edge, decideProcessable(),
                                (source.getDepth() + 1)});
        }

        LOG_TOPIC("78159", TRACE, Logger::GRAPHS)
            << "  <MockGraphProvider - default> added <Step><Vertex>: " << toH
            << ", Edge: " << edge.toString() << ", previous: " << previousIndex;
      }
    }
  }
  LOG_TOPIC("78160", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Expansion length: " << result.size();
  _stats.incrScannedIndex(result.size());
  return result;
}

void MockGraphProvider::prepareIndexExpressions(aql::Ast* ast) {
  // Nothing to do here. We do not have any special index conditions
}

void MockGraphProvider::prepareContext(aql::InputAqlItemRow input) {
  // Nothing to do here. We do not have any special index conditions
}

void MockGraphProvider::unPrepareContext() {
  // Nothing to do here. We do not have any special index conditions
}

bool MockGraphProvider::isResponsible(Step const& step) const { return true; }

bool MockGraphProvider::hasDepthSpecificLookup(uint64_t depth) const noexcept {
  // TODO: This needs to be implemented / checked.
  LOG_DEVEL << "----- Adjustments needed here -----";
  return false;
}
[[nodiscard]] transaction::Methods* MockGraphProvider::trx() { return &_trx; }
[[nodiscard]] TRI_vocbase_t const& MockGraphProvider::vocbase() const {
  return _trx.vocbase();
}

aql::TraversalStats MockGraphProvider::stealStats() {
  auto t = _stats;
  // Placement new of stats, do not reallocate space.
  _stats.~TraversalStats();
  new (&_stats) aql::TraversalStats{};
  return t;
}
