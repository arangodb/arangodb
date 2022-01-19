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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "ReplicatedStateFeature.h"

#include "Basics/Exceptions.h"

#include "Logger/LogMacros.h"
#include "Basics/debugging.h"
#include "Basics/application-exit.h"

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_state::ReplicatedStateFeature::createReplicatedState(
    std::string_view name, std::shared_ptr<replicated_log::ReplicatedLog> log)
    -> std::shared_ptr<ReplicatedStateBase> {
  auto name_str =
      std::string{name};  // TODO C++20 transparent hashing not yet available
  if (auto iter = factories.find(name_str); iter != std::end(factories)) {
    return iter->second->createReplicatedState(std::move(log));
  }
  THROW_ARANGO_EXCEPTION(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);  // TODO fix error code
}

void replicated_state::ReplicatedStateFeature::assertWasInserted(
    std::string_view name, bool wasInserted) {
  if (!wasInserted) {
    LOG_TOPIC("5b761", FATAL, Logger::REPLICATED_STATE)
        << "register state type with duplicated name " << name;
    FATAL_ERROR_EXIT();
  }
}
