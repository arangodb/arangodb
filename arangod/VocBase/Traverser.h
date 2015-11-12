////////////////////////////////////////////////////////////////////////////////
/// @brief Traverser
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOCBASE_TRAVERSER_H
#define ARANGODB_VOCBASE_TRAVERSER_H 1

#include "basics/Traverser.h"

namespace triagens {
  namespace arango {
    namespace traverser {


// -----------------------------------------------------------------------------
// --SECTION--                                                   struct VertexId
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Template for a vertex id. Is simply a pair of cid and key
/// NOTE: This struct will never free the value asigned to char const* key
///       The environment has to make sure that the string it points to is
///       not freed as long as this struct is in use!
////////////////////////////////////////////////////////////////////////////////

    struct VertexId {
      TRI_voc_cid_t cid;
      char const* key;

      VertexId () 
        : cid(0), 
          key("") {
      }

      VertexId (TRI_voc_cid_t cid, char const* key) 
        : cid(cid),
          key(key) {
      }

      bool operator== (const VertexId& other) const {
        if (cid == other.cid) {
          return strcmp(key, other.key) == 0;
        }
        return false;
      }

    };

    // EdgeId and VertexId are similar here. both have a key and a cid
    typedef VertexId EdgeId; 

// -----------------------------------------------------------------------------
// --SECTION--                                               class TraversalPath
// -----------------------------------------------------------------------------

    class TraversalPath {

        public:

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor. This is an abstract only class.
////////////////////////////////////////////////////////////////////////////////

          TraversalPath () {
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief Builds the complete path as Json
///        Has the format:
///        {
///           vertices: [<vertex-as-json>],
///           edges: [<edge-as-json>]
///        }
////////////////////////////////////////////////////////////////////////////////

          virtual triagens::basics::Json* pathToJson (Transaction*,
                                                      CollectionNameResolver*) const = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Builds only the last edge on the path as Json
////////////////////////////////////////////////////////////////////////////////

          virtual triagens::basics::Json* lastEdgeToJson (Transaction*,
                                                          CollectionNameResolver*) const = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Builds only the last vertex as Json
////////////////////////////////////////////////////////////////////////////////

          virtual triagens::basics::Json* lastVertexToJson (Transaction*,
                                                            CollectionNameResolver*) const = 0;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                   class Traverser
// -----------------------------------------------------------------------------

      class Traverser {
        
        public:

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor. This is an abstract only class.
////////////////////////////////////////////////////////////////////////////////

          Traverser () {
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief Destructor
////////////////////////////////////////////////////////////////////////////////

          ~Traverser () {
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief Reset the traverser to use another start vertex
////////////////////////////////////////////////////////////////////////////////

          virtual void setStartVertex (VertexId& v) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Skip amount many paths of the graph.
////////////////////////////////////////////////////////////////////////////////

          virtual size_t skip (size_t amount) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the next possible path in the graph.
////////////////////////////////////////////////////////////////////////////////

          virtual const TraversalPath* next () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Prune the current path prefix. Do not evaluate it any further.
////////////////////////////////////////////////////////////////////////////////

          virtual void prune () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Simple check if there potentially more paths.
///        It might return true although there are no more paths available.
///        If it returns false it is guaranteed that there are no more paths.
////////////////////////////////////////////////////////////////////////////////

          virtual bool hasMore () = 0;

      };

    } // traverser
  } // arango
} // triagens

// -----------------------------------------------------------------------------
// --SECTION--                                     functions for std::containers
// -----------------------------------------------------------------------------

namespace std {
  template<>
  struct hash<triagens::arango::traverser::VertexId> {
    public:
      size_t operator() (triagens::arango::traverser::VertexId const& s) const {
        size_t h1 = std::hash<TRI_voc_cid_t>()(s.cid);
        size_t h2 = TRI_FnvHashString(s.key);
        return h1 ^ ( h2 << 1 );
      }
  };

  template<>
  struct equal_to<triagens::arango::traverser::VertexId> {
    public:
      bool operator() (triagens::arango::traverser::VertexId const& s, triagens::arango::traverser::VertexId const& t) const {
        return s.cid == t.cid && strcmp(s.key, t.key) == 0;
      }
  };

  template<>
    struct less<triagens::arango::traverser::VertexId> {
      public:
        bool operator() (const triagens::arango::traverser::VertexId& lhs, const triagens::arango::traverser::VertexId& rhs) {
          if (lhs.cid != rhs.cid) {
            return lhs.cid < rhs.cid;
          }
          return strcmp(lhs.key, rhs.key) < 0;
        }
    };
}

#endif
