////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "Actor/ActorPID.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Conductor/ExecutionStates/CreateWorkersState.h"

#include "Basics/ResultT.h"

#include <gtest/gtest.h>

using namespace arangodb::pregel;
using namespace arangodb::pregel::conductor;

static const std::string databaseName = "dontCare";

struct LookupInfoMock : conductor::CollectionLookup {
  LookupInfoMock(std::vector<std::string> serverIDs) : _servers(serverIDs) {}
  ~LookupInfoMock() = default;

  [[nodiscard]] auto getServerMapVertices() const -> ServerMapping override {
    ServerMapping mapping{};
    for (auto const& serverID : _servers) {
      mapping.emplace(serverID,
                      std::map<std::string, std::vector<std::string>>{});
    }
    return mapping;
  }

  // edges related methods:
  [[nodiscard]] auto getServerMapEdges() const -> ServerMapping override {
    ServerMapping mapping{};
    for (auto const& serverID : _servers) {
      mapping.emplace(serverID,
                      std::map<std::string, std::vector<std::string>>{});
    }
    return mapping;
  }

  // both combined (vertices, edges) related methods:
  [[nodiscard]] auto getAllShards() const -> ShardsMapping override {
    return {};
  }

  [[nodiscard]] auto getCollectionPlanIdMapAll() const
      -> CollectionPlanIDMapping override {
    return {};
  }

  std::vector<std::string> _servers;
};

struct AlgorithmFake : IAlgorithm {
  ~AlgorithmFake() = default;
  AlgorithmFake() = default;
  [[nodiscard]] auto masterContext(
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const -> MasterContext* override {
    return nullptr;
  }
  [[nodiscard]] auto masterContextUnique(
      uint64_t vertexCount, uint64_t edgeCount,
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const
      -> std::unique_ptr<MasterContext> override {
    return nullptr;
  }
  [[nodiscard]] auto workerContext(
      std::unique_ptr<AggregatorHandler> readAggregators,
      std::unique_ptr<AggregatorHandler> writeAggregators,
      arangodb::velocypack::Slice userParams) const -> WorkerContext* override {
    return nullptr;
  }
  [[nodiscard]] auto name() const -> std::string_view override {
    return "fake";
  };
};

TEST(ConductorStateTest,
     must_always_be_initialized_with_initial_execution_state) {
  auto fakeActorPID = actor::ActorPID{
      .server = "A", .database = "database", .id = actor::ActorID{4}};

  std::vector<std::string> emptyServers{};
  auto cState = ConductorState(
      std::make_unique<AlgorithmFake>(), ExecutionSpecifications(),
      std::make_unique<LookupInfoMock>(emptyServers), fakeActorPID,
      fakeActorPID, fakeActorPID, fakeActorPID);
  ASSERT_EQ(cState.executionState->name(), "initial");
}

TEST(CreateWorkersStateTest, creates_as_many_messages_as_required_servers) {
  auto fakeActorPID = actor::ActorPID{
      .server = "A", .database = "database", .id = actor::ActorID{4}};

  std::vector<std::vector<std::string>> amountOfServers = {
      {}, {"ServerA"}, {"ServerA", "ServerB"}};
  for (auto const& servers : amountOfServers) {
    auto cState = ConductorState(
        std::make_unique<AlgorithmFake>(), ExecutionSpecifications(),
        std::make_unique<LookupInfoMock>(servers), fakeActorPID, fakeActorPID,
        fakeActorPID, fakeActorPID);
    auto createWorkersState = CreateWorkers(cState);
    auto msgs = createWorkersState.messagesToServers();
    ASSERT_EQ(msgs.size(), servers.size());

    for (auto const& serverName : servers) {
      ASSERT_TRUE(msgs.contains(serverName));
    }
  }
}

TEST(CreateWorkersStateTest, creates_worker_pids_from_received_messages) {
  auto fakeActorPID = actor::ActorPID{
      .server = "A", .database = "database", .id = actor::ActorID{4}};

  std::vector<std::string> servers = {"ServerA", "ServerB", "ServerC"};
  auto cState = ConductorState(
      std::make_unique<AlgorithmFake>(), ExecutionSpecifications(),
      std::make_unique<LookupInfoMock>(servers), fakeActorPID, fakeActorPID,
      fakeActorPID, fakeActorPID);
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messagesToServers();

  auto created = arangodb::ResultT<conductor::message::WorkerCreated>();
  ASSERT_TRUE(created.ok());
  {
    createWorkers.receive(
        {.server = servers.at(0), .database = databaseName, .id = {0}},
        created);
    createWorkers.receive(
        {.server = servers.at(1), .database = databaseName, .id = {1}},
        created);
    createWorkers.receive(
        {.server = servers.at(2), .database = databaseName, .id = {2}},
        created);
  }

  ASSERT_EQ(cState.workers.size(), servers.size());
  // for (auto const& serverID : servers) {
  //  TODO check also if proper PIDs are inserted here
  //   enable as soon as _workers ar a std::set instead of a std::vector
  // }
}

TEST(CreateWorkersStateTest,
     reply_with_loading_state_as_soon_as_all_servers_replied) {
  auto fakeActorPID = actor::ActorPID{
      .server = "A", .database = "database", .id = actor::ActorID{4}};

  std::vector<std::string> servers = {"ServerA", "ServerB", "ServerC"};
  auto cState = ConductorState(
      std::make_unique<AlgorithmFake>(), ExecutionSpecifications(),
      std::make_unique<LookupInfoMock>(servers), fakeActorPID, fakeActorPID,
      fakeActorPID, fakeActorPID);
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messagesToServers();
  ASSERT_EQ(msgs.size(), servers.size());

  {
    actor::ActorPID actorPid{
        .server = servers.at(0), .database = databaseName, .id = {0}};
    auto receiveResponse =
        createWorkers.receive(actorPid, conductor::message::WorkerCreated{});
    ASSERT_EQ(receiveResponse, std::nullopt);
  }
  {
    actor::ActorPID actorPid{
        .server = servers.at(1), .database = databaseName, .id = {1}};
    auto receiveResponse =
        createWorkers.receive(actorPid, conductor::message::WorkerCreated{});
    ASSERT_EQ(receiveResponse, std::nullopt);
  }
  {
    actor::ActorPID actorPid{
        .server = servers.at(2), .database = databaseName, .id = {2}};
    auto receiveResponse =
        createWorkers.receive(actorPid, conductor::message::WorkerCreated{});
    ASSERT_TRUE(receiveResponse.has_value());
    ASSERT_EQ(receiveResponse.value().newState->name(), "loading");
  }
}

TEST(CreateWorkersStateTest, receive_invalid_message_type) {
  auto fakeActorPID = actor::ActorPID{
      .server = "A", .database = "database", .id = actor::ActorID{4}};

  std::vector<std::string> servers = {"ServerA"};
  auto cState = ConductorState(
      std::make_unique<AlgorithmFake>(), ExecutionSpecifications(),
      std::make_unique<LookupInfoMock>(servers), fakeActorPID, fakeActorPID,
      fakeActorPID, fakeActorPID);
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messagesToServers();

  {
    actor::ActorPID actorPid{
        .server = servers.at(0), .database = databaseName, .id = {0}};
    auto invalidMessage = conductor::message::ConductorStart{};
    auto receiveResponse = createWorkers.receive(actorPid, invalidMessage);
    ASSERT_EQ(receiveResponse.value().newState->name(), "fatal error");
  }
}

TEST(CreateWorkersStateTest, receive_valid_message_from_unknown_server) {
  auto fakeActorPID = actor::ActorPID{
      .server = "A", .database = "database", .id = actor::ActorID{4}};

  std::vector<std::string> servers = {"ServerA"};
  auto cState = ConductorState(
      std::make_unique<AlgorithmFake>(), ExecutionSpecifications(),
      std::make_unique<LookupInfoMock>(servers), fakeActorPID, fakeActorPID,
      fakeActorPID, fakeActorPID);
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messagesToServers();

  {
    actor::ActorPID unknownActorPid{
        .server = "UnknownServerX", .database = databaseName, .id = {0}};
    auto receiveResponse = createWorkers.receive(
        unknownActorPid, conductor::message::WorkerCreated{});
    ASSERT_EQ(receiveResponse.value().newState->name(), "fatal error");
  }
}

TEST(CreateWorkersStateTest, receive_valid_error_message) {
  auto fakeActorPID = actor::ActorPID{
      .server = "A", .database = "database", .id = actor::ActorID{4}};

  std::vector<std::string> servers = {"ServerA"};
  auto cState = ConductorState(
      std::make_unique<AlgorithmFake>(), ExecutionSpecifications(),
      std::make_unique<LookupInfoMock>(servers), fakeActorPID, fakeActorPID,
      fakeActorPID, fakeActorPID);
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messagesToServers();

  {
    actor::ActorPID unknownActorPid{
        .server = servers.at(0), .database = databaseName, .id = {0}};
    auto errorMessage = arangodb::ResultT<conductor::message::WorkerCreated>(
        TRI_ERROR_ARANGO_ILLEGAL_NAME);
    auto receiveResponse = createWorkers.receive(unknownActorPid, errorMessage);
    ASSERT_EQ(receiveResponse.value().newState->name(), "fatal error");
  }
}
