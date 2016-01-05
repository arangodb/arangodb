////////////////////////////////////////////////////////////////////////////////
/// @brief Class for arangodb's graph features. Wrapper around the graph informations
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GRAPHS_H
#define ARANGODB_GRAPHS_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                       Graph Class
// -----------------------------------------------------------------------------

    class Graph {

// -----------------------------------------------------------------------------
// --SECTION--                                          constructor & destructor
// -----------------------------------------------------------------------------
      public:

        Graph (triagens::basics::Json j);

        ~Graph () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
      private: 

////////////////////////////////////////////////////////////////////////////////
/// @brief the cids of all vertexCollections
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<std::string> _vertexColls;

////////////////////////////////////////////////////////////////////////////////
/// @brief the cids of all edgeCollections
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<std::string> _edgeColls;

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph collection edge definition attribute name
////////////////////////////////////////////////////////////////////////////////

        static char const* _attrEdgeDefs;

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph collection orphan list arribute name
////////////////////////////////////////////////////////////////////////////////

        static char const* _attrOrphans;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph collection name
////////////////////////////////////////////////////////////////////////////////

        static const std::string _graphs;

////////////////////////////////////////////////////////////////////////////////
/// @brief Add Collections to the object
////////////////////////////////////////////////////////////////////////////////

        void insertVertexCollectionsFromJsonArray(triagens::basics::Json& arr);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the cids of all vertexCollections
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<std::string> const & vertexCollections () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the cids of all edgeCollections
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<std::string> const & edgeCollections () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief Add an edge collection to this graphs definition
////////////////////////////////////////////////////////////////////////////////

        void addEdgeCollection (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a vertex collection to this graphs definition
////////////////////////////////////////////////////////////////////////////////

        void addVertexCollection (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the graph
/// the caller is responsible for freeing the JSON later
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJson (TRI_memory_zone_t*,
                                       bool) const;

    };

  } // namespace aql
} // namespace triagens

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
