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

#include "Graphs.h"
#include "Utils/transactions.h"
#include "Basics/JsonHelper.h"

using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                       Local utils
// -----------------------------------------------------------------------------

static void insertVertexCollectionsFromJsonArray(Graph& g, Json& arr) {
  for (size_t j = 0; j < arr.size(); ++j) {
    Json c = arr.at(j);
    TRI_ASSERT(c.isString());
    std::string name = JsonHelper::getStringValue(c.json(), "");
    g.addVertexCollection(name);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       Graph Class
// -----------------------------------------------------------------------------

std::unordered_set<std::string> const Graph::vertexCollections () {
  return _vertexColls;
}

std::unordered_set<std::string> const Graph::edgeCollections () {
  return _edgeColls;
}

void Graph::addEdgeCollection (std::string name) {
  _edgeColls.insert(name);
}

void Graph::addVertexCollection (std::string name) {
  _vertexColls.insert(name);
}
// -----------------------------------------------------------------------------
// --SECTION--                                                      GraphFactory
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Instanciation of static class members
////////////////////////////////////////////////////////////////////////////////

GraphFactory* GraphFactory::_singleton = nullptr;
const std::string GraphFactory::_graphs = "_graphs";
char const* GraphFactory::_attrEdgeDefs = "edgeDefinitions";
char const* GraphFactory::_attrOrphans = "orphanCollections";

GraphFactory* GraphFactory::factory () {
  if (_singleton == nullptr) {
    _singleton = new GraphFactory();
  }
  return _singleton;
}

Graph const& GraphFactory::byName (TRI_vocbase_t* vocbase, std::string name) {
  auto outit = _cache.find(vocbase->_id);
  if (outit == _cache.end()) {
    // database not yet found.
    std::unordered_map<std::string, Graph> map;
    _cache.emplace(vocbase->_id, map);
    outit = _cache.find(vocbase->_id);
  }
  auto it = outit->second.find(name);
  if (it == outit->second.end()) {
    CollectionNameResolver resolver(vocbase);
    SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(), vocbase, resolver.getCollectionId(_graphs));
    int res = trx.begin();
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    TRI_doc_mptr_copy_t mptr;
    res = trx.read(&mptr, name);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    auto shaper = trx.trxCollection()->_collection->_collection->getShaper();
    auto pid = shaper->lookupAttributePathByName(_attrEdgeDefs);
    if (pid == 0) {
      //TODO FIXME
      THROW_ARANGO_EXCEPTION(42);
    }
    TRI_shaped_json_t document;
    TRI_EXTRACT_SHAPED_JSON_MARKER(document, mptr.getDataPtr());
    TRI_shaped_json_t result;
    TRI_shape_t const* shape;
    bool ok = shaper->extractShapedJson(&document,
                                        0,
                                        pid,
                                        &result,
                                        &shape);

    if (! ok) {
      //TODO FIXME
      THROW_ARANGO_EXCEPTION(42);
    }
    Graph g;
    Json jsonDef(TRI_UNKNOWN_MEM_ZONE, TRI_JsonShapedJson(shaper, &result));
    for (size_t i = 0; i < jsonDef.size(); ++i) {
      Json def = jsonDef.at(i);
      TRI_ASSERT(def.isObject());
      Json e = def.get("collection");
      TRI_ASSERT(e.isString());
      std::string eCol = JsonHelper::getStringValue(e.json(), "");
      g.addEdgeCollection(eCol);
      e = def.get("from");
      TRI_ASSERT(e.isArray());
      insertVertexCollectionsFromJsonArray(g, e);
      e = def.get("to");
      TRI_ASSERT(e.isArray());
      insertVertexCollectionsFromJsonArray(g, e);
    }
    pid = shaper->lookupAttributePathByName(_attrOrphans);
    if (pid == 0) {
      //TODO FIXME
      THROW_ARANGO_EXCEPTION(42);
    }
    TRI_EXTRACT_SHAPED_JSON_MARKER(document, mptr.getDataPtr());
    ok = shaper->extractShapedJson(&document,
                                   0,
                                   pid,
                                   &result,
                                   &shape);
    if (! ok) {
      //TODO FIXME
      THROW_ARANGO_EXCEPTION(42);
    }
    Json orphans(TRI_UNKNOWN_MEM_ZONE, TRI_JsonShapedJson(shaper, &result));
    insertVertexCollectionsFromJsonArray(g, orphans);
    trx.finish(res);
    outit->second.emplace(name, g);
    it = outit->second.find(name);
  }
  return it->second;
}

void GraphFactory::invalidateCache (TRI_vocbase_t* vocbase, std::string name) {
  auto outit = _cache.find(vocbase->_id);
  if (outit != _cache.end()) {
    outit->second.erase(name);
  }
  // No else here, if the database has not yet been cached the graph cannot be
}

