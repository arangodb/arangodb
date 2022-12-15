/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <variant>
#include "Basics/ResultT.h"
#include "VocBase/vocbase.h"
#include "velocypack/Builder.h"
#include "Properties.h"

namespace arangodb::pregel::collections::graph {

struct GraphOrCollection : std::variant<GraphCollectionNames, GraphName> {};
template<class Inspector>
auto inspect(Inspector& f, GraphOrCollection& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<GraphCollectionNames>("collectionNames"),
      arangodb::inspection::type<GraphName>("graphName"));
}

/**
   The source of a graph includes the name of its collections and the
restrictions on edge collections.
It can be created either via a graph name or the vertex and edge collection
names. If a graph is given, the graph edges are automatically added
to the edge collection restrictions.
 **/
struct GraphSource {
  GraphSource(GraphOrCollection graphOrCollections,
              EdgeCollectionRestrictions restrictions)
      : graphOrCollections{std::move(graphOrCollections)},
        edgeCollectionRestrictions{std::move(restrictions)} {}
  auto collectionNames(TRI_vocbase_t& vocbase) -> ResultT<GraphCollectionNames>;
  auto restrictions(TRI_vocbase_t& vocbase)
      -> ResultT<EdgeCollectionRestrictions>;
  template<typename Inspector>
  friend auto inspect(Inspector& f, GraphSource& x);

 private:
  GraphOrCollection graphOrCollections;
  EdgeCollectionRestrictions edgeCollectionRestrictions;
  auto graphRestrictions(TRI_vocbase_t& vocbase)
      -> ResultT<EdgeCollectionRestrictions>;
};
template<typename Inspector>
auto inspect(Inspector& f, GraphSource& x) {
  return f.object(x).fields(
      f.field("graphOrCollection", x.graphOrCollections),
      f.field("edgeCollectionRestrictions", x.edgeCollectionRestrictions));
}

}  // namespace arangodb::pregel::collections::graph
