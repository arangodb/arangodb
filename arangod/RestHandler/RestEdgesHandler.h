////////////////////////////////////////////////////////////////////////////////
/// @brief edges request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_HANDLER_REST_EDGES_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_EDGES_HANDLER_H 1

#include "Basics/Common.h"

#include "RestHandler/RestVocbaseBaseHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  RestEdgesHandler
// -----------------------------------------------------------------------------

namespace triagens {
namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace traverser {
class TraverserExpression;
}
// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

class RestEdgesHandler : public RestVocbaseBaseHandler {
  // -----------------------------------------------------------------------------
  // --SECTION--                                      constructors and
  // destructors
  // -----------------------------------------------------------------------------

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief constructor
  ////////////////////////////////////////////////////////////////////////////////

  explicit RestEdgesHandler(rest::HttpRequest*);

  // -----------------------------------------------------------------------------
  // --SECTION--                                                   Handler
  // methods
  // -----------------------------------------------------------------------------

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// {@inheritDoc}
  ////////////////////////////////////////////////////////////////////////////////

  status_t execute() override final;

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 protected
  // methods
  // -----------------------------------------------------------------------------

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief reads all edges in given direction for given vertex
  ///        Uses the TraverserExpression for filtering.
  ///        Might be nullptr for no filtering
  ////////////////////////////////////////////////////////////////////////////////

  bool readEdges(std::vector<traverser::TraverserExpression*> const&);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief reads all edges in given direction for given vertex
  ///        Also parses the body into an TraverserExpression
  ////////////////////////////////////////////////////////////////////////////////

  bool readFilteredEdges();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief reads all edges in given direction for a given list of vertices
  ////////////////////////////////////////////////////////////////////////////////

  bool readEdgesForMultipleVertices();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get all edges for a given vertex. Independent from the request
  ////////////////////////////////////////////////////////////////////////////////

  bool getEdgesForVertex(
      std::string const& id,
      std::vector<traverser::TraverserExpression*> const& expressions,
      TRI_edge_direction_e direction, SingleCollectionReadOnlyTransaction& trx,
      triagens::basics::Json& result, size_t& scannedIndex, size_t& filtered);
};
}
}

#endif
