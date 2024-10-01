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

#include "Replication2/StateMachines/Document/CollectionReader.h"

#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/ReplicationIterator.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

namespace arangodb::replication2::replicated_state::document {

SnapshotTransaction::SnapshotTransaction(
    std::shared_ptr<transaction::Context> ctx)
    : transaction::Methods(std::move(ctx), options()) {}

auto SnapshotTransaction::options() -> transaction::Options {
  transaction::Options options;
  options.requiresReplication = false;
  options.fillBlockCache = false;  // TODO - check whether this is a good idea
  return options;
}

void SnapshotTransaction::addCollection(LogicalCollection const& collection) {
  transaction::Methods::addCollectionAtRuntime(
      collection.id(), collection.name(), AccessMode::Type::READ)
      .waitAndGet();
}

DatabaseSnapshot::DatabaseSnapshot(TRI_vocbase_t& vocbase)
    : _vocbase(vocbase),
      _ctx(transaction::StandaloneContext::create(
          vocbase,
          transaction::OperationOriginInternal{
              "snapshotting collection for replication"})),
      _trx(std::make_unique<SnapshotTransaction>(_ctx)) {
  // We call begin here so that rocksMethods are initialized
  if (auto res = _trx->begin(); res.fail()) {
    LOG_TOPIC("b4e74", ERR, Logger::REPLICATION2)
        << "Failed to begin transaction: " << res.errorMessage();
    THROW_ARANGO_EXCEPTION(res);
  }
}

auto DatabaseSnapshot::createCollectionReader(
    std::shared_ptr<LogicalCollection> shard)
    -> std::unique_ptr<ICollectionReader> {
  return std::make_unique<CollectionReader>(std::move(shard), *_trx);
}

auto DatabaseSnapshot::resetTransaction() -> Result {
  _trx.reset();
  _ctx = transaction::StandaloneContext::create(
      _vocbase, transaction::OperationOriginInternal{
                    "snapshotting collection for replication"});
  _trx = std::make_unique<SnapshotTransaction>(_ctx);
  return _trx->begin();
}

CollectionReader::CollectionReader(
    std::shared_ptr<LogicalCollection> logicalCollection,
    SnapshotTransaction& trx)
    : _logicalCollection(std::move(logicalCollection)) {
  trx.addCollection(*_logicalCollection);

  OperationOptions countOptions(ExecContext::current());
  OperationResult countResult =
      trx.countAsync(_logicalCollection->name(),
                     transaction::CountType::kNormal, countOptions)
          .waitAndGet();
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
