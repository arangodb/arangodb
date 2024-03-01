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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Thread.h"

#include <rocksdb/listener.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace rocksdb {
class DB;
class ColumnFamilyHandle;
struct CompactionJobInfo;
struct FlushJobInfo;
class WriteControllerToken;
}  // namespace rocksdb

namespace arangodb {
class RocksDBEngine;

class RocksDBRateLimiterThread final : public Thread,
                                       public rocksdb::EventListener {
 public:
  RocksDBRateLimiterThread(RocksDBEngine& engine, uint64_t numSlots,
                           uint64_t frequency, uint64_t scalingFactor,
                           uint64_t minWriteRate, uint64_t maxWriteRate);
  ~RocksDBRateLimiterThread();

  uint64_t currentRate() const noexcept;

  // overrides for listener interface from rocksdb/event_listener.h:
  void OnFlushBegin(rocksdb::DB* db,
                    rocksdb::FlushJobInfo const& flush_job_info) override;

  void OnFlushCompleted(rocksdb::DB* db,
                        rocksdb::FlushJobInfo const& flush_job_info) override;

  void OnCompactionCompleted(rocksdb::DB* db,
                             rocksdb::CompactionJobInfo const& /*ci*/) override;

  void OnStallConditionsChanged(
      rocksdb::WriteStallInfo const& /*info*/) override;

  void beginShutdown() override;

  void setFamilies(std::vector<rocksdb::ColumnFamilyHandle*> const& families);

 protected:
  void run() override;

 private:
  uint64_t computePendingCompactionBytes() const;
  // must be called with _mutex held
  size_t actualHistoryIndex() const noexcept;
  // must be called with _mutex held
  void setRateInRocksDB(uint64_t rate, std::string_view context);

  RocksDBEngine& _engine;

  rocksdb::DB* _db;

  std::vector<rocksdb::ColumnFamilyHandle*> _families;

  uint64_t _numSlots;
  uint64_t _frequency;  // provided in ms
  uint64_t _scalingFactor;
  uint64_t _minWriteRate;
  uint64_t _maxWriteRate;

  std::atomic<uint64_t> _currentRate;

  std::mutex _mutex;
  std::condition_variable _cv;

  // protected by _mutex
  struct HistoryEntry {
    uint64_t compactionDebt{0};
    uint64_t totalBytes{0};
    std::chrono::microseconds totalTime{};
  };
  std::vector<HistoryEntry> _history;
  // current rate limiting round. will start at 0 and will increase until
  // overflow. do not use it to access entries inside _history, as this will
  // produce out-of-bounds-accesses. instead use actualHistoryIndex() to
  // access the history items inside the valid range
  size_t _currentHistoryIndex;

  // protected by _mutex
  std::unique_ptr<rocksdb::WriteControllerToken> _delayToken;
};
}  // namespace arangodb
