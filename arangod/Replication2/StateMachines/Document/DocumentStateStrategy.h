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

namespace replication2 {
struct GlobalLogIdentifier;
class LogId;
}  // namespace replication2

namespace velocypack {
class Builder;
}
}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateAgencyHandler {
  virtual ~IDocumentStateAgencyHandler() = default;
  virtual auto getCollectionPlan(std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> = 0;
  virtual auto reportShardInCurrent(
      std::string const& collectionId, std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties) -> Result = 0;
};

class DocumentStateAgencyHandler : public IDocumentStateAgencyHandler {
 public:
  explicit DocumentStateAgencyHandler(GlobalLogIdentifier gid,
                                      ArangodServer& server,
                                      AgencyCache& agencyCache);
  auto getCollectionPlan(std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> override;
  auto reportShardInCurrent(
      std::string const& collectionId, std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> Result override;

 private:
  GlobalLogIdentifier _gid;
  ArangodServer& _server;
  AgencyCache& _agencyCache;
};

struct IDocumentStateShardHandler {
  virtual ~IDocumentStateShardHandler() = default;
  virtual auto createLocalShard(
      std::string const& collectionId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> = 0;
};

class DocumentStateShardHandler : public IDocumentStateShardHandler {
 public:
  explicit DocumentStateShardHandler(GlobalLogIdentifier gid,
                                     MaintenanceFeature& maintenanceFeature);
  static auto stateIdToShardId(LogId logId) -> std::string;
  auto createLocalShard(std::string const& collectionId,
                        std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> override;

 private:
  GlobalLogIdentifier _gid;
  MaintenanceFeature& _maintenanceFeature;
};

struct IDocumentStateTransaction {
  virtual ~IDocumentStateTransaction() = default;

  [[nodiscard]] virtual auto apply(DocumentLogEntry const& entry)
      -> futures::Future<Result> = 0;
  [[nodiscard]] virtual auto commit() -> futures::Future<Result> = 0;
  [[nodiscard]] virtual auto abort() -> futures::Future<Result> = 0;
};

class DocumentStateTransaction
    : public IDocumentStateTransaction,
      public std::enable_shared_from_this<DocumentStateTransaction> {
 public:
  explicit DocumentStateTransaction(
      std::unique_ptr<transaction::Methods> methods);
  auto apply(DocumentLogEntry const& entry) -> futures::Future<Result> override;
  auto commit() -> futures::Future<Result> override;
  auto abort() -> futures::Future<Result> override;

 private:
  std::unique_ptr<transaction::Methods> _methods;
};

struct IDocumentStateTransactionHandler {
  virtual ~IDocumentStateTransactionHandler() = default;
  virtual auto ensureTransaction(DocumentLogEntry entry)
      -> std::shared_ptr<IDocumentStateTransaction> = 0;
  virtual void removeTransaction(TransactionId tid) = 0;
};

class DocumentStateTransactionHandler
    : public IDocumentStateTransactionHandler {
 public:
  explicit DocumentStateTransactionHandler(GlobalLogIdentifier gid,
                                           DatabaseFeature& databaseFeature);
  auto ensureTransaction(DocumentLogEntry entry)
      -> std::shared_ptr<IDocumentStateTransaction> override;
  void removeTransaction(TransactionId tid) override;

 private:
  auto getTrx(TransactionId tid) -> std::shared_ptr<DocumentStateTransaction>;

 private:
  GlobalLogIdentifier _gid;
  DatabaseGuard _db;
  std::unordered_map<TransactionId, std::shared_ptr<DocumentStateTransaction>>
      _transactions;
};

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
  DocumentStateHandlersFactory(ArangodServer& server, AgencyCache& agencyCache,
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
  AgencyCache& _agencyCache;
  MaintenanceFeature& _maintenanceFeature;
  DatabaseFeature& _databaseFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
