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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "./PathResult.h"
#include "Basics/StaticStrings.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class Step>
PathResult<Step>::PathResult() {}

template <class Step>
auto PathResult<Step>::clear() -> void {
  _vertices.clear();
  _edges.clear();
}

template <class Step>
auto PathResult<Step>::appendVertex(typename Step::Vertex v) -> void {
  _vertices.push_back(v);
}

template <class Step>
auto PathResult<Step>::prependVertex(typename Step::Vertex v) -> void {
  _vertices.insert(_vertices.begin(), v);
}

template <class Step>
auto PathResult<Step>::appendEdge(typename Step::Edge e) -> void {
  _edges.push_back(e);
}

template <class Step>
auto PathResult<Step>::prependEdge(typename Step::Edge e) -> void {
  _edges.insert(_edges.begin(), e);
}

template <class Step>
auto PathResult<Step>::toVelocyPack(arangodb::velocypack::Builder& builder) -> void {
  VPackObjectBuilder path{&builder};
  {
    builder.add(VPackValue(StaticStrings::GraphQueryVertices));
    VPackArrayBuilder vertices{&builder};
    for (auto const& v : _vertices) {
      v.addToBuilder(builder);
    }
  }

  {
    builder.add(VPackValue(StaticStrings::GraphQueryEdges));
    VPackArrayBuilder edges(&builder);
    for (auto const& e : _edges) {
      e.addToBuilder(builder);
    }
  }
}

template <class Step>
auto PathResult<Step>::isValid() const -> bool {
  return true;
}