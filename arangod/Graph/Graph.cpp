////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Aql/AstNode.h"
#include "Aql/Graphs.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"

using namespace arangodb;
using namespace arangodb::graph;

char const* Graph::_attrEdgeDefs = "edgeDefinitions";
char const* Graph::_attrOrphans = "orphanCollections";

void Graph::insertVertexCollections(VPackSlice& arr) {
  TRI_ASSERT(arr.isArray());
  for (auto const& c : VPackArrayIterator(arr)) {
    TRI_ASSERT(c.isString());
    addVertexCollection(c.copyString());
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

Graph::Graph(std::string&& graphName_, velocypack::Slice const& slice)
    : _graphName(graphName_), _vertexColls(), _edgeColls() {
  if (slice.hasKey(_attrEdgeDefs)) {
    VPackSlice edgeDefs = slice.get(_attrEdgeDefs);
    TRI_ASSERT(edgeDefs.isArray());
    if (!edgeDefs.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_GRAPH_INVALID_GRAPH,
          "'edgeDefinitions' are not an array in the graph definition");
    }

    for (auto const& def : VPackArrayIterator(edgeDefs)) {
      Result res = addEdgeDefinition(def);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      // TODO maybe the following code can be simplified using _edgeDefs
      // and/or after def is already validated.
      // Or, this can be done during addEdgeDefinition as well.
      TRI_ASSERT(def.isObject());
      try {
        std::string eCol =
            basics::VelocyPackHelper::getStringValue(def, "collection", "");
        addEdgeCollection(eCol);
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find 'collection' in the graph definition");
      }
      // TODO what if graph is not in a valid format any more
      try {
        VPackSlice tmp = def.get("from");
        insertVertexCollections(tmp);
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find from-collection in the graph definition");
      }
      try {
        VPackSlice tmp = def.get("to");
        insertVertexCollections(tmp);
      } catch (...) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_GRAPH_INVALID_GRAPH,
            "didn't find to-collection in the graph definition");
      }
    }
  }
  if (slice.hasKey(_attrOrphans)) {
    auto orphans = slice.get(_attrOrphans);
    insertVertexCollections(orphans);
  }
}

void Graph::enhanceEngineInfo(VPackBuilder&) const {}

// validates the type:
// edgeDefinition : { collection : string, from : [string], to : [string] }
Result Graph::validateEdgeDefinition(VPackSlice const& edgeDefinition) {
  TRI_ASSERT(edgeDefinition.isObject());
  if (!edgeDefinition.isObject()) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                  "edge definition is not an object!");
  }

  for (auto const& key :
       std::array<std::string, 3>{"collection", "from", "to"}) {
    if (!edgeDefinition.hasKey(key)) {
      return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                    "Attribute '" + key + "' missing in edge definition!");
    }
  }

  if (!edgeDefinition.get("collection").isString()) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                  "edge definition is not a string!");
  }

  for (auto const& key : std::array<std::string, 2>{"from", "to"}) {
    if (!edgeDefinition.get(key).isArray()) {
      return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                    "Edge definition '" + key + "' is not an array!");
    }

    for (auto const& it : VPackArrayIterator(edgeDefinition.get(key))) {
      if (!it.isString()) {
        return Result(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,
                      std::string("Edge definition '") + key +
                          "' does not only contain strings!");
      }
    }
  }

  return Result();
}

Result Graph::addEdgeDefinition(VPackSlice const& edgeDefinition) {
  Result res = validateEdgeDefinition(edgeDefinition);
  // should always be valid unless someone broke his _graphs collection
  // manually
  TRI_ASSERT(res.ok());
  if (res.fail()) {
    return res;
  }
  std::string collection = edgeDefinition.get("collection").copyString();
  VPackSlice from = edgeDefinition.get("from");
  VPackSlice to = edgeDefinition.get("to");

  std::unordered_set<std::string> fromSet;
  std::unordered_set<std::string> toSet;

  // duplicates in from and to shouldn't occur, but are safely ignored here
  for (auto const& it : VPackArrayIterator(from)) {
    fromSet.emplace(it.copyString());
  }
  for (auto const& it : VPackArrayIterator(to)) {
    toSet.emplace(it.copyString());
  }

  bool inserted;
  std::tie(std::ignore, inserted) = _edgeDefs.emplace(
      collection,
      EdgeDefinition{collection, std::move(fromSet), std::move(toSet)});

  // This can only happen if addEdgeDefinition is called without clearing
  // _edgeDefs first (which would be a logical error), or if the same collection
  // is defined multiple times (which the user should not be allowed to do).
  TRI_ASSERT(inserted);
  if (!inserted) {
    return Result(TRI_ERROR_GRAPH_INTERNAL_EDGE_COLLECTION_ALREADY_SET,
                  "Collection '" + collection + "' already added!");
  }

  return Result();
}

std::ostream& Graph::operator<<(std::ostream& ostream) {
  ostream << "Graph \"" << name() << "\" {\n";
  for (auto const& it : _edgeDefs) {
    EdgeDefinition const& def = it.second;
    ostream << "  collection \"" << def.getName() << "\" {\n";
    ostream << "    from [";
    bool first = true;
    for (auto const& from : def.getFrom()) {
      if (!first) {
        ostream << ", ";
      }
      first = false;
      ostream << from;
    }
    ostream << "    to [";
    first = true;
    for (auto const& to : def.getTo()) {
      if (!first) {
        ostream << ", ";
      }
      first = false;
      ostream << to;
    }
    ostream << "  }\n";
  }
  ostream << "}";

  return ostream;
}

ResultT<std::pair<OperationResult, Result>> GraphOperations::getVertex(
    std::string const& collectionName, std::string const& key,
    boost::optional<TRI_voc_rid_t> rev) {
  OperationOptions options;
  options.ignoreRevs = true;

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(StaticStrings::KeyString, VPackValue(key));
    if (rev) {
      options.ignoreRevs = false;
      builder.add(StaticStrings::RevString,
                  VPackValue(TRI_RidToString(rev.get())));
    }
  }
  VPackSlice search = builder.slice();

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(ctx(), collectionName,
                                  AccessMode::Type::READ);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    return res;
  }

  OperationResult result = trx.document(collectionName, search, options);

  res = trx.finish(result.result);

  return std::make_pair(std::move(result), std::move(res));
}
