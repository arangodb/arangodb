////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_HANDLER_REST_EDGES_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_EDGES_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <velocypack/Builder.h>

namespace arangodb {
class SingleCollectionTransaction;

namespace traverser {
class TraverserExpression;
}

class RestEdgesHandler : public RestVocbaseBaseHandler {
 public:
  explicit RestEdgesHandler(GeneralRequest*, GeneralResponse*);

 public:
  status execute() override final;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief reads all edges in given direction for given vertex
  ///        Uses the TraverserExpression for filtering.
  ///        Might be nullptr for no filtering
  //////////////////////////////////////////////////////////////////////////////

  bool readEdges(std::vector<traverser::TraverserExpression*> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reads all edges in given direction for given vertex
  ///        Also parses the body into an TraverserExpression
  //////////////////////////////////////////////////////////////////////////////

  bool readFilteredEdges();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reads all edges in given direction for a given list of vertices
  //////////////////////////////////////////////////////////////////////////////

  bool readEdgesForMultipleVertices();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get all edges for a given vertex. Independent from the request
  //////////////////////////////////////////////////////////////////////////////

  bool getEdgesForVertex(
      std::string const& id, std::string const& collectionName,
      std::vector<traverser::TraverserExpression*> const& expressions,
      TRI_edge_direction_e direction, SingleCollectionTransaction& trx,
      arangodb::velocypack::Builder&, size_t& scannedIndex, size_t& filtered);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get all edges for a list of vertices. Independent from the request
  //////////////////////////////////////////////////////////////////////////////

  bool getEdgesForVertexList(
      arangodb::velocypack::Slice const ids,
      std::vector<traverser::TraverserExpression*> const& expressions,
      TRI_edge_direction_e direction, SingleCollectionTransaction& trx,
      arangodb::velocypack::Builder&, size_t& scannedIndex, size_t& filtered);
};
}

#endif
