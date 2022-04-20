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

#include "PrototypeStateMachine.h"
#include "PrototypeLeaderState.h"
#include "PrototypeFollowerState.h"
#include "PrototypeCore.h"

#include "Futures/Future.h"
#include "Logger/LogContextKeys.h"
#include "velocypack/Iterator.h"

#include "Replication2/ReplicatedLog/LogCommon.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

auto PrototypeFactory::constructFollower(std::unique_ptr<PrototypeCore> core)
    -> std::shared_ptr<PrototypeFollowerState> {
  return std::make_shared<PrototypeFollowerState>(std::move(core),
                                                  networkInterface);
}

auto PrototypeFactory::constructLeader(std::unique_ptr<PrototypeCore> core)
    -> std::shared_ptr<PrototypeLeaderState> {
  return std::make_shared<PrototypeLeaderState>(std::move(core));
}

auto PrototypeFactory::constructCore(GlobalLogIdentifier const& gid)
    -> std::unique_ptr<PrototypeCore> {
  LoggerContext const logContext =
      LoggerContext(Logger::REPLICATED_STATE).with<logContextKeyLogId>(gid.id);
  return std::make_unique<PrototypeCore>(gid, logContext, storageInterface);
}

PrototypeFactory::PrototypeFactory(
    std::shared_ptr<IPrototypeNetworkInterface> networkInterface,
    std::shared_ptr<IPrototypeStorageInterface> storageInterface)
    : networkInterface(std::move(networkInterface)),
      storageInterface(std::move(storageInterface)) {}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

template struct replicated_state::ReplicatedState<PrototypeState>;
