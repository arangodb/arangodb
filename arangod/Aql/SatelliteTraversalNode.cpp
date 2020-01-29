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

#include "Aql/Collection.h"
#include "Basics/Exceptions.h"
#include "Graph/Graph.h"

using namespace arangodb;
using namespace arangodb::aql;

SatelliteTraversalNode::SatelliteTraversalNode(ExecutionPlan& plan,
                                               TraversalNode const& traversalNode,
                                               aql::Collection const& collection)
    : TraversalNode(plan, traversalNode), CollectionAccessingNode(&collection) {
  TRI_ASSERT(&plan == traversalNode.plan());
  TRI_ASSERT(collection.vocbase() == traversalNode.vocbase());
  TRI_ASSERT(traversalNode.isEligibleAsSatelliteTraversal());
  if (ADB_UNLIKELY(!traversalNode.isEligibleAsSatelliteTraversal())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL_AQL,
        "Logic error: traversal not eligible for satellite traversal");
  }
}
