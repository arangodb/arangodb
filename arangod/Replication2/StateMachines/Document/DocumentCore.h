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

#pragma once

#include "DocumentLogEntry.h"
#include "DocumentStateMachine.h"
#include "DocumentStateStrategy.h"

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb::replication2::replicated_state::document {

struct DocumentCore {
  explicit DocumentCore(
      GlobalLogIdentifier gid, DocumentCoreParameters coreParameters,
      std::shared_ptr<IDocumentStateAgencyHandler> agencyHandler,
      std::shared_ptr<IDocumentStateShardHandler> shardHandler,
      LoggerContext loggerContext);

  LoggerContext const loggerContext;

  auto getShardId() -> std::string_view;

 private:
  GlobalLogIdentifier _gid;
  DocumentCoreParameters _params;
  ShardID _shardId;
  std::shared_ptr<IDocumentStateAgencyHandler> _agencyHandler;
  std::shared_ptr<IDocumentStateShardHandler> _shardHandler;
};
}  // namespace arangodb::replication2::replicated_state::document
