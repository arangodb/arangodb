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

#include "Graphs.h"
#include "Basics/JsonHelper.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::aql;

char const* Graph::_attrEdgeDefs = "edgeDefinitions";
char const* Graph::_attrOrphans = "orphanCollections";

void Graph::insertVertexCollectionsFromJsonArray(arangodb::basics::Json& arr) {
  for (size_t j = 0; j < arr.size(); ++j) {
    Json c = arr.at(j);
    TRI_ASSERT(c.isString());
    std::string name = JsonHelper::getStringValue(c.json(), "");
    addVertexCollection(name);
  }
}

std::unordered_set<std::string> const& Graph::vertexCollections() const {
  return _vertexColls;
}

std::unordered_set<std::string> const& Graph::edgeCollections() const {
  return _edgeColls;
}

void Graph::addEdgeCollection(std::string const& name) {
  _edgeColls.insert(name);
}

void Graph::addVertexCollection(std::string const& name) {
  _vertexColls.insert(name);
}

arangodb::basics::Json Graph::toJson(TRI_memory_zone_t* z, bool verbose) const {
  arangodb::basics::Json json(z, arangodb::basics::Json::Object);

  if (!_vertexColls.empty()) {
    arangodb::basics::Json vcn(z, arangodb::basics::Json::Array,
                               _vertexColls.size());
    for (auto const& cn : _vertexColls) {
      vcn.add(arangodb::basics::Json(cn));
    }
    json("vertexCollectionNames", vcn);
  }

  if (!_edgeColls.empty()) {
    arangodb::basics::Json ecn(z, arangodb::basics::Json::Array,
                               _edgeColls.size());
    for (auto const& cn : _edgeColls) {
      ecn.add(arangodb::basics::Json(cn));
    }
    json("edgeCollectionNames", ecn);
  }

  return json;
}

void Graph::toVelocyPack(VPackBuilder& builder, bool verbose) const {
  VPackObjectBuilder guard(&builder);

  if (!_vertexColls.empty()) {
    builder.add(VPackValue("vertexCollectionNames"));
    VPackArrayBuilder guard2(&builder);
    for (auto const& cn : _vertexColls) {
      builder.add(VPackValue(cn));
    }
  }

  if (!_edgeColls.empty()) {
    builder.add(VPackValue("edgeCollectionNames"));
    VPackArrayBuilder guard2(&builder);
    for (auto const& cn : _edgeColls) {
      builder.add(VPackValue(cn));
    }
  }
}

Graph::Graph(arangodb::basics::Json const& j) : _vertexColls(), _edgeColls() {
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
