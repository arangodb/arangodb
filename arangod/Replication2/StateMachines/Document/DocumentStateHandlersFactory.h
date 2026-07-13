////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/ShardID.h"
#include "Replication2/LoggerContext.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <memory>

namespace arangodb {
struct Database;
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

struct IDocumentStateErrorHandler;
struct IDocumentStateNetworkHandler;
struct IDocumentStateShardHandler;
struct IDocumentStateSnapshotHandler;
struct IDocumentStateTransactionHandler;
struct IDocumentStateTransaction;
struct IMaintenanceActionExecutor;

struct IDocumentStateHandlersFactory {
  virtual ~IDocumentStateHandlersFactory() = default;
  virtual auto createShardHandler(Database& vocbase, GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> = 0;
  virtual auto createSnapshotHandler(Database& vocbase, GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateSnapshotHandler> = 0;
  virtual auto createTransactionHandler(
      Database& vocbase, GlobalLogIdentifier gid,
      std::shared_ptr<IDocumentStateShardHandler> shardHandler)
      -> std::unique_ptr<IDocumentStateTransactionHandler> = 0;
  virtual auto createTransaction(Database& vocbase, TransactionId tid,
                                 ShardID const& shard,
                                 AccessMode::Type accessType,
                                 std::string_view userName)
      -> std::shared_ptr<IDocumentStateTransaction> = 0;
  virtual auto createNetworkHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateNetworkHandler> = 0;
  virtual auto createMaintenanceActionExecutor(Database& vocbase,
                                               GlobalLogIdentifier gid,
                                               ServerID server)
      -> std::shared_ptr<IMaintenanceActionExecutor> = 0;
  virtual auto createErrorHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateErrorHandler> = 0;
  virtual auto createLogger(GlobalLogIdentifier gid) -> LoggerContext = 0;
};

class DocumentStateHandlersFactory
    : public IDocumentStateHandlersFactory,
      public std::enable_shared_from_this<DocumentStateHandlersFactory> {
 public:
  DocumentStateHandlersFactory(network::ConnectionPool* connectionPool,
                               MaintenanceFeature& maintenanceFeature,
                               LoggerContext defaultLoggerContext);
  auto createShardHandler(Database& vocbase, GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> override;
  auto createSnapshotHandler(Database& vocbase, GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateSnapshotHandler> override;
  auto createTransactionHandler(
      Database& vocbase, GlobalLogIdentifier gid,
      std::shared_ptr<IDocumentStateShardHandler> shardHandler)
      -> std::unique_ptr<IDocumentStateTransactionHandler> override;
  auto createTransaction(Database& vocbase, TransactionId tid,
                         ShardID const& shard, AccessMode::Type accessType,
                         std::string_view userName)
      -> std::shared_ptr<IDocumentStateTransaction> override;
  auto createNetworkHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateNetworkHandler> override;
  auto createMaintenanceActionExecutor(Database& vocbase,
                                       GlobalLogIdentifier gid, ServerID server)
      -> std::shared_ptr<IMaintenanceActionExecutor> override;
  auto createErrorHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateErrorHandler> override;
  auto createLogger(GlobalLogIdentifier gid) -> LoggerContext override;

 private:
  network::ConnectionPool* _connectionPool;
  MaintenanceFeature& _maintenanceFeature;
  LoggerContext const _defaultLoggerContext;
};

}  // namespace arangodb::replication2::replicated_state::document
