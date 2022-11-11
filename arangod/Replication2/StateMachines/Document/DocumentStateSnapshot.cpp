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

#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"

#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::replication2::replicated_state::document {
auto SnapshotId::fromString(std::string_view name) noexcept
    -> std::optional<SnapshotId> {
  if (std::all_of(name.begin(), name.end(),
                  [](char c) { return std::isdigit(c); })) {
    return SnapshotId{basics::StringUtils::uint64(name)};
  }
  return std::nullopt;
}

[[nodiscard]] SnapshotId::operator velocypack::Value() const noexcept {
  return velocypack::Value(id());
}

auto to_string(SnapshotId snapshotId) -> std::string {
  return std::to_string(snapshotId.id());
}

Snapshot::Snapshot(SnapshotId id,
                   std::shared_ptr<LogicalCollection> logicalCollection)
    : _id(id),
      _state{snapshot::Ongoing(_id, std::move(logicalCollection))},
      _status(_state) {}

namespace snapshot {
Ongoing::Ongoing(SnapshotId id,
                 std::shared_ptr<LogicalCollection> logicalCollection)
    : _id(id),
      _logicalCollection(std::move(logicalCollection)),
      _ctx(transaction::StandaloneContext::Create(
          _logicalCollection->vocbase())) {
  transaction::Options options;
  options.requiresReplication = false;

  _trx = std::make_unique<SingleCollectionTransaction>(
      _ctx, *_logicalCollection, AccessMode::Type::READ, options);

  // We call begin here so that rocksMethods are initialized
  if (auto res = _trx->begin(); res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  OperationOptions countOptions(ExecContext::current());
  OperationResult countResult = _trx->count(
      _logicalCollection->name(), transaction::CountType::Normal, countOptions);
  if (countResult.ok()) {
    _totalDocs = countResult.slice().getNumber<uint64_t>();
  }

  auto* physicalCollection = _logicalCollection->getPhysical();
  if (physicalCollection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   _logicalCollection->name());
  }

  _it = physicalCollection->getReplicationIterator(
      ReplicationIterator::Ordering::Revision, *_trx);
}

auto Ongoing::next() -> SnapshotBatch {
  if (!_it->hasMore()) {
    return SnapshotBatch{_id, _logicalCollection->name()};
  }

  std::size_t batchSize{0};
  VPackBuilder builder;
  {
    VPackArrayBuilder ab(&builder);
    for (auto& revIterator = dynamic_cast<RevisionReplicationIterator&>(*_it);
         revIterator.hasMore() && batchSize < kBatchSizeLimit;
         revIterator.next()) {
      auto slice = revIterator.document();
      batchSize += slice.byteSize();
      builder.add(slice);
    }
  }

  return SnapshotBatch{.snapshotId = _id,
                       .shard = _logicalCollection->name(),
                       .hasMore = hasMore(),
                       .payload = builder.sharedSlice()};
}

bool Ongoing::hasMore() const { return _it->hasMore(); }

auto Ongoing::getTotalDocs() const -> std::optional<uint64_t> {
  return _totalDocs;
}
}  // namespace snapshot

auto Snapshot::fetch() -> ResultT<SnapshotBatch> {
  return std::visit(
      overload{
          [&](snapshot::Ongoing& ongoing) -> ResultT<SnapshotBatch> {
            auto batch = ongoing.next();
            ++_status.batchesSent;
            _status.bytesSent += batch.payload.byteSize();
            if (batch.payload.isArray()) {
              _status.docsSent += batch.payload.length();
            }
            _status.lastUpdated = std::chrono::system_clock::now();
            _status.shard = batch.shard;
            return ResultT<SnapshotBatch>::success(std::move(batch));
          },
          [&](snapshot::Finished const&) -> ResultT<SnapshotBatch> {
            return ResultT<SnapshotBatch>::error(
                TRI_ERROR_INTERNAL,
                fmt::format("Snapshot {} was finished!", _id));
          },
          [&](snapshot::Aborted const&) -> ResultT<SnapshotBatch> {
            return ResultT<SnapshotBatch>::error(
                TRI_ERROR_INTERNAL,
                fmt::format("Snapshot {} was aborted!", _id));
          },
      },
      _state);
}

auto Snapshot::finish() -> Result {
  return std::visit(
      overload{
          [&](snapshot::Ongoing& ongoing) -> Result {
            if (ongoing.hasMore()) {
              LOG_TOPIC("23913", DEBUG, Logger::REPLICATION2)
                  << "Snapshot " << _id
                  << " is being finished, but still has more data!";
            }
            _state = snapshot::Finished{};
            _status.shard = std::nullopt;
            return {};
          },
          [&](snapshot::Finished const&) -> Result { return {}; },
          [&](snapshot::Aborted const&) -> Result {
            return Result{TRI_ERROR_INTERNAL,
                          fmt::format("Snapshot {} was aborted!", _id)};
          },
      },
      _state);
}

auto Snapshot::abort() -> Result {
  return std::visit(
      overload{
          [&](snapshot::Ongoing& ongoing) -> Result {
            if (ongoing.hasMore()) {
              LOG_TOPIC("5ce86", DEBUG, Logger::REPLICATION2)
                  << "Snapshot " << _id
                  << " is being aborted, but still has more data!";
            }
            _state = snapshot::Aborted{};
            _status.shard = std::nullopt;
            return {};
          },
          [&](snapshot::Finished const&) -> Result {
            return Result{TRI_ERROR_INTERNAL,
                          fmt::format("Snapshot {} was finished!", _id)};
          },
          [&](snapshot::Aborted const&) -> Result { return {}; },
      },
      _state);
}

auto Snapshot::status() -> SnapshotStatus { return _status; }
}  // namespace arangodb::replication2::replicated_state::document
