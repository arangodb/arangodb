////////////////////////////////////////////////////////////////////////////////
/// @brief V8 Traverser
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

#ifndef ARANGODB_V8_TRAVERSER_H
#define ARANGODB_V8_TRAVERSER_H 1

#include "Basics/Common.h"
#include "Traverser.h"

namespace triagens {
  namespace basics {
    namespace traverser {
      struct ShortestPathOptions {
        std::string direction;
        bool useWeight;
        std::string weightAttribute;
        double defaultWeight;
        bool bidirectional;
        bool multiThreaded;

        ShortestPathOptions() :
          direction("outbound"),
          useWeight(false),
          weightAttribute(""),
          defaultWeight(1),
          bidirectional(true),
          multiThreaded(true) {
        }

      };
      struct NeighborsOptions {
        std::string direction;
        bool distinct;

        NeighborsOptions() :
          direction("outbound"),
          distinct(false) {
        }

      };
 
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Template for a vertex id. Is simply a pair of cid and key
////////////////////////////////////////////////////////////////////////////////

struct VertexId {
  TRI_voc_cid_t cid;
  char const*   key;

  VertexId() : cid(0), key(nullptr) {
  }

  VertexId( TRI_voc_cid_t cid, char const* key) 
    : cid(cid), key(key) {
  }

  // Find unnecessary copies
  //   VertexId(const VertexId&) = delete;
  // VertexId(const VertexId& v) : first(v.first), second(v.second) { std::cout << "move failed!\n";}
  // VertexId(VertexId&& v) : first(v.first), second(std::move(v.second)) {}


};


////////////////////////////////////////////////////////////////////////////////
/// @brief typedef the template instanciation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef triagens::basics::PathFinder<VertexId, std::string, double> 
        ArangoDBPathFinder;




////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch (
  std::string const& vertexCollectionName,
  std::string const& edgeCollectionName,
  std::string const& startVertex,
  std::string const& targetVertex,
  triagens::arango::CollectionNameResolver const* resolver,
  TRI_document_collection_t* ecol,
  triagens::basics::traverser::ShortestPathOptions& opts
);

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

std::vector<VertexId> TRI_RunNeighborsSearch (
  v8::Isolate* isolate,
  TRI_vocbase_t* vocbase,
  std::string const& vertexCollectionName,
  std::string const& edgeCollectionName,
  std::string const& startVertex,
  triagens::arango::CollectionNameResolver const* resolver,
  TRI_document_collection_t* ecol,
  triagens::basics::traverser::NeighborsOptions& opts
);

#endif
