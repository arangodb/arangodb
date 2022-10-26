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

using namespace arangodb::velocypack;

namespace arangodb::slicegraph {
SharedSlice setup2Path();

SharedSlice setupUndirectedEdge();

SharedSlice setupThreeDisjointDirectedCycles();

SharedSlice setupThreeDisjointAlternatingCycles();

SharedSlice setup1SingleVertex();

SharedSlice setup2IsolatedVertices();

SharedSlice setup1DirectedTree();

SharedSlice setup1AlternatingTree();

SharedSlice setup2CliquesConnectedByDirectedEdge();

SharedSlice setupDuplicateVertices();

struct GraphSliceHelper {
  static auto getNumVertices(SharedSlice const& graphSlice) -> size_t;

  static auto getNumEdges(SharedSlice const& graphSlice) -> size_t;

};

} // namespace arangodb::slicegraph