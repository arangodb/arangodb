////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "SatelliteTraversalNode.h"

#include "Basics/Exceptions.h"
#include "Graph/Graph.h"

using namespace arangodb;
using namespace arangodb::aql;

SatelliteTraversalNode::SatelliteTraversalNode(TraversalNode&& traversalNode,
                                               aql::Collection const& collection)
    : TraversalNode(std::move(traversalNode)), CollectionAccessingNode(&collection) {
  // TODO Replace the following checks with a check on traversalNode->isEligibleAsSatelliteTraversal().
  if (graph() == nullptr) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL, "Logic error: satellite traversals currently only supported on named graphs");
  }

  if (!graph()->isSatellite()) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL, "Logic error: satellite traversals on non-satellite graph");
  }

}
