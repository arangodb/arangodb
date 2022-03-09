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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBBackgroundErrorListener.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {

RocksDBBackgroundErrorListener::~RocksDBBackgroundErrorListener() = default;

void RocksDBBackgroundErrorListener::OnBackgroundError(
    rocksdb::BackgroundErrorReason reason, rocksdb::Status* status) {
  if (status != nullptr && status->IsShutdownInProgress()) {
    // this is not a relevant error, so let's ignore it
    return;
  }

  if (!_called.exchange(true)) {
    char const* operation = "unknown";
    switch (reason) {
      case rocksdb::BackgroundErrorReason::kFlush: {
        operation = "flush";
        break;
      }
      case rocksdb::BackgroundErrorReason::kCompaction: {
        operation = "compaction";
        break;
      }
      case rocksdb::BackgroundErrorReason::kWriteCallback: {
        operation = "write callback";
        break;
      }
      case rocksdb::BackgroundErrorReason::kMemTable: {
        operation = "memtable";
        break;
      }
      case rocksdb::BackgroundErrorReason::kManifestWrite: {
        operation = "manifest write";
        break;
      }
      case rocksdb::BackgroundErrorReason::kManifestWriteNoWAL: {
        operation = "manifest write no WAL";
        break;
      }
      case rocksdb::BackgroundErrorReason::kFlushNoWAL: {
        operation = "flush no WAL";
        break;
      }
    }

    LOG_TOPIC("fae2c", ERR, Logger::ROCKSDB)
        << "RocksDB encountered a background error during a " << operation
        << " operation: "
        << (status != nullptr ? status->ToString() : "unknown error")
        << "; The database will be put in read-only mode, and subsequent write "
           "errors are likely. It is advised to shut down this instance, "
           "resolve the error offline and then restart it.";
  }
}

void RocksDBBackgroundErrorListener::OnErrorRecoveryCompleted(
    rocksdb::Status /* old_bg_error */) {
  _called.store(false, std::memory_order_relaxed);

  LOG_TOPIC("8ff56", WARN, Logger::ROCKSDB)
      << "RocksDB resuming operations after background error";
}

}  // namespace arangodb
