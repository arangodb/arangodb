////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <string>
#include <memory>
#include <optional>

struct TRI_vocbase_t;

namespace arangodb {
class MaintenanceFeature;

namespace network {
class ConnectionPool;
}

namespace replication2 {
struct GlobalLogIdentifier;
class LogId;
}  // namespace replication2

}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateNetworkHandler;
struct IDocumentStateShardHandler;
struct IDocumentStateSnapshotHandler;
struct IDocumentStateTransactionHandler;
struct IDocumentStateTransaction;
struct IMaintenanceActionExecutor;

struct IDocumentStateHandlersFactory {
  virtual ~IDocumentStateHandlersFactory() = default;
  virtual auto createShardHandler(TRI_vocbase_t& vocbase,
                                  GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> = 0;
  virtual auto createSnapshotHandler(TRI_vocbase_t& vocbase,
                                     GlobalLogIdentifier const& gid)
      -> std::shared_ptr<IDocumentStateSnapshotHandler> = 0;
  virtual auto createTransactionHandler(
      TRI_vocbase_t& vocbase, GlobalLogIdentifier gid,
      std::shared_ptr<IDocumentStateShardHandler> shardHandler)
      -> std::unique_ptr<IDocumentStateTransactionHandler> = 0;
  virtual auto createTransaction(TRI_vocbase_t& vocbase, TransactionId tid,
                                 ShardID const& shard,
                                 AccessMode::Type accessType)
      -> std::shared_ptr<IDocumentStateTransaction> = 0;
  virtual auto createNetworkHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateNetworkHandler> = 0;
  virtual auto createMaintenanceActionExecutor(GlobalLogIdentifier gid,
                                               ServerID server)
      -> std::shared_ptr<IMaintenanceActionExecutor> = 0;
};

class DocumentStateHandlersFactory
    : public IDocumentStateHandlersFactory,
      public std::enable_shared_from_this<DocumentStateHandlersFactory> {
 public:
  DocumentStateHandlersFactory(network::ConnectionPool* connectionPool,
                               MaintenanceFeature& maintenanceFeature);
  auto createShardHandler(TRI_vocbase_t& vocbase, GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> override;
  auto createSnapshotHandler(TRI_vocbase_t& vocbase,
                             GlobalLogIdentifier const& gid)
      -> std::shared_ptr<IDocumentStateSnapshotHandler> override;
  auto createTransactionHandler(
      TRI_vocbase_t& vocbase, GlobalLogIdentifier gid,
      std::shared_ptr<IDocumentStateShardHandler> shardHandler)
      -> std::unique_ptr<IDocumentStateTransactionHandler> override;
  auto createTransaction(TRI_vocbase_t& vocbase, TransactionId tid,
                         ShardID const& shard, AccessMode::Type accessType)
      -> std::shared_ptr<IDocumentStateTransaction> override;
  auto createNetworkHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateNetworkHandler> override;
  auto createMaintenanceActionExecutor(GlobalLogIdentifier gid, ServerID server)
      -> std::shared_ptr<IMaintenanceActionExecutor> override;

 private:
  network::ConnectionPool* _connectionPool;
  MaintenanceFeature& _maintenanceFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
