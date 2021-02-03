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

#include "gtest/gtest.h"

#include "Mocks/PreparedResponseConnectionPool.h"
#include "Mocks/Servers.h"

#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

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

void MockGraph::storeData(TRI_vocbase_t& vocbase, std::string const& vertexCollectionName,
                          std::string const& edgeCollectionName) const {
  {
    // Insert vertices
    arangodb::OperationOptions options;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              vertexCollectionName,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    size_t added = 0;
    velocypack::Builder b;
    for (auto& v : vertices()) {
      b.clear();
      v.addToBuilder(b);
      auto res = trx.insert(vertexCollectionName, b.slice(), options);
      EXPECT_TRUE((res.ok()));
      added++;
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(added == vertices().size());
  }

  {
    // Insert edges
    arangodb::OperationOptions options;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              edgeCollectionName,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));
    size_t added = 0;
    velocypack::Builder b;
    for (auto& edge : edges()) {
      b.clear();
      edge.addToBuilder(b);
      auto res = trx.insert(edgeCollectionName, b.slice(), options);
      if (res.fail()) {
        LOG_DEVEL << res.errorMessage() << " " << b.toJson();
      }
      EXPECT_TRUE((res.ok()));
      added++;
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(added == edges().size());
  }
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

  // NOTE: This only works on a single shard yet.
  storeData(server.getSystemDatabase(), _vertexShards[0].first, _edgeShards[0].first);
}

template <>
void MockGraph::prepareServer(MockCoordinator& server) const {
  std::string db = "_system";
  std::ignore = server.createCollection(db, getVertexCollectionName(),
                                        getVertexShardNameServerPairs(),
                                        TRI_COL_TYPE_DOCUMENT);

  std::ignore = server.createCollection(db, getEdgeCollectionName(),
                                        getEdgeShardNameServerPairs(), TRI_COL_TYPE_EDGE);
}

template <>
void MockGraph::simulateApi(MockDBServer const&,
                            std::vector<arangodb::tests::PreparedRequestResponse>& preparedResponses) const {
  // NOTE: We need the server input only for template magic.
  // Can be solved differently, but for a test i think this is sufficient.

  // TODO Implement me!
}