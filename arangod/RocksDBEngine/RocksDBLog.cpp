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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "Basics/Exceptions.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/debugging.h"

#include "RocksDBKey.h"
#include "RocksDBLog.h"
#include "RocksDBValue.h"

using namespace arangodb;
using namespace arangodb::replication2;

RocksDBLog::RocksDBLog(replication2::LogId id, rocksdb::ColumnFamilyHandle* cf,
                       rocksdb::DB* db, uint64_t objectId)
    : PersistedLog(id), _objectId(objectId), _cf(cf), _db(db) {}

auto RocksDBLog::insert(std::shared_ptr<LogIterator> iter) -> Result {
  rocksdb::WriteBatch wb;
  while (auto entry = iter->next()) {
    auto key = RocksDBKey{};
    key.constructLogEntry(_objectId, entry->logIndex());
    auto value = RocksDBValue::LogEntry(entry->logTerm(), entry->logPayload());
    auto s = wb.Put(_cf, key.string(), value.string());
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }
  }

  rocksdb::WriteOptions opts;
  opts.sync = true;
  if (auto s = _db->Write(opts, &wb); !s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

struct RocksDBLogIterator : replication2::LogIterator {
  ~RocksDBLogIterator() override = default;

  RocksDBLogIterator(RocksDBLog* log, rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf, LogIndex start)
      : _bounds(log->getBounds()), _upperBound(_bounds.end()) {
    rocksdb::ReadOptions opts;
    opts.prefix_same_as_start = true;
    opts.iterate_upper_bound = &_upperBound;
    _iter.reset(db->NewIterator(opts, cf));

    auto first = RocksDBKey{};
    first.constructLogEntry(log->objectId(), start);
    _iter->Seek(first.string());
  }

  auto next() -> std::optional<LogEntry> override {
    if (!_first) {
      _iter->Next();
    }
    _first = false;

    if (!_iter->Valid()) {
      auto s = _iter->status();
      if (!s.ok()) {
        auto res = rocksutils::convertStatus(s);
        THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
      }

      return std::nullopt;
    }

    return std::optional<LogEntry>{std::in_place, RocksDBValue::logTerm(_iter->value()),
                                   RocksDBKey::logIndex(_iter->key()),
                                   RocksDBValue::logPayload(_iter->value())};
  }

  RocksDBKeyBounds const _bounds;
  rocksdb::Slice const _upperBound;
  std::unique_ptr<rocksdb::Iterator> _iter;
  bool _first = true;
};

auto RocksDBLog::read(LogIndex start) -> std::shared_ptr<LogIterator> {
  return std::make_shared<RocksDBLogIterator>(this, _db, _cf, start);
}

auto RocksDBLog::drop() -> Result {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto RocksDBLog::removeFront(replication2::LogIndex stop) -> Result {
  auto last = RocksDBKey();
  last.constructLogEntry(_objectId, LogIndex{stop.value});

  rocksdb::WriteOptions opts;
  auto s = _db->DeleteRange(opts, _cf, getBounds().start(), last.string());
  return rocksutils::convertStatus(s);
}

auto RocksDBLog::removeBack(replication2::LogIndex start) -> Result {
  auto first = RocksDBKey();
  first.constructLogEntry(_objectId, LogIndex{start.value});

  rocksdb::WriteOptions opts;
  auto s = _db->DeleteRange(opts, _cf, first.string(), getBounds().end());
  return rocksutils::convertStatus(s);
}
