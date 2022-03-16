////////////////////////////////////////////////////////////////////////////////
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "PrototypeStateMachineFeature.h"
#include "PrototypeStateMachine.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "velocypack/Iterator.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

namespace {
class PrototypeLeaderInterface : public IPrototypeLeaderInterface {
 public:
  PrototypeLeaderInterface(ParticipantId participantId,
                           network::ConnectionPool* pool)
      : participantId(std::move(participantId)), pool(pool){};

  auto getSnapshot(LogIndex waitForIndex) -> futures::Future<
      ResultT<std::unordered_map<std::string, std::string>>> override {
    auto path = basics::StringUtils::joinT("/", "_api/prototype-state", logId,
                                           "snapshot");
    network::RequestOptions opts;
    // TODO add functionality when other databases available
    opts.database = "replication2_prototype_state_test_db";
    opts.param("waitForIndex", std::to_string(waitForIndex.value));

    return network::sendRequest(pool, "server:" + participantId,
                                fuerte::RestVerb::Get, path, {}, opts)
        .thenValue(
            [](network::Response&& resp)
                -> ResultT<std::unordered_map<std::string, std::string>> {
              LOG_DEVEL << "response " << resp.statusCode();
              if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
                THROW_ARANGO_EXCEPTION(resp.combinedResult());
              } else {
                auto slice = resp.slice();
                if (auto result = slice.get("result"); result.isObject()) {
                  std::unordered_map<std::string, std::string> map;
                  for (auto it : VPackObjectIterator{result}) {
                    map.emplace(it.key.copyString(), it.value.copyString());
                  }
                  return map;
                }
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_INTERNAL, basics::StringUtils::concatT(
                                            "expected result containing map "
                                            "in leader response: ",
                                            slice.toJson()));
              }
            });
  }

 private:
  ParticipantId participantId;
  LogId logId = LogId{12};
  network::ConnectionPool* pool;
};

class PrototypeNetworkInterface : public IPrototypeNetworkInterface {
 public:
  explicit PrototypeNetworkInterface(network::ConnectionPool* pool)
      : pool(pool){};

  auto getLeaderInterface(ParticipantId id)
      -> ResultT<std::shared_ptr<IPrototypeLeaderInterface>> override {
    return ResultT<std::shared_ptr<IPrototypeLeaderInterface>>::success(
        std::make_shared<PrototypeLeaderInterface>(id, pool));
  }

  network::ConnectionPool* pool{nullptr};
};
}  // namespace

void PrototypeStateMachineFeature::start() {
  auto& replicatedStateFeature =
      server().getFeature<ReplicatedStateAppFeature>();
  auto& networkFeature = server().getFeature<NetworkFeature>();
  replicatedStateFeature.registerStateType<PrototypeState>(
      "prototype",
      std::make_shared<PrototypeNetworkInterface>(networkFeature.pool()));
}

PrototypeStateMachineFeature::PrototypeStateMachineFeature(Server& server)
    : ArangodFeature{server, *this} {}
