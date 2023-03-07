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

#include "Pregel/Conductor/State.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/Conductor/ExecutionStates/CreateWorkers.h"

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

TEST(ConductorStateTest,
     must_always_be_initialized_with_initial_execution_state) {
  std::vector<std::string> emptyServers{};
  auto cState = ConductorState(ExecutionSpecifications(),
                               std::make_unique<LookupInfoMock>(emptyServers));
  ASSERT_EQ(cState._executionState->name(), "initial");
}

TEST(CreateWorkersStateTest, creates_as_many_messages_as_required_servers) {
  std::vector<std::vector<std::string>> amountOfServers = {
      {}, {"ServerA"}, {"ServerA", "ServerB"}};

  for (auto const& servers : amountOfServers) {
    auto cState = ConductorState(ExecutionSpecifications(),
                                 std::make_unique<LookupInfoMock>(servers));
    auto createWorkersState = CreateWorkers(cState);
    auto msgs = createWorkersState.messages();
    ASSERT_EQ(msgs.size(), servers.size());

    for (auto const& serverName : servers) {
      ASSERT_TRUE(msgs.contains(serverName));
    }
  }
}

TEST(CreateWorkersStateTest, creates_worker_pids_from_received_messages) {
  std::vector<std::string> servers = {"ServerA", "ServerB", "ServerC"};

  auto cState = ConductorState(ExecutionSpecifications(),
                               std::make_unique<LookupInfoMock>(servers));
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messages();

  auto created = arangodb::ResultT<message::WorkerCreated>();
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

  ASSERT_EQ(cState._workers.size(), servers.size());
  // for (auto const& serverID : servers) {
  //  TODO check also if proper PIDs are inserted here
  //   enable as soon as _workers ar a std::set instead of a std::vector
  // }
}

TEST(CreateWorkersStateTest,
     reply_with_loading_state_as_soon_as_all_servers_replied) {
  std::vector<std::string> servers = {"ServerA", "ServerB", "ServerC"};
  auto cState = ConductorState(ExecutionSpecifications(),
                               std::make_unique<LookupInfoMock>(servers));
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messages();
  ASSERT_EQ(msgs.size(), servers.size());

  {
    actor::ActorPID actorPid{
        .server = servers.at(0), .database = databaseName, .id = {0}};
    auto receiveResponse =
        createWorkers.receive(actorPid, message::WorkerCreated{});
    ASSERT_EQ(receiveResponse, std::nullopt);
  }
  {
    actor::ActorPID actorPid{
        .server = servers.at(1), .database = databaseName, .id = {1}};
    auto receiveResponse =
        createWorkers.receive(actorPid, message::WorkerCreated{});
    ASSERT_EQ(receiveResponse, std::nullopt);
  }
  {
    actor::ActorPID actorPid{
        .server = servers.at(2), .database = databaseName, .id = {2}};
    auto receiveResponse =
        createWorkers.receive(actorPid, message::WorkerCreated{});
    ASSERT_TRUE(receiveResponse.has_value());
    ASSERT_EQ(receiveResponse->get()->name(), "loading");
  }
}

TEST(CreateWorkersStateTest, receive_invalid_message_type) {
  std::vector<std::string> servers = {"ServerA"};
  auto cState = ConductorState(ExecutionSpecifications(),
                               std::make_unique<LookupInfoMock>(servers));
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messages();

  {
    actor::ActorPID actorPid{
        .server = servers.at(0), .database = databaseName, .id = {0}};
    auto invalidMessage = message::ConductorStart{};
    auto receiveResponse = createWorkers.receive(actorPid, invalidMessage);
    // Currenty returns nullopt. After (GORDO-1553) will return error state.
    ASSERT_EQ(receiveResponse, std::nullopt);
  }
}

TEST(CreateWorkersStateTest, receive_valid_message_from_unknown_server) {
  std::vector<std::string> servers = {"ServerA"};
  auto cState = ConductorState(ExecutionSpecifications(),
                               std::make_unique<LookupInfoMock>(servers));
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messages();

  {
    actor::ActorPID unknownActorPid{
        .server = "UnknownServerX", .database = databaseName, .id = {0}};
    auto receiveResponse =
        createWorkers.receive(unknownActorPid, message::WorkerCreated{});
    // Currenty returns nullopt. After (GORDO-1553) will return error state.
    ASSERT_EQ(receiveResponse, std::nullopt);
  }
}

TEST(CreateWorkersStateTest, receive_valid_error_message) {
  std::vector<std::string> servers = {"ServerA"};
  auto cState = ConductorState(ExecutionSpecifications(),
                               std::make_unique<LookupInfoMock>(servers));
  auto createWorkers = CreateWorkers(cState);
  auto msgs = createWorkers.messages();

  {
    actor::ActorPID unknownActorPid{
        .server = servers.at(0), .database = databaseName, .id = {0}};
    auto errorMessage = arangodb::ResultT<message::WorkerCreated>(
        TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    auto receiveResponse = createWorkers.receive(unknownActorPid, errorMessage);
    // Currenty returns nullopt. After (GORDO-1553) will return error state.
    ASSERT_EQ(receiveResponse, std::nullopt);
  }
}