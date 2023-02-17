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

namespace arangodb::replication2::test {
MockDocumentStateTransactionHandler::MockDocumentStateTransactionHandler(
    std::shared_ptr<
        replicated_state::document::IDocumentStateTransactionHandler>
        real)
    : _real(std::move(real)) {
  ON_CALL(*this, applyEntry(testing::_))
      .WillByDefault([this](replicated_state::document::DocumentLogEntry doc) {
        return _real->applyEntry(std::move(doc));
      });
  ON_CALL(*this, ensureTransaction(testing::_))
      .WillByDefault(
          [this](replicated_state::document::DocumentLogEntry const& doc)
              -> std::shared_ptr<
                  replicated_state::document::IDocumentStateTransaction> {
            return _real->ensureTransaction(doc);
          });
  ON_CALL(*this, removeTransaction(testing::_))
      .WillByDefault(
          [this](TransactionId tid) { return _real->removeTransaction(tid); });
  ON_CALL(*this, getUnfinishedTransactions())
      .WillByDefault([this]() -> TransactionMap const& {
        return _real->getUnfinishedTransactions();
      });
}

MockDocumentStateSnapshotHandler::MockDocumentStateSnapshotHandler(
    std::shared_ptr<replicated_state::document::IDocumentStateSnapshotHandler>
        real)
    : _real(std::move(real)) {
  ON_CALL(*this, create(testing::_))
      .WillByDefault([this](std::vector<ShardID> shardIds) {
        return _real->create(std::move(shardIds));
      });
  ON_CALL(*this, find(testing::_))
      .WillByDefault(
          [this](replicated_state::document::SnapshotId const& snapshotId) {
            return _real->find(snapshotId);
          });
  ON_CALL(*this, status()).WillByDefault([this]() { return _real->status(); });
  ON_CALL(*this, clear()).WillByDefault([this]() { _real->clear(); });
  ON_CALL(*this, clearInactiveSnapshots()).WillByDefault([this]() {
    _real->clearInactiveSnapshots();
  });
}

MockDatabaseSnapshot::MockDatabaseSnapshot(
    std::shared_ptr<replicated_state::document::ICollectionReader> reader)
    : _reader(std::move(reader)) {
  ON_CALL(*this, createCollectionReader(testing::_))
      .WillByDefault([this](std::string_view collectionName) {
        return std::make_unique<MockCollectionReaderDelegator>(_reader);
      });
}

MockDocumentStateHandlersFactory::MockDocumentStateHandlersFactory(
    std::shared_ptr<MockDatabaseSnapshotFactory> snapshotFactory)
    : databaseSnapshotFactory(std::move(snapshotFactory)) {}

auto MockDocumentStateHandlersFactory::makeUniqueDatabaseSnapshotFactory()
    -> std::unique_ptr<replicated_state::document::IDatabaseSnapshotFactory> {
  return std::make_unique<MockDatabaseSnapshotFactoryDelegator>(
      databaseSnapshotFactory);
}

auto MockDocumentStateHandlersFactory::makeRealSnapshotHandler()
    -> std::shared_ptr<MockDocumentStateSnapshotHandler> {
  auto real = std::make_shared<
      replicated_state::document::DocumentStateSnapshotHandler>(
      makeUniqueDatabaseSnapshotFactory());
  return std::make_shared<testing::NiceMock<MockDocumentStateSnapshotHandler>>(
      real);
}

auto MockDocumentStateHandlersFactory::makeRealTransactionHandler(
    GlobalLogIdentifier const& gid)
    -> std::shared_ptr<MockDocumentStateTransactionHandler> {
  auto real = std::make_shared<
      replicated_state::document::DocumentStateTransactionHandler>(
      gid, nullptr, shared_from_this());
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
}  // namespace arangodb::replication2::test
