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

#include "Basics/JsonHelper.h"
#include "Aql/Graphs.h"
using namespace triagens::basics;
using namespace triagens::aql;

char const* Graph::_attrEdgeDefs = "edgeDefinitions";
char const* Graph::_attrOrphans = "orphanCollections";

// -----------------------------------------------------------------------------
// --SECTION--                                                       Local utils
// -----------------------------------------------------------------------------

void Graph::insertVertexCollectionsFromJsonArray(triagens::basics::Json& arr) {
  for (size_t j = 0; j < arr.size(); ++j) {
    Json c = arr.at(j);
    TRI_ASSERT(c.isString());
    std::string name = JsonHelper::getStringValue(c.json(), "");
    addVertexCollection(name);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       Graph Class
// -----------------------------------------------------------------------------

std::unordered_set<std::string> const& Graph::vertexCollections () const {
  return _vertexColls;
}

std::unordered_set<std::string> const& Graph::edgeCollections () const {
  return _edgeColls;
}

void Graph::addEdgeCollection (std::string name) {
  _edgeColls.insert(name);
}

void Graph::addVertexCollection (std::string name) {
  _vertexColls.insert(name);
}

triagens::basics::Json Graph::toJson (TRI_memory_zone_t* z,
                                      bool verbose) const {
  triagens::basics::Json json(z, triagens::basics::Json::Object);

  if (_vertexColls.size() > 0) {
    triagens::basics::Json vcn(z, triagens::basics::Json::Array);
    for (auto cn : _vertexColls) {
      vcn.add(triagens::basics::Json(cn));
    }
    json("vertexCollectionNames", vcn);
  }

  if (_edgeColls.size() > 0) {
    triagens::basics::Json ecn(z, triagens::basics::Json::Array);
    for (auto cn : _edgeColls) {
      ecn.add(triagens::basics::Json(cn));
    }
    json("edgeCollectionNames", ecn);
  }

  return json;
}

Graph::Graph(triagens::basics::Json j) : 
  _vertexColls(),
  _edgeColls()
{
  auto jsonDef = j.get(_attrEdgeDefs);

  for (size_t i = 0; i < jsonDef.size(); ++i) {
    Json def = jsonDef.at(i);
    TRI_ASSERT(def.isObject());
    Json e = def.get("collection");
    TRI_ASSERT(e.isString());
    std::string eCol = JsonHelper::getStringValue(e.json(), "");
    addEdgeCollection(eCol);
    e = def.get("from");
    TRI_ASSERT(e.isArray());
    insertVertexCollectionsFromJsonArray(e);
    e = def.get("to");
    TRI_ASSERT(e.isArray());
    insertVertexCollectionsFromJsonArray(e);
  }
  auto orphans = j.get(_attrOrphans);
  insertVertexCollectionsFromJsonArray(orphans);
}

