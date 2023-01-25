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
class AgencyCache;
class DatabaseFeature;
class MaintenanceFeature;
class TransactionState;

template<typename T>
class ResultT;

namespace network {
class ConnectionPool;
}

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
struct IDocumentStateNetworkHandler;
struct IDocumentStateShardHandler;
struct IDocumentStateSnapshotHandler;
struct IDocumentStateTransactionHandler;
struct IDocumentStateTransaction;

struct IDocumentStateHandlersFactory {
  virtual ~IDocumentStateHandlersFactory() = default;
  virtual auto createAgencyHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateAgencyHandler> = 0;
  virtual auto createShardHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> = 0;
  virtual auto createSnapshotHandler(TRI_vocbase_t& vocbase,
                                     GlobalLogIdentifier const& gid)
      -> std::unique_ptr<IDocumentStateSnapshotHandler> = 0;
  virtual auto createTransactionHandler(TRI_vocbase_t& vocbase,
                                        GlobalLogIdentifier gid)
      -> std::unique_ptr<IDocumentStateTransactionHandler> = 0;
  virtual auto createTransaction(DocumentLogEntry const& doc,
                                 TRI_vocbase_t& vocbase)
      -> std::shared_ptr<IDocumentStateTransaction> = 0;
  virtual auto createNetworkHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateNetworkHandler> = 0;
};

class DocumentStateHandlersFactory
    : public IDocumentStateHandlersFactory,
      public std::enable_shared_from_this<DocumentStateHandlersFactory> {
 public:
  DocumentStateHandlersFactory(ArangodServer& server, AgencyCache& agencyCache,
                               network::ConnectionPool* connectionPool,
                               MaintenanceFeature& maintenanceFeature);
  auto createAgencyHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateAgencyHandler> override;
  auto createShardHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> override;
  auto createSnapshotHandler(TRI_vocbase_t& vocbase,
                             GlobalLogIdentifier const& gid)
      -> std::unique_ptr<IDocumentStateSnapshotHandler> override;
  auto createTransactionHandler(TRI_vocbase_t& vocbase, GlobalLogIdentifier gid)
      -> std::unique_ptr<IDocumentStateTransactionHandler> override;
  auto createTransaction(DocumentLogEntry const& doc, TRI_vocbase_t& vocbase)
      -> std::shared_ptr<IDocumentStateTransaction> override;
  auto createNetworkHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateNetworkHandler> override;

 private:
  ArangodServer& _server;
  AgencyCache& _agencyCache;
  network::ConnectionPool* _connectionPool;
  MaintenanceFeature& _maintenanceFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
