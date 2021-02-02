////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

#include "MockGraph.h"

#include "Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;
using namespace arangodb::tests::mocks;

void MockGraph::EdgeDef::addToBuilder(arangodb::velocypack::Builder& builder) const {
  std::string fromId = _from;
  std::string toId = _to;
  std::string keyId = _from.substr(2) + "-" + _to.substr(2);

  builder.openObject();
  builder.add(StaticStrings::IdString, VPackValue(_eCol + "/" + keyId));
  builder.add(StaticStrings::KeyString, VPackValue(keyId));
  builder.add(StaticStrings::FromString, VPackValue(fromId));
  builder.add(StaticStrings::ToString, VPackValue(toId));
  builder.add("weight", VPackValue(_weight));
  builder.close();
}

void MockGraph::VertexDef::addToBuilder(arangodb::velocypack::Builder& builder) const {
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_id.substr(2)));
  builder.add(StaticStrings::IdString, VPackValue(_id));
  builder.close();
}

void MockGraph::addEdge(std::string from, std::string to, double weight) {
  _edges.emplace_back(EdgeDef{from, to, weight, _edgeCollectionName});
  _vertices.emplace(std::move(from));
  _vertices.emplace(std::move(to));
}

void MockGraph::addEdge(size_t from, size_t to, double weight) {
  addEdge(getVertexCollectionName() + "/" + basics::StringUtils::itoa(from),
          getVertexCollectionName() + "/" + basics::StringUtils::itoa(to), weight);
}

template <>
void MockGraph::prepareServer(MockDBServer& server) const {
  std::string db = "_system";
  auto vCol = server.createCollection(db, getVertexCollectionName(),
                                      getVertexShardNameServerPairs(), TRI_COL_TYPE_DOCUMENT);
  for (auto const& [shard, servName] : _vertexShards) {
    server.createShard(db, shard, *vCol);
  }
  auto eCol = server.createCollection(db, getEdgeCollectionName(),
                                      getEdgeShardNameServerPairs(), TRI_COL_TYPE_EDGE);

  for (auto const& [shard, servName] : _edgeShards) {
    server.createShard(db, shard, *eCol);
  }
}