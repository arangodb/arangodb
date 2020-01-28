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

#ifndef ARANGOD_AQL_SATELLITE_TRAVERSAL_NODE_H
#define ARANGOD_AQL_SATELLITE_TRAVERSAL_NODE_H

#include "Aql/CollectionAccessingNode.h"
#include "Aql/TraversalNode.h"

namespace arangodb::aql {

class SatelliteTraversalNode : public TraversalNode, public CollectionAccessingNode {
  SatelliteTraversalNode(TraversalNode&& traversalNode, aql::Collection const& collection);
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_SATELLITE_TRAVERSAL_NODE_H
