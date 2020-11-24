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

#include "./MockGraph.h"

#include "Basics/StaticStrings.h"

#include "Futures/Future.h"
#include "Futures/Utilities.h"
#include "Aql/QueryContext.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;

namespace arangodb {
namespace tests {
namespace graph {
auto operator<<(std::ostream& out, MockGraphProvider::Step const& step) -> std::ostream& {
  out << step._vertex.data();
  return out;
}
}
}
}

MockGraphProvider::Step::Step(VertexType v, bool isProcessable)
    : arangodb::graph::BaseStep<Step>{},
      _vertex(v),
      _edge(std::nullopt),
      _isProcessable(isProcessable) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, EdgeType e, bool isProcessable)
    : arangodb::graph::BaseStep<Step>{prev},
      _vertex(v),
      _edge(e),
      _isProcessable(isProcessable) {}

MockGraphProvider::Step::~Step() {}

void MockGraphProvider::Step::Vertex::addToBuilder(arangodb::velocypack::Builder& builder) const {
  // TODO: works only if collection name stays "v/"
  std::string id = _vertex.toString();
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(id.substr(2)));
  builder.add(StaticStrings::IdString, VPackValue(id));
  builder.close();
}

void MockGraphProvider::Step::Edge::addToBuilder(arangodb::velocypack::Builder& builder) const {
  _edge.addToBuilder(builder);
}

arangodb::velocypack::HashedStringRef MockGraphProvider::Step::Vertex::getId() const {
  return _vertex;
}

MockGraphProvider::MockGraphProvider(MockGraph const& data,
                                     arangodb::aql::QueryContext& queryContext,
                                     LooseEndBehaviour looseEnds, bool reverse)
: _trx(queryContext.newTrxContext()), _reverse(reverse), _looseEnds(looseEnds) {
  for (auto const& it : data.edges()) {
    _fromIndex[it._from].push_back(it);
    _toIndex[it._to].push_back(it);
  }
}

MockGraphProvider::~MockGraphProvider() {}

auto MockGraphProvider::decideProcessable() const -> bool {
  switch (_looseEnds) {
    case LooseEndBehaviour::NEVER:
      return true;
    case LooseEndBehaviour::ALLWAYS:
      return false;
  }
}

auto MockGraphProvider::startVertex(VertexType v) -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Start Vertex:" << v;
  return Step(v, decideProcessable());
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

auto MockGraphProvider::expand(Step const& source, size_t previousIndex)
    -> std::vector<Step> {
  LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Expanding...";
  std::vector<Step> result{};

  LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Searching: " << source.getVertex().data().toString();

  if (_reverse) {
    LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
        << "<MockGraphProvider - reverse> _toIndex size: " << _toIndex.size();
    if (_toIndex.find(source.getVertex().data().toString()) != _toIndex.end()) {
      for (auto const& edge : _toIndex[source.getVertex().data().toString()]) {
        VPackHashedStringRef fromH{edge._from.c_str(),
                                   static_cast<uint32_t>(edge._from.length())};
        result.push_back(Step{previousIndex, fromH, edge, decideProcessable()});

        LOG_TOPIC("78158", TRACE, Logger::GRAPHS)
            << "  <MockGraphProvider> added <Step><Vertex>: " << fromH
            << ", Edge: " << edge.toString() << ", previous: " << previousIndex;
      }
    }
  } else {
    LOG_TOPIC("78157", TRACE, Logger::GRAPHS)
        << "<MockGraphProvider - default> _fromIndex size: " << _fromIndex.size();
    if (_fromIndex.find(source.getVertex().data().toString()) != _fromIndex.end()) {
      for (auto const& edge : _fromIndex[source.getVertex().data().toString()]) {
        VPackHashedStringRef toH{edge._to.c_str(),
                                 static_cast<uint32_t>(edge._to.length())};
        result.push_back(Step{previousIndex, toH, edge, decideProcessable()});

        LOG_TOPIC("78159", TRACE, Logger::GRAPHS)
            << "  <MockGraphProvider - default> added <Step><Vertex>: " << toH
            << ", Edge: " << edge.toString() << ", previous: " << previousIndex;
      }
    }
  }
  LOG_TOPIC("78160", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Expansion length: " << result.size();
  return result;
}

[[nodiscard]] transaction::Methods* MockGraphProvider::trx() {
  return &_trx;
}
