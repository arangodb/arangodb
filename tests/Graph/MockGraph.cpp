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
#include <Aql/QueryRegistry.h>
#include "IResearch/RestHandlerMock.h"
#include "InternalRestHandler/InternalRestTraverserHandler.h"

#include "gtest/gtest.h"

#include "Mocks/PreparedResponseConnectionPool.h"
#include "Mocks/Servers.h"

#include "Aql/RestAqlHandler.h"
#include "Network/NetworkFeature.h"
#include "Transaction/Helpers.h"
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
// Future: Also engineID's need to be returned here.
std::vector<arangodb::tests::PreparedRequestResponse> MockGraph::simulateApi(
    MockDBServer& server, std::unordered_set<VertexDef, hashVertexDef> verticesList,
    arangodb::graph::BaseOptions& opts, aql::ExecutionPlan* queryPlan, aql::AstNode* condition) const {
  // NOTE: We need the server input only for template magic.
  // Can be solved differently, but for a test i think this is sufficient.
  std::vector<arangodb::tests::PreparedRequestResponse> preparedResponses{};

  aql::QueryRegistry queryRegistry{120};

  {
    // init restaqlhandler
    arangodb::tests::PreparedRequestResponse prep{server.getSystemDatabase()};
    // generate and add body here
    VPackBuilder builder;
    builder.openObject();
    builder.add("lockInfo", VPackValue(VPackValueType::Object));

    builder.add("read", VPackValue(VPackValueType::Array));
    // append here the collection names (?) <-- TODO: Check RestAqlHandler.cpp:230
    // builder.add(VPackValue(_vertexCollectionName));
    // builder.add(VPackValue(_edgeCollectionName));
    // appending collection shard ids
    for (auto const& vShard : _vertexShards) {
      builder.add(VPackValue(vShard.first));
    }
    for (auto const& eShard : _edgeShards) {
      builder.add(VPackValue(eShard.first));
    }
    builder.close(); // array READ
    builder.close(); // object lockInfo

    builder.add("options", VPackValue(VPackValueType::Object));
    builder.add("ttl", VPackValue(120));
    builder.close();  // object options

    builder.add("snippets", VPackValue(VPackValueType::Object));
    builder.close();  // object snippets

    builder.add("variables", VPackValue(VPackValueType::Array));
    builder.close();  // object variables

    // TODO: traverserEngines need to be available and generated
    /*
     * Testing Start
     */
    builder.add("traverserEngines", VPackValue(VPackValueType::Array));

    builder.openObject(); // main container

    builder.add(VPackValue("options"));

    // create collectionToShardMap
    std::map<std::string, std::string> collectionToShard {};
    for (auto const& vertexTuple: getVertexShardNameServerPairs()) {
      collectionToShard[vertexTuple.first] = vertexTuple.second;
    }
    for (auto const& edgeTuple: getEdgeShardNameServerPairs()) {
      collectionToShard[edgeTuple.first] = edgeTuple.second;
    }
    LOG_DEVEL << "Collection list: " << collectionToShard.size();
    LOG_DEVEL << "TODO: Info for Michael -> this was my last attempt to fix the creation of baseLookupInfos"
                 << "as I've seen this correctly we need to set setCollectionToShard() and then addLookupInfo."
                 << "addLookupInfo needs a ";
    opts.setCollectionToShard(collectionToShard);
    opts.addLookupInfo(queryPlan, "v", "attributeName", condition, false);
    opts.addLookupInfo(queryPlan, "e", "attributeName", condition, false);

    opts.buildEngineInfo(builder);

    builder.add(VPackValue("shards"));
    builder.openObject();

    builder.add(VPackValue("vertices"));
    builder.openObject();

    for (auto const& vertexTuple: getVertexShardNameServerPairs()) {
      builder.add(_vertexCollectionName, VPackValue(VPackValueType::Array));
      builder.add(VPackValue(vertexTuple.first)); // shardID
      builder.close(); // inner array
    }

    builder.close(); // vertices

    builder.add(VPackValue("edges"));
    builder.openArray();
    for (auto const& edgeTuple: getEdgeShardNameServerPairs()) {
      builder.openArray();
      builder.add(VPackValue(edgeTuple.first)); // shardID
      builder.close(); // inner array
    }
    builder.close(); // edges
    builder.close(); // shards
    builder.close(); // main container
    builder.close(); // array traverserEngines
    /*
     * Testing End
     */

    builder.close();  // object (outer)
    LOG_DEVEL << builder.toJson();

    prep.addBody(builder.slice());
    prep.addSuffix("setup");

    prep.setRequestType(arangodb::rest::RequestType::POST);
    auto fakeRequest = prep.generateRequest();
    auto fakeResponse = std::make_unique<GeneralResponseMock>();
    arangodb::aql::RestAqlHandler aqlHandler{server.server(), fakeRequest.release(),
                                             fakeResponse.release(), &queryRegistry};

    aqlHandler.execute();
    auto response = aqlHandler.stealResponse();  // Read: (EngineId eid)
    LOG_DEVEL << static_cast<GeneralResponseMock*>(response.get())->_payload.toJson();
    // TODO: read resposnse (eid)
  }

  // merge given vertices with available graph vertices
  verticesList.insert(vertices().begin(), vertices().end());

  for (auto const& vertex : verticesList) {
    LOG_DEVEL << "  -> " << vertex._id;

    // 1.) fetch the vertex itself
    arangodb::tests::PreparedRequestResponse prep{server.getSystemDatabase()};
    auto fakeResponse = std::make_unique<GeneralResponseMock>();

    /*
     *  Export to external method later (Create network request including options)
     */
    VPackBuilder leased;
    leased.openObject();
    leased.add("keys", VPackValue(VPackValueType::Array));
    leased.add(VPackValue(vertex._id));
    leased.close();  // 'keys' Array
    leased.close();  // base object

    // Request muss ins array: preparedResponses
    prep.setRequestType(arangodb::rest::RequestType::PUT);
    prep.addSuffix("vertex");
    prep.addSuffix(basics::StringUtils::itoa(1));  // check test - Write: (EngineId eid) <- vermutlich
    prep.addBody(leased.slice());

    auto fakeRequest = prep.generateRequest();
    InternalRestTraverserHandler testee{server.server(), fakeRequest.release(),
                                        fakeResponse.release(), &queryRegistry};

    std::ignore = testee.execute(); // todo check if needed
    auto res = testee.stealResponse();
    prep.rememberResponse(std::move(res));
    preparedResponses.emplace_back(std::move(prep));

    // 2.) fetch all connected edges requests
  }

  // Try without first, if not working:
  // 3.) Send a Delete to RestAqlHandler (? => End query execution)

  return preparedResponses;
}