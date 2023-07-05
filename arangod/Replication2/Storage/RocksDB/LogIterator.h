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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/RocksDBUtils.h"
#include "Replication2/ReplicatedLog/LogEntry.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"

#include <rocksdb/db.h>
#include <rocksdb/iterator.h>

namespace arangodb::replication2::storage::rocksdb {

struct LogIterator : replication2::LogIterator {
  ~LogIterator() override = default;

  LogIterator(std::uint64_t objectId, ::rocksdb::DB* db,
              ::rocksdb::ColumnFamilyHandle* cf, LogIndex start)
      : _bounds(RocksDBKeyBounds::LogRange(objectId)),
        _upperBound(_bounds.end()) {
    ::rocksdb::ReadOptions opts;
    opts.prefix_same_as_start = true;
    opts.iterate_upper_bound = &_upperBound;
    _iter.reset(db->NewIterator(opts, cf));

    auto first = RocksDBKey{};
    first.constructLogEntry(objectId, start);
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

    return std::optional<LogEntry>{std::in_place,
                                   RocksDBKey::logIndex(_iter->key()),
                                   RocksDBValue::data(_iter->value())};
  }

  RocksDBKeyBounds const _bounds;
  ::rocksdb::Slice const _upperBound;
  std::unique_ptr<::rocksdb::Iterator> _iter;
  bool _first = true;
};

}  // namespace arangodb::replication2::storage::rocksdb
