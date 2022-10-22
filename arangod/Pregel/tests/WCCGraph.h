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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Basics/VelocyPackStringLiteral.h>
#include <Inspection/VPackPure.h>

#include "Graph.h"
#include "DisjointSet.h"

using namespace arangodb::velocypack;


using VertexIndex = size_t;
using EdgeIndex = size_t;


struct WCCVertexProperties {
  uint64_t value;
};

template<typename Inspector>
auto inspect(Inspector& f, WCCVertexProperties& x) {
  return f.object(x).fields(f.field("value", x.value));
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
class WCCGraph : public ValuedGraph<EdgeProperties, VertexProperties> {
 public:
  using WCCEdge = BaseEdge<EdgeProperties>;

  WCCGraph(SharedSlice const& graphJson, bool checkDuplicateVertices);

  DisjointSet wccs;
};

