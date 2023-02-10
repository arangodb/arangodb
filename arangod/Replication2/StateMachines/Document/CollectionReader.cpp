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

#include "Replication2/StateMachines/Document/CollectionReader.h"

#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/ReplicationIterator.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

namespace arangodb::replication2::replicated_state::document {

ReplicationTransaction::ReplicationTransaction(
    std::shared_ptr<transaction::Context> ctx)
    : transaction::Methods(std::move(ctx), options()) {}

auto ReplicationTransaction::options() -> transaction::Options {
  transaction::Options options;
  options.requiresReplication = false;
  options.fillBlockCache = false;  // TODO - check whether this is a good idea
  return options;
}

auto ReplicationTransaction::addCollection(LogicalCollection const& collection)
    -> Result {
  return transaction::Methods::addCollection(collection.id(), collection.name(),
                                             AccessMode::Type::READ);
}

DatabaseSnapshot::DatabaseSnapshot(TRI_vocbase_t& vocbase)
    : _vocbase(vocbase), _ctx(transaction::StandaloneContext::Create(vocbase)) {
  _trx = std::make_unique<ReplicationTransaction>(_ctx);

  // We call begin here so that rocksMethods are initialized
  if (auto res = _trx->begin(); res.fail()) {
    LOG_TOPIC("b4e74", ERR, Logger::REPLICATION2)
        << "Failed to begin transaction: " << res.errorMessage();
    THROW_ARANGO_EXCEPTION(res);
  }
}

auto DatabaseSnapshot::createCollectionReader(std::string_view collectionName)
    -> std::unique_ptr<ICollectionReader> {
  auto logicalCollection = _vocbase.lookupCollection(collectionName);
  if (logicalCollection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   collectionName);
  }
  return std::make_unique<CollectionReader>(std::move(logicalCollection),
                                            *_trx);
}

CollectionReader::CollectionReader(
    std::shared_ptr<LogicalCollection> logicalCollection,
    ReplicationTransaction& trx)
    : _logicalCollection(std::move(logicalCollection)) {
  Result res = trx.addCollection(*logicalCollection);

  if (!res.ok()) {
    LOG_TOPIC("c49e5", ERR, Logger::REPLICATION2)
        << "Failed to add collection " << logicalCollection->name()
        << " to a transaction: " << res.errorMessage();
    THROW_ARANGO_EXCEPTION(res);
  }

  OperationOptions countOptions(ExecContext::current());
  OperationResult countResult = trx.count(
      _logicalCollection->name(), transaction::CountType::Normal, countOptions);
  if (countResult.ok()) {
    _totalDocs = countResult.slice().getNumber<uint64_t>();
  } else {
    // Will not fail because this is used just as an insight into the
    // collection
    LOG_TOPIC("49e64", WARN, Logger::REPLICATION2)
        << "Failed to get total number of documents in collection "
        << _logicalCollection->name() << ": " << countResult.errorMessage();
  }

  auto* physicalCollection = _logicalCollection->getPhysical();
  if (physicalCollection == nullptr) {
    LOG_TOPIC("7c8ce", ERR, Logger::REPLICATION2)
        << "Failed to get physical collection for collection "
        << _logicalCollection->name();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   _logicalCollection->name());
  }

  _it = physicalCollection->getReplicationIterator(
      ReplicationIterator::Ordering::Revision, trx);
  TRI_ASSERT(_it != nullptr);
}

[[nodiscard]] bool CollectionReader::hasMore() { return _it->hasMore(); }

[[nodiscard]] std::optional<uint64_t> CollectionReader::getDocCount() {
  return _totalDocs;
}

void CollectionReader::read(VPackBuilder& builder,
                            std::size_t const softLimit) {
  TRI_ASSERT(builder.isEmpty()) << builder.toJson();

  VPackArrayBuilder ab(&builder);
  if (!_it->hasMore()) {
    return;
  }

  std::size_t batchSize{0};
  for (auto& revIterator = dynamic_cast<RevisionReplicationIterator&>(*_it);
       revIterator.hasMore() && batchSize < softLimit; revIterator.next()) {
    auto slice = revIterator.document();
    batchSize += slice.byteSize();
    builder.add(slice);
  }
}

DatabaseSnapshotFactory::DatabaseSnapshotFactory(TRI_vocbase_t& vocbase)
    : _vocbase(vocbase) {}

auto DatabaseSnapshotFactory::createSnapshot()
    -> std::unique_ptr<IDatabaseSnapshot> {
  return std::make_unique<DatabaseSnapshot>(_vocbase);
}

}  // namespace arangodb::replication2::replicated_state::document
