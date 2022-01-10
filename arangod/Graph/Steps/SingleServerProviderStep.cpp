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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "./SingleServerProviderStep.h"

#include "Graph/Providers/SingleServerProvider.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace graph {
auto operator<<(std::ostream& out, SingleServerProviderStep const& step)
    -> std::ostream& {
  out << step._vertex.getID();
  return out;
}
}  // namespace graph
}  // namespace arangodb

SingleServerProviderStep::SingleServerProviderStep(VertexType v)
    : _vertex(v), _edge() {}

SingleServerProviderStep::SingleServerProviderStep(VertexType v, size_t depth,
                                                   double weight)
    : BaseStep(std::numeric_limits<size_t>::max(), depth, weight),
      _vertex(v),
      _edge() {}

SingleServerProviderStep::SingleServerProviderStep(VertexType v,
                                                   EdgeDocumentToken edge,
                                                   size_t prev)
    : BaseStep(prev), _vertex(v), _edge(std::move(edge)) {}

SingleServerProviderStep::SingleServerProviderStep(VertexType v,
                                                   EdgeDocumentToken edge,
                                                   size_t prev, size_t depth,
                                                   double weight, size_t)
    : BaseStep(prev, depth, weight), _vertex(v), _edge(std::move(edge)) {}

SingleServerProviderStep::~SingleServerProviderStep() = default;

VertexType const& SingleServerProviderStep::Vertex::getID() const {
  return _vertex;
}

SingleServerProviderStep::StepType const&
SingleServerProviderStep::Edge::getID() const {
  return _token;
}

bool SingleServerProviderStep::Edge::isValid() const {
  return getID().isValid();
};

void SingleServerProviderStep::Edge::addToBuilder(
    SingleServerProvider<SingleServerProviderStep>& provider,
    arangodb::velocypack::Builder& builder) const {
  provider.insertEdgeIntoResult(getID(), builder);
}

#ifndef USE_ENTERPRISE
bool SingleServerProviderStep::isResponsible(transaction::Methods* trx) const {
  return true;
};
#endif
