////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/StateMachines/Document/DocumentLogEntry.h"

#include "Futures/Future.h"
#include "RestServer/arangod.h"
#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "Transaction/Options.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <string>
#include <memory>
#include <optional>

struct TRI_vocbase_t;

namespace arangodb {
class ClusterFeature;
class DatabaseFeature;
class MaintenanceFeature;
class TransactionState;

template<typename T>
class ResultT;

namespace replication2 {
struct GlobalLogIdentifier;
class LogId;
}  // namespace replication2

namespace velocypack {
class Builder;
}
}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateAgencyHandler;
struct IDocumentStateShardHandler;
struct IDocumentStateTransactionHandler;

struct IDocumentStateHandlersFactory {
  virtual ~IDocumentStateHandlersFactory() = default;
  virtual auto createAgencyHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateAgencyHandler> = 0;
  virtual auto createShardHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> = 0;
  virtual auto createTransactionHandler(GlobalLogIdentifier gid)
      -> std::unique_ptr<IDocumentStateTransactionHandler> = 0;
};

class DocumentStateHandlersFactory : public IDocumentStateHandlersFactory {
 public:
  DocumentStateHandlersFactory(ArangodServer& server,
                               ClusterFeature& clusterFeature,
                               MaintenanceFeature& maintenaceFeature,
                               DatabaseFeature& databaseFeature);
  auto createAgencyHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateAgencyHandler> override;
  auto createShardHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> override;
  auto createTransactionHandler(GlobalLogIdentifier gid)
      -> std::unique_ptr<IDocumentStateTransactionHandler> override;

 private:
  ArangodServer& _server;
  ClusterFeature& _clusterFeature;
  MaintenanceFeature& _maintenanceFeature;
  DatabaseFeature& _databaseFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
