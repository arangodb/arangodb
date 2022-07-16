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

#include "Futures/Future.h"
#include "RestServer/arangod.h"
#include "Transaction/Options.h"
#include "VocBase/Identifiers/TransactionId.h"

#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Options.h"

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
};  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {

struct DocumentLogEntry;

struct IDocumentStateAgencyHandler {
  virtual ~IDocumentStateAgencyHandler() = default;
  virtual auto getCollectionPlan(std::string const& database,
                                 std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> = 0;
  virtual auto reportShardInCurrent(
      std::string const& database, std::string const& collectionId,
      std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties) -> Result = 0;
};

class DocumentStateAgencyHandler : public IDocumentStateAgencyHandler {
 public:
  explicit DocumentStateAgencyHandler(ArangodServer& server,
                                      AgencyCache& agencyCache);
  auto getCollectionPlan(std::string const& database,
                         std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> override;
  auto reportShardInCurrent(
      std::string const& database, std::string const& collectionId,
      std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> Result override;

 private:
  ArangodServer& _server;
  AgencyCache& _agencyCache;
};

struct IDocumentStateShardHandler {
  virtual ~IDocumentStateShardHandler() = default;
  virtual auto createLocalShard(
      GlobalLogIdentifier const& gid, std::string const& collectionId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> = 0;
};

class DocumentStateShardHandler : public IDocumentStateShardHandler {
 public:
  explicit DocumentStateShardHandler(MaintenanceFeature& maintenanceFeature);
  static auto stateIdToShardId(LogId logId) -> std::string;
  auto createLocalShard(GlobalLogIdentifier const& gid,
                        std::string const& collectionId,
                        std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> override;

 private:
  MaintenanceFeature& _maintenanceFeature;
};

struct IDocumentStateTransaction {
  virtual ~IDocumentStateTransaction() = default;

  virtual auto getTid() -> TransactionId = 0;
  virtual auto getOperation() -> OperationType = 0;
};

class DocumentStateTransaction : public IDocumentStateTransaction {
 public:
  explicit DocumentStateTransaction(TRI_vocbase_t* vocbase,
                                    DocumentLogEntry entry);
  auto getTid() -> TransactionId override;
  auto getOperation() -> OperationType override;

  auto getShardId() -> ShardID;
  auto getOptions() -> transaction::Options;
  auto getState() -> std::shared_ptr<TransactionState>;
  auto getPayload() -> VPackSlice;
  auto getMethods() -> std::shared_ptr<transaction::Methods>;

  void setMethods(std::shared_ptr<transaction::Methods> methods);

 private:
  DocumentLogEntry _entry;
  const TransactionId _tid;

 public:
  transaction::Options _options;
  std::shared_ptr<TransactionState> _state;
  std::shared_ptr<transaction::Methods> _methods;
};

struct IDocumentStateTransactionHandler {
  virtual ~IDocumentStateTransactionHandler() = default;
  virtual auto ensureTransaction(DocumentLogEntry entry)
      -> std::shared_ptr<IDocumentStateTransaction> = 0;
  virtual auto initTransaction(TransactionId tid) -> Result = 0;
  virtual auto startTransaction(TransactionId tid) -> Result = 0;
  virtual auto applyTransaction(TransactionId tid)
      -> futures::Future<Result> = 0;
  virtual auto finishTransaction(TransactionId tid)
      -> futures::Future<Result> = 0;
};

class DocumentStateTransactionHandler
    : public IDocumentStateTransactionHandler {
 public:
  explicit DocumentStateTransactionHandler(DatabaseFeature& databaseFeature);
  explicit DocumentStateTransactionHandler(
      std::shared_ptr<IDocumentStateTransactionHandler> const& handler,
      std::string const& database);

  auto ensureTransaction(DocumentLogEntry entry)
      -> std::shared_ptr<IDocumentStateTransaction> override;
  auto initTransaction(TransactionId tid) -> Result override;
  auto startTransaction(TransactionId tid) -> Result override;
  auto applyTransaction(TransactionId tid) -> futures::Future<Result> override;
  auto finishTransaction(TransactionId tid) -> futures::Future<Result> override;

 private:
  auto getTrx(TransactionId tid) -> std::shared_ptr<DocumentStateTransaction>;

 private:
  DatabaseFeature& _databaseFeature;
  TRI_vocbase_t* _vocbase;
  std::unordered_map<TransactionId, std::shared_ptr<DocumentStateTransaction>>
      _transactions;
};

}  // namespace arangodb::replication2::replicated_state::document
