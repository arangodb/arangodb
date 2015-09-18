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

#include "vocbase.h"
#include "voc-types.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                       Graph Class
// -----------------------------------------------------------------------------

    class Graph {

// -----------------------------------------------------------------------------
// --SECTION--                                          constructor & destructor
// -----------------------------------------------------------------------------
      public:

        Graph () {
        }

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the cids of all vertexCollections
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<std::string> const vertexCollections ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the cids of all edgeCollections
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<std::string> const edgeCollections ();

////////////////////////////////////////////////////////////////////////////////
/// @brief Add an edge collection to this graphs definition
////////////////////////////////////////////////////////////////////////////////

        void addEdgeCollection (std::string);

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a vertex collection to this graphs definition
////////////////////////////////////////////////////////////////////////////////

        void addVertexCollection (std::string);

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                Factory for graphs
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief This factory is a singleton
////////////////////////////////////////////////////////////////////////////////
    class GraphFactory {


// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
      private: 

////////////////////////////////////////////////////////////////////////////////
/// @brief 2 level cache for graphs. level 1 selects database, level 2 selects graph
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<TRI_voc_tick_t,
          std::unordered_map<std::string, Graph>> _cache;

////////////////////////////////////////////////////////////////////////////////
/// @brief the instance
////////////////////////////////////////////////////////////////////////////////

        static GraphFactory* _singleton;

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph collection name
////////////////////////////////////////////////////////////////////////////////

        static const std::string _graphs;

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph collection edge definition attribute name
////////////////////////////////////////////////////////////////////////////////

        static char const* _attrEdgeDefs;

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph collection orphan list arribute name
////////////////////////////////////////////////////////////////////////////////

        static char const* _attrOrphans;

// -----------------------------------------------------------------------------
// --SECTION--                                             protected constructor
// -----------------------------------------------------------------------------
      protected:
          
        GraphFactory () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the factory instance
////////////////////////////////////////////////////////////////////////////////

        static GraphFactory* factory ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get an instance of Graph by Name. returns nullptr if graph is not existing
////////////////////////////////////////////////////////////////////////////////

        Graph const& byName (TRI_vocbase_t*, std::string);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate the cache for the given graph. Has to be called if the
///        Information of this graph has changed.
////////////////////////////////////////////////////////////////////////////////

        void invalidateCache (TRI_vocbase_t*, std::string);

    };

  } // namespace arango
} // namespace triagens

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
