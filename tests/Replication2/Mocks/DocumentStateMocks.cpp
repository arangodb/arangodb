////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/Mocks/DocumentStateMocks.h"

namespace arangodb::replication2::tests {
MockDocumentStateTransactionHandler::MockDocumentStateTransactionHandler(
    std::shared_ptr<
        replicated_state::document::IDocumentStateTransactionHandler>
        real)
    : _real(std::move(real)) {
  ON_CALL(*this,
          applyEntry(
              testing::Matcher<replicated_state::document::ReplicatedOperation>(
                  testing::_)))
      .WillByDefault(
          [this](replicated_state::document::ReplicatedOperation op) {
            return _real->applyEntry(std::move(op));
          });
  ON_CALL(*this,
          applyEntry(
              testing::Matcher<replicated_state::document::ReplicatedOperation::
                                   OperationType const&>(testing::_)))
      .WillByDefault([this](replicated_state::document::ReplicatedOperation::
                                OperationType const& op) {
        return _real->applyEntry(op);
      });
  ON_CALL(*this, removeTransaction(testing::_))
      .WillByDefault(
          [this](TransactionId tid) { return _real->removeTransaction(tid); });
  ON_CALL(*this, getUnfinishedTransactions())
      .WillByDefault([this]() -> TransactionMap const& {
        return _real->getUnfinishedTransactions();
      });
  ON_CALL(*this, validate(testing::_))
      .WillByDefault(
          [this](replicated_state::document::ReplicatedOperation::
                     OperationType const& op) { return _real->validate(op); });
  ON_CALL(*this, getTransactionsForShard(testing::_))
      .WillByDefault([this](ShardID const& shard) {
        return _real->getTransactionsForShard(shard);
      });
}

cluster::RebootTracker MockDocumentStateSnapshotHandler::rebootTracker =
    cluster::RebootTracker{nullptr};

MockDocumentStateSnapshotHandler::MockDocumentStateSnapshotHandler(
    std::shared_ptr<replicated_state::document::IDocumentStateSnapshotHandler>
        real)
    : _real(std::move(real)) {
  ON_CALL(*this, create(testing::_, testing::_))
      .WillByDefault(
          [this](
              replicated_state::document::ShardMap shards,
              replicated_state::document::SnapshotParams::Start const& params) {
            return _real->create(std::move(shards), params);
          });
  ON_CALL(*this, find(testing::_))
      .WillByDefault(
          [this](replicated_state::document::SnapshotId const& snapshotId) {
            return _real->find(snapshotId);
          });
  ON_CALL(*this, status()).WillByDefault([this]() { return _real->status(); });
  ON_CALL(*this, clear()).WillByDefault([this]() { _real->clear(); });
  ON_CALL(*this, finish(testing::_))
      .WillByDefault([this](replicated_state::document::SnapshotId const& id) {
        return _real->finish(id);
      });
  ON_CALL(*this, abort(testing::_))
      .WillByDefault([this](replicated_state::document::SnapshotId const& id) {
        return _real->abort(id);
      });
  ON_CALL(*this, giveUpOnShard(testing::_))
      .WillByDefault(
          [this](ShardID const& shardId) { _real->giveUpOnShard(shardId); });
}

MockDatabaseSnapshot::MockDatabaseSnapshot(
    std::shared_ptr<replicated_state::document::ICollectionReader> reader)
    : _reader(std::move(reader)) {
  ON_CALL(*this, createCollectionReader(testing::_))
      .WillByDefault([this](std::string_view collectionName) {
        return std::make_unique<MockCollectionReaderDelegator>(_reader);
      });
  ON_CALL(*this, resetTransaction()).WillByDefault([]() { return Result{}; });
}

MockDocumentStateHandlersFactory::MockDocumentStateHandlersFactory(
    std::shared_ptr<MockDatabaseSnapshotFactory> snapshotFactory)
    : databaseSnapshotFactory(std::move(snapshotFactory)) {}

auto MockDocumentStateHandlersFactory::makeUniqueDatabaseSnapshotFactory()
    -> std::unique_ptr<replicated_state::document::IDatabaseSnapshotFactory> {
  return std::make_unique<MockDatabaseSnapshotFactoryDelegator>(
      databaseSnapshotFactory);
}

auto MockDocumentStateHandlersFactory::makeRealSnapshotHandler(
    cluster::RebootTracker* rebootTracker)
    -> std::shared_ptr<MockDocumentStateSnapshotHandler> {
  if (rebootTracker == nullptr) {
    rebootTracker = &MockDocumentStateSnapshotHandler::rebootTracker;
  }
  auto real = std::make_shared<
      replicated_state::document::DocumentStateSnapshotHandler>(
      makeUniqueDatabaseSnapshotFactory(), *rebootTracker);
  return std::make_shared<testing::NiceMock<MockDocumentStateSnapshotHandler>>(
      std::move(real));
}

auto MockDocumentStateHandlersFactory::makeRealTransactionHandler(
    TRI_vocbase_t* vocbase, GlobalLogIdentifier const& gid,
    std::shared_ptr<replicated_state::document::IDocumentStateShardHandler>
        shardHandler) -> std::shared_ptr<MockDocumentStateTransactionHandler> {
  auto real = std::make_shared<
      replicated_state::document::DocumentStateTransactionHandler>(
      gid, vocbase, shared_from_this(), std::move(shardHandler));
  return std::make_shared<
      testing::NiceMock<MockDocumentStateTransactionHandler>>(std::move(real));
}

MockCollectionReader::MockCollectionReader(std::vector<std::string> const& data)
    : controlledSoftLimit(1), _data(data), _idx(0) {
  ON_CALL(*this, hasMore()).WillByDefault([this]() {
    return _idx < _data.size();
  });
  ON_CALL(*this, getDocCount()).WillByDefault([this]() {
    return _data.size();
  });
  ON_CALL(*this, read(testing::_, testing::_))
      .WillByDefault([this](VPackBuilder& builder, std::size_t softLimit) {
        VPackArrayBuilder b(&builder);
        std::size_t totalSize{0};
        while (_idx < _data.size() && totalSize < controlledSoftLimit) {
          totalSize += _data[_idx].size();
          builder.add(VPackValue(_data[_idx]));
          ++_idx;
        }
      });
}

void MockCollectionReader::reset() {
  _idx = 0;
  controlledSoftLimit = 1;
}

DocumentLogEntryIterator::DocumentLogEntryIterator(
    std::vector<replicated_state::document::DocumentLogEntry> entries)
    : entries(std::move(entries)), iter(this->entries.begin()) {}

auto DocumentLogEntryIterator::next() -> std::optional<
    streams::StreamEntryView<replicated_state::document::DocumentLogEntry>> {
  if (iter != entries.end()) {
    auto idx = LogIndex(std::distance(std::begin(entries), iter) + 1);
    auto res = std::make_pair(idx, std::ref(*iter));
    ++iter;
    return res;
  } else {
    return std::nullopt;
  }
}

auto DocumentLogEntryIterator::range() const noexcept -> LogRange {
  return LogRange{LogIndex{1}, LogIndex{entries.size() + 1}};
}
}  // namespace arangodb::replication2::tests
